---
title: "[LOW] Missing debug logging for I2C read/write operations"
severity: LOW
domain: logging
lens: logging-coverage
labels:
  - "audit:observability/logging"
---

## Summary
The I2C bus implementation (`components/cdc_hal/src/I2cBus.cpp`) has missing debug logging for read/write operations. While the init and device registration are logged, individual I2C transactions don't produce log entries, making it difficult to diagnose intermittent I2C communication issues.

**Missing log entries:**

1. **`writeReg()` function** (lines 128-146): No logging for write operations
```cpp
esp_err_t I2cBusImpl::writeReg(I2cDeviceHandle handle, uint8_t reg,
                               const uint8_t* data, size_t len) {
    auto* dev = static_cast<I2cDevice*>(handle);
    if (!dev) return ESP_ERR_INVALID_ARG;
    
    // ... I2C command setup ...
    
    esp_err_t err = i2c_master_cmd_begin(dev->port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    
    return err;  // No LOG_D for success, no LOG_E for failure!
}
```

2. **`readReg()` function** (lines 156-180): No logging for read operations
```cpp
esp_err_t I2cBusImpl::readReg(I2cDeviceHandle handle, uint8_t reg,
                              uint8_t* data, size_t len) {
    auto* dev = static_cast<I2cDevice*>(handle);
    if (!dev || !data || len == 0) return ESP_ERR_INVALID_ARG;
    
    // ... I2C command setup ...
    
    esp_err_t err = i2c_master_cmd_begin(dev->port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    
    return err;  // No LOG_D for success, no LOG_E for failure!
}
```

## Impact
- **Debug difficulty**: When I2C devices fail to respond, there's no log showing which operation failed
- **Troubleshooting**: Hard to identify if I2C issues are timing-related, address-related, or device-related
- **Inconsistent with HAL patterns**: SPI bus has better logging (see `SpiBus.cpp`)

## Evidence
Compare with SPI bus which has error logging:
```cpp
// SpiBus.cpp - has error logging
bool SpiBus::transfer(uint8_t* tx, uint8_t* rx, size_t len) {
    esp_err_t err = spi_device_transmit(...);
    if (err != ESP_OK) {
        LOG_E(TAG, "SPI transfer failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}
```

I2C bus missing the same pattern:
```cpp
// I2cBus.cpp - no operation logging
esp_err_t I2cBusImpl::writeReg(I2cDeviceHandle handle, uint8_t reg,
                               const uint8_t* data, size_t len) {
    // ...
    esp_err_t err = i2c_master_cmd_begin(dev->port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    return err;  // Silent return
}
```

Note: The init logging is good (lines 82, 89, 95):
```cpp
LOG_E(TAG, "%s: i2c_param_config failed: %d", name_, err);
LOG_E(TAG, "%s: i2c_driver_install failed: %d", name_, err);
LOG_I(TAG, "%s initialized (SDA=%d, SCL=%d)", name_, sda_, scl_);
```

## Recommended Fix
Add debug-level logging to I2C operations:

1. **In `writeReg()` function** (around line 142):
```cpp
esp_err_t I2cBusImpl::writeReg(I2cDeviceHandle handle, uint8_t reg,
                               const uint8_t* data, size_t len) {
    auto* dev = static_cast<I2cDevice*>(handle);
    if (!dev) return ESP_ERR_INVALID_ARG;
    
    // ... I2C command setup ...
    
    esp_err_t err = i2c_master_cmd_begin(dev->port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    
    if (err != ESP_OK) {
        LOG_E(TAG, "%s: writeReg(0x%02X, %zu) failed: %s", 
              name_, dev->addr, len, esp_err_to_name(err));
    } else {
        LOG_D(TAG, "%s: wrote %zu bytes to 0x%02X", name_, len, dev->addr);
    }
    return err;
}
```

2. **In `readReg()` function** (around line 176):
```cpp
esp_err_t I2cBusImpl::readReg(I2cDeviceHandle handle, uint8_t reg,
                              uint8_t* data, size_t len) {
    auto* dev = static_cast<I2cDevice*>(handle);
    if (!dev || !data || len == 0) return ESP_ERR_INVALID_ARG;
    
    // ... I2C command setup ...
    
    esp_err_t err = i2c_master_cmd_begin(dev->port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    
    if (err != ESP_OK) {
        LOG_E(TAG, "%s: readReg(0x%02X, %zu) failed: %s", 
              name_, dev->addr, len, esp_err_to_name(err));
    } else {
        LOG_D(TAG, "%s: read %zu bytes from 0x%02X", name_, len, dev->addr);
    }
    return err;
}
```

## References
- `components/cdc_hal/src/I2cBus.cpp` - I2C bus implementation
- `components/cdc_hal/src/SpiBus.cpp` - SPI bus with better logging patterns
