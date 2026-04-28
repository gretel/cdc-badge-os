---
title: "[MEDIUM] Missing error handling for SPI bus acquisition in TROPIC01 port"
severity: MEDIUM
domain: hardware-abstraction
lens: error-handling
labels:
  - "audit:error-handling/unhandled-errors"
---

## Summary
In `components/cdc_hal/src/libtropic_port_esp32.cpp`, the `lt_port_spi_csn_high()` function calls `spi_device_release_bus()` without checking if the SPI device handle is valid. If `device->spi` is null (e.g., from partial initialization), this could cause a crash.

## Impact
If `lt_port_init()` partially succeeds (e.g., SPI bus initialized but device add fails), subsequent calls to `lt_port_spi_csn_high()` could:
- Dereference a null SPI device handle
- Cause a panic/crash in the SPI driver
- Leave the TROPIC01 in an undefined state

## Evidence
File: `components/cdc_hal/src/libtropic_port_esp32.cpp:91-103`

```cpp
extern "C" lt_ret_t lt_port_spi_csn_high(lt_l2_state_t *s2) {
    if (!s2 || !s2->device) {
        return LT_PARAM_ERR;
    }
    lt_dev_esp32_t *device = static_cast<lt_dev_esp32_t *>(s2->device);
    gpio_set_level(device->cs_pin, 1);

    // Release SPI bus after deasserting CS
    if (device->spi) {
        spi_device_release_bus(device->spi);  // <-- No error check
    }
    return LT_OK;
}
```

While there is a null check for `device->spi`, `spi_device_release_bus()` can still fail (e.g., if the bus was already released, or if called from wrong context). The error is silently ignored.

Also, looking at `lt_port_spi_csn_low()` at lines 72-89:
```cpp
if (device->spi) {
    esp_err_t err = spi_device_acquire_bus(device->spi, portMAX_DELAY);
    if (err != ESP_OK) {
        LOG_E(TAG, "Failed to acquire SPI bus: %d", err);
        return LT_L1_SPI_ERROR;
    }
}
```

This one properly checks the return value. But `lt_port_spi_csn_high()` doesn't.

## Recommended Fix
Add error logging for SPI release:

```cpp
extern "C" lt_ret_t lt_port_spi_csn_high(lt_l2_state_t *s2) {
    if (!s2 || !s2->device) {
        return LT_PARAM_ERR;
    }
    lt_dev_esp32_t *device = static_cast<lt_dev_esp32_t *>(s2->device);
    gpio_set_level(device->cs_pin, 1);

    // Release SPI bus after deasserting CS
    if (device->spi) {
        esp_err_t err = spi_device_release_bus(device->spi);
        if (err != ESP_OK) {
            LOG_W(TAG, "SPI release returned: %d", err);  // Warning, not error - may be expected
        }
    }
    return LT_OK;
}
```

Note: `spi_device_release_bus()` typically doesn't fail unless called incorrectly, so a warning log is appropriate rather than returning an error.

## References
- ESP-IDF SPI master driver: `spi_device_release_bus()` documentation
- libtropic porting guide: SPI bus synchronization
