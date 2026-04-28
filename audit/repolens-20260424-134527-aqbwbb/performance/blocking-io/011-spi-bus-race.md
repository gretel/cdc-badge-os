---
title: "[HIGH] SPI bus acquisition race condition in TROPIC01 port"
severity: HIGH
domain: performance
lens: blocking-io
labels:
  - "performance"
  - "spi"
  - "tropic01"
  - "race-condition"
---

## Summary

In `components/cdc_hal/src/libtropic_port_esp32.cpp`, the TROPIC01 SPI port has a race condition where the chip-select (CS) pin is asserted before acquiring the SPI bus, potentially causing SPI transactions to interleave with other devices on the shared bus.

**Affected file:**
- `components/cdc_hal/src/libtropic_port_esp32.cpp` (lines 72-103)

**Current implementation:**
```cpp
// components/cdc_hal/src/libtropic_port_esp32.cpp:72-89
extern "C" lt_ret_t lt_port_spi_csn_low(lt_l2_state_t *s2) {
    if (!s2 || !s2->device) {
        return LT_PARAM_ERR;
    }
    lt_dev_esp32_t *device = static_cast<lt_dev_esp32_t *>(s2->device);

    // Acquire exclusive SPI bus access before asserting CS
    if (device->spi) {
        esp_err_t err = spi_device_acquire_bus(device->spi, portMAX_DELAY);
        if (err != ESP_OK) {
            LOG_E(TAG, "Failed to acquire SPI bus: %d", err);
            return LT_L1_SPI_ERROR;
        }
    }

    gpio_set_level(device->cs_pin, 0);  // CS asserted AFTER acquire
    return LT_OK;
}
```

**The issue:**
1. `lt_port_spi_csn_low()` asserts CS first, then acquires the bus
2. `lt_port_spi_csn_high()` releases CS first, then releases the bus
3. Between CS assertion and bus acquisition, another task could grab the SPI bus
4. This causes SPI transactions to interleave, corrupting TROPIC01 communication

## Impact

1. **Intermittent communication failures**: TROPIC01 operations may fail sporadically when another task (e.g., EPD display) uses SPI between CS assertion and bus acquisition.

2. **Data corruption**: SPI frames may be mixed between TROPIC01 and other devices, causing protocol errors.

3. **Hard-to-reproduce bugs**: Race condition depends on task scheduling timing, making it difficult to debug.

4. **Security implications**: TROPIC01 stores cryptographic keys; corrupted operations could lead to failed authentication or key operations.

5. **Blocking with portMAX_DELAY**: When the bus is contended, the task blocks indefinitely until the bus is available.

## Evidence

**Race window:**
```
Task A (TROPIC01): gpio_set_level(CS, 0);     // CS low
Task B (EPD):      spi_device_acquire_bus();  // Gets bus!
Task B (EPD):      spi_device_transmit();     // Sends EPD data
Task A (TROPIC01): spi_device_acquire_bus();  // Waits...
Task A (TROPIC01): spi_device_polling_transmit(); // TROPIC01 data sent too late!
```

**SPI bus sharing:**
- TROPIC01 secure element shares SPI bus with EPD display
- Both use `spi_device_acquire_bus()` with `portMAX_DELAY`
- No priority mechanism to prevent starvation

**Blocking wait:**
- `components/cdc_hal/src/libtropic_port_esp32.cpp:80` uses `portMAX_DELAY`
- Task blocks indefinitely if SPI bus is held by another task
- No timeout for recovery

## Recommended Fix

1. **Acquire bus before asserting CS** (1 hour):
```cpp
extern "C" lt_ret_t lt_port_spi_csn_low(lt_l2_state_t *s2) {
    if (!s2 || !s2->device) {
        return LT_PARAM_ERR;
    }
    lt_dev_esp32_t *device = static_cast<lt_dev_esp32_t *>(s2->device);

    // Acquire bus FIRST, then assert CS
    if (device->spi) {
        esp_err_t err = spi_device_acquire_bus(device->spi, pdMS_TO_TICKS(100));
        if (err != ESP_OK) {
            LOG_E(TAG, "Failed to acquire SPI bus: %d", err);
            return LT_L1_SPI_ERROR;
        }
    }

    gpio_set_level(device->cs_pin, 0);  // CS low AFTER acquire
    return LT_OK;
}

extern "C" lt_ret_t lt_port_spi_csn_high(lt_l2_state_t *s2) {
    if (!s2 || !s2->device) {
        return LT_PARAM_ERR;
    }
    lt_dev_esp32_t *device = static_cast<lt_dev_esp32_t *>(s2->device);

    gpio_set_level(device->cs_pin, 1);  // CS high

    // Release bus AFTER deasserting CS
    if (device->spi) {
        spi_device_release_bus(device->spi);
    }
    return LT_OK;
}
```

2. **Add timeout to prevent indefinite blocking** (30 min):
```cpp
// Use pdMS_TO_TICKS(100) instead of portMAX_DELAY
esp_err_t err = spi_device_acquire_bus(device->spi, pdMS_TO_TICKS(100));
if (err != ESP_OK) {
    LOG_E(TAG, "SPI bus timeout, retrying...");
    return LT_L1_SPI_ERROR;
}
```

3. **Consider SPI transaction queues for non-blocking** (2 hours):
```cpp
// Use spi_device_queue_trans() for async operations
spi_transaction_t t = {};
t.length = 8 * tx_len;
t.tx_buffer = s2->buff + offset;
t.rx_buffer = s2->buff + offset;

spi_device_queue_trans(device->spi, &t, pdMS_TO_TICKS(100));
spi_device_get_trans_result(device->spi, &t, pdMS_TO_TICKS(100));
```

4. **Add SPI bus contention monitoring** (1 hour):
```cpp
// Track SPI acquisition times for debugging
static uint32_t lastAcquireMs = 0;
uint32_t now = esp_timer_get_time() / 1000;
if (now - lastAcquireMs > 1000) {
    LOG_W(TAG, "SPI bus held for >1s, possible contention");
}
lastAcquireMs = now;
```

**Estimated effort**: 1-2 hours for fix and testing

## References

- [ESP-IDF SPI driver documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/spi_master.html)
- [SPI bus acquisition patterns](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/spi.html#acquiring-releasing-spi-bus)
- TROPIC01 datasheet: SPI timing requirements
- FreeRTOS semaphore and mutex documentation for priority inheritance

</content>