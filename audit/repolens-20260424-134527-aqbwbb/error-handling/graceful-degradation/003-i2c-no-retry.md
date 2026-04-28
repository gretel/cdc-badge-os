---
title: "[MEDIUM] I2C Bus Operations Lack Timeout and Retry for Transient Failures"
severity: MEDIUM
domain: hardware-abstraction
lens: graceful-degradation
labels:
  - "audit:error-handling/graceful-degradation"
---

## Summary
In `components/cdc_hal/src/I2cBus.cpp`, I2C read/write operations use a fixed 100ms timeout but no retry logic for transient failures (e.g., bus contention, chip wake-up latency). When operations fail, the caller receives an error with no fallback. This affects all I2C devices including the BQ25895 power manager and TCA9535 keypad, causing complete loss of functionality for those subsystems.

Lines of interest:
- `I2cBus.cpp:128-145` - `writeReg()` with single attempt
- `I2cBus.cpp:147-175` - `readReg()` with single attempt
- `BQ25895Power.cpp:140-150` - `readReg()` returns false on first failure
- `BQ25895Power.cpp:194-286` - Power manager init fails hard on I2C failure

## Impact
I2C bus failures cause:
1. **Power manager becomes unusable** - battery level, charging status, and power control lost
2. **Keypad stops working** - user input unavailable
3. **No transient error recovery** - e.g., if chip is slow to wake from sleep, all operations fail
4. **Cascading failures** - dependent modules (display, sleep manager) may also fail

## Evidence
```cpp
// I2cBus.cpp:128-145
esp_err_t I2cBusImpl::writeReg(I2cDeviceHandle handle, uint8_t reg,
                                const uint8_t* data, size_t len) {
    // ...
    esp_err_t err = i2c_master_cmd_begin(dev->port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    return err;  // Returns ESP_ERR_* on failure - no retry
}

// I2cBus.cpp:147-175
esp_err_t I2cBusImpl::readReg(I2cDeviceHandle handle, uint8_t reg,
                               uint8_t* data, size_t len) {
    // ...
    esp_err_t err = i2c_master_cmd_begin(dev->port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    return err;  // Returns ESP_ERR_* on failure - no retry
}

// BQ25895Power.cpp:140-150
bool BQ25895Power::readReg(uint8_t reg, uint8_t* value) const {
    if (!device_ || !value) return false;
    return bus_->readReg(device_, reg, value, 1) == ESP_OK;  // Single attempt
}

// BQ25895Power.cpp:219-229
bool BQ25895Power::init() {
    // ...
    uint8_t vendor = 0;
    if (!readReg(BQ_REG_VENDOR, &vendor)) {
        LOG_E(TAG, "Failed to read vendor register");
        state_ = core::ServiceState::ERROR;
        return false;  // Hard failure - no retry
    }
    // ...
}
```

## Recommended Fix
Implement I2C retry with exponential backoff:

1. **Add retry wrapper for I2C operations**:
   ```cpp
   // I2cBus.h
   esp_err_t readRegWithRetry(I2cDeviceHandle dev, uint8_t reg, uint8_t* data,
                               size_t len, uint8_t maxRetries = 3);
   ```

2. **Implement retry logic**:
   ```cpp
   // I2cBus.cpp
   esp_err_t I2cBusImpl::readRegWithRetry(I2cDeviceHandle handle, uint8_t reg,
                                           uint8_t* data, size_t len, uint8_t maxRetries) {
       esp_err_t err = ESP_FAIL;
       for (uint8_t i = 0; i < maxRetries; i++) {
           err = readReg(handle, reg, data, len);
           if (err == ESP_OK) return ESP_OK;
           
           // Exponential backoff: 10ms, 20ms, 40ms
           vTaskDelay(pdMS_TO_TICKS(10 * (1 << i)));
       }
       return err;
   }
   ```

3. **Use retry for critical operations**:
   ```cpp
   // BQ25895Power.cpp
   bool BQ25895Power::readReg(uint8_t reg, uint8_t* value) const {
       if (!device_ || !value) return false;
       return bus_->readRegWithRetry(device_, reg, value, 1, 3) == ESP_OK;
   }
   ```

4. **Add fallback for non-critical reads** - e.g., return cached battery voltage if read fails.

## References
- [ESP-IDF I2C Driver Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/i2c.html)
- [I2C Bus Error Handling Best Practices](https://www.nxp.com/docs/en/application-note/AN10668.pdf)
