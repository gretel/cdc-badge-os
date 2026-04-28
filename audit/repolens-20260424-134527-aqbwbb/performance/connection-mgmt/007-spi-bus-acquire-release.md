---
title: "[MEDIUM] SPI bus acquire/release not symmetric on error paths"
severity: MEDIUM
domain: connection-mgmt
lens: performance/connection-mgmt
labels:
  - "audit:performance/connection-mgmt"
---

## Summary
In `components/cdc_hal/src/libtropic_port_esp32.cpp`, the SPI bus is acquired in `lt_port_spi_csn_low()` but released in `lt_port_spi_csn_high()`. This pattern assumes that every `csn_low()` call will be followed by a `csn_high()` call. However, if an error occurs between these calls (e.g., in `lt_port_spi_transfer()` or in the application logic), the SPI bus might be held indefinitely, blocking other devices sharing the same SPI bus.

**Location:** `components/cdc_hal/src/libtropic_port_esp32.cpp:72-103`

## Impact
1. **SPI bus deadlock**: If `lt_port_spi_csn_low()` acquires the bus but `lt_port_spi_csn_high()` is never called (due to error, exception, or early return), the SPI bus remains locked forever. This blocks:
   - Display updates (shares SPI2_HOST)
   - Other TROPIC01 operations
   - Any other SPI device on the shared bus

2. **Resource leak**: The SPI bus is a shared resource with limited availability. Holding it indefinitely prevents other components from accessing the bus.

3. **System hang**: Since `portMAX_DELAY` is used for acquisition, if another task holds the bus and waits for TROPIC01 (which is waiting for the bus), a circular wait condition could occur.

## Evidence
**File: `components/cdc_hal/src/libtropic_port_esp32.cpp`**

1. **SPI bus acquired in csn_low (line 72-89):**
```cpp
extern "C" lt_ret_t lt_port_spi_csn_low(lt_l2_state_t *s2) {
    if (!s2 || !s2->device) {
        return LT_PARAM_ERR;
    }
    lt_dev_esp32_t *device = static_cast<lt_dev_esp32_t *>(s2->device);

    // Acquire exclusive SPI bus access before asserting CS
    if (device->spi) {
        esp_err_t err = spi_device_acquire_bus(device->spi, portMAX_DELAY);  // ACQUIRE
        if (err != ESP_OK) {
            LOG_E(TAG, "Failed to acquire SPI bus: %d", err);
            return LT_L1_SPI_ERROR;
        }
    }

    gpio_set_level(device->cs_pin, 0);
    return LT_OK;
}
```

2. **SPI bus released in csn_high (line 91-103):**
```cpp
extern "C" lt_ret_t lt_port_spi_csn_high(lt_l2_state_t *s2) {
    if (!s2 || !s2->device) {
        return LT_PARAM_ERR;
    }
    lt_dev_esp32_t *device = static_cast<lt_dev_esp32_t *>(s2->device);
    gpio_set_level(device->cs_pin, 1);

    // Release SPI bus after deasserting CS
    if (device->spi) {
        spi_device_release_bus(device->spi);  // RELEASE
    }
    return LT_OK;
}
```

3. **Transfer function does NOT acquire/release (line 105-129):**
```cpp
extern "C" lt_ret_t lt_port_spi_transfer(lt_l2_state_t *s2, uint8_t offset, uint16_t tx_len,
                                         uint32_t timeout_ms) {
    (void)timeout_ms;
    if (!s2 || !s2->device) {
        return LT_PARAM_ERR;
    }
    lt_dev_esp32_t *device = static_cast<lt_dev_esp32_t *>(s2->device);
    if (!device->spi) {
        LOG_E(TAG, "SPI not initialized");
        return LT_L1_SPI_ERROR;
    }

    // In-place SPI transfer (TX/RX share buffer)
    spi_transaction_t t = {};
    t.length = static_cast<size_t>(tx_len) * 8;
    t.tx_buffer = s2->buff + offset;
    t.rx_buffer = s2->buff + offset;
    esp_err_t err = spi_device_polling_transmit(device->spi, &t);  // Uses already-acquired bus

    if (err != ESP_OK) {
        LOG_E(TAG, "SPI transmit failed: %d", err);
        return LT_L1_SPI_ERROR;  // Error path - but bus still held!
    }
    return LT_OK;
}
```

4. **Usage pattern in libtropic:**
The typical call sequence is:
```
csn_low()  <- Acquire SPI bus
transfer() <- SPI transfer (error here leaves bus held)
csn_high() <- Release SPI bus
```

If `transfer()` returns an error and the caller decides not to call `csn_high()`, the SPI bus remains locked.

## Recommended Fix
Add error handling to ensure SPI bus is released even on error paths. Two approaches:

### Approach 1: Add release on error in csn_low
```cpp
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

    gpio_set_level(device->cs_pin, 0);
    return LT_OK;
}

extern "C" lt_ret_t lt_port_spi_csn_high(lt_l2_state_t *s2) {
    if (!s2 || !s2->device) {
        return LT_PARAM_ERR;
    }
    lt_dev_esp32_t *device = static_cast<lt_dev_esp32_t *>(s2->device);
    
    // Always release SPI bus, even if CS was never asserted
    if (device->spi) {
        spi_device_release_bus(device->spi);
    }
    
    gpio_set_level(device->cs_pin, 1);
    return LT_OK;
}
```

### Approach 2: Add explicit release function for error handling
```cpp
extern "C" lt_ret_t lt_port_spi_csn_release(lt_l2_state_t *s2) {
    if (!s2 || !s2->device) {
        return LT_PARAM_ERR;
    }
    lt_dev_esp32_t *device = static_cast<lt_dev_esp32_t *>(s2->device);
    
    // Release SPI bus without toggling CS (for error recovery)
    if (device->spi) {
        spi_device_release_bus(device->spi);
    }
    
    return LT_OK;
}
```

### Approach 3: Document the contract clearly
Add a comment at the top of the file documenting the expected usage pattern:
```cpp
/**
 * SPI bus lifecycle for TROPIC01:
 * 
 * Typical sequence:
 *   lt_port_spi_csn_low()   <- Acquire SPI bus, assert CS
 *   lt_port_spi_transfer()  <- Perform transfer
 *   lt_port_spi_csn_high()  <- Release SPI bus, deassert CS
 * 
 * IMPORTANT: Every csn_low() MUST be followed by csn_high(), even on error.
 * Example error handling:
 *   lt_port_spi_csn_low(s2);
 *   lt_ret_t ret = lt_port_spi_transfer(s2, offset, len);
 *   lt_port_spi_csn_high(s2);  // Always call this!
 *   return ret;
 */
```

## References
- [ESP-IDF SPI Driver](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/spi_master.html) - SPI bus acquire/release
- [SPI Transaction Pattern](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/spi_master.html#transaction-pattern) - Proper transaction handling
- [Resource Acquisition Is Initialization (RAII)](https://en.wikipedia.org/wiki/Resource_acquisition_is_initialization) - Resource management pattern
