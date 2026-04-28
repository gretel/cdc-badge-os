---
title: "[LOW] No error boundary in I2C bus operations"
severity: LOW
domain: error-handling
lens: error-boundaries
labels:
  - "error-handling"
  - "hardware"
---

## Summary
I2C bus operations in `components/cdc_hal/src/I2cBus.cpp` return ESP-IDF status codes but don't provide error boundaries for higher-level callers. A single device communication failure can cascade if not handled properly by callers.

**Evidence:**
- `components/cdc_hal/src/I2cBus.cpp:128-150` (writeReg):
```cpp
esp_err_t I2cBusImpl::writeReg(I2cDeviceHandle handle, uint8_t reg,
                               const uint8_t* data, size_t len) {
    auto* dev = static_cast<I2cDevice*>(handle);
    if (!dev) return ESP_ERR_INVALID_ARG;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    if (data && len > 0) {
        i2c_master_write(cmd, data, len, true);
    }
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(dev->port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    return err;  // Error returned but no boundary for callers
}
```

- `components/cdc_hal/src/I2cBus.cpp:156-180` (readReg):
```cpp
esp_err_t I2cBusImpl::readReg(I2cDeviceHandle handle, uint8_t reg,
                              uint8_t* data, size_t len) {
    auto* dev = static_cast<I2cDevice*>(handle);
    if (!dev || !data || len == 0) return ESP_ERR_INVALID_ARG;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->addr << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(dev->port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    return err;  // Error returned but no boundary for callers
}
```

## Impact
- **Error propagation**: Callers must check return codes everywhere
- **Inconsistent handling**: Some callers may ignore errors
- **No retry logic**: Transient I2C errors not recovered

## Recommended Fix
Add wrapper methods with error boundaries and optional retry:

```cpp
class I2cBusImpl : public II2cBus {
public:
    // Existing methods
    esp_err_t writeReg(I2cDeviceHandle dev, uint8_t reg,
                       const uint8_t* data, size_t len) override;
    esp_err_t readReg(I2cDeviceHandle dev, uint8_t reg,
                      uint8_t* data, size_t len) override;

    // New wrapper with retry
    esp_err_t writeRegWithRetry(I2cDeviceHandle dev, uint8_t reg,
                                 const uint8_t* data, size_t len,
                                 uint8_t retries = 3) {
        for (uint8_t i = 0; i < retries; i++) {
            esp_err_t err = writeReg(dev, reg, data, len);
            if (err == ESP_OK) return ESP_OK;
            vTaskDelay(pdMS_TO_TICKS(10));  // Brief delay before retry
        }
        LOG_E(TAG, "writeReg failed after %d retries", retries);
        return ESP_FAIL;
    }

    esp_err_t readRegWithRetry(I2cDeviceHandle dev, uint8_t reg,
                                uint8_t* data, size_t len,
                                uint8_t retries = 3) {
        for (uint8_t i = 0; i < retries; i++) {
            esp_err_t err = readReg(dev, reg, data, len);
            if (err == ESP_OK) return ESP_OK;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        LOG_E(TAG, "readReg failed after %d retries", retries);
        return ESP_FAIL;
    }
};
```

## References
- [I2C Error Handling](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2c.html)
- [Hardware Retry Patterns](https://www.analog.com/en/technical-articles/i2c-error-handling.html)
