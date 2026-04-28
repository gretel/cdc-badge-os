---
title: "[MEDIUM] No I2C bus device health monitoring"
severity: MEDIUM
domain: observability
lens: health-monitoring
labels:
  - "audit:observability/health-monitoring"
---

## Summary

The I2C bus implementation (`components/cdc_hal/src/I2cBus.cpp`) has no mechanism to monitor or report the health status of connected devices. While devices are added during initialization, there's no way to:
- Verify devices are still responding
- Check device connectivity status
- Report which devices are healthy vs. failed

**Current implementation:**
```cpp
// I2cBus.cpp:105-119
esp_err_t I2cBusImpl::addDevice(uint8_t addr, I2cDeviceHandle* out_dev) {
    if (deviceCount_ >= MAX_DEVICES) {
        LOG_E(TAG, "%s: device pool full", name_);
        return ESP_ERR_NO_MEM;
    }

    I2cDevice* dev = &devices_[deviceCount_++];
    dev->port = port_;
    dev->addr = addr;
    *out_dev = dev;

    LOG_I(TAG, "%s: added device at 0x%02X", name_, addr);
    return ESP_OK;
}
```

The device is registered but no health tracking is maintained.

## Impact

- **Silent device failures**: If an I2C device stops responding (e.g., TCA9535 keypad, BQ25895 power manager), the failure is only discovered when a read/write operation fails
- **No proactive monitoring**: Cannot check device health before operations fail
- **Debugging difficulty**: Hard to diagnose intermittent I2C bus issues
- **No health summary**: STATUS command cannot report I2C device health

## Evidence

**File:** `components/cdc_hal/include/cdc_hal/II2cBus.h:15-41` - Interface defines `addDevice()`, `writeReg()`, `readReg()` but no health check methods

**File:** `components/cdc_hal/src/I2cBus.cpp` - Device pool stores `addr` and `port` but no status/health field

**File:** `main/main.cpp:107-113` - I2C init only logs success/failure, no ongoing health tracking:
```cpp
if (s_i2cBus && s_i2cBus->init()) {
    LOG_I(TAG, "I2C bus ready");
} else {
    LOG_E(TAG, "I2C bus init failed!");
}
```

## Recommended Fix

Add I2C device health monitoring:

1. **Add health status to I2cDevice structure:**
   ```cpp
   struct I2cDevice {
       i2c_port_t port;
       uint8_t addr;
       uint32_t lastSuccessMs;   // Last successful operation
       uint8_t failureCount;      // Consecutive failures
       bool isHealthy() const { return failureCount < 3; }
   };
   ```

2. **Add health check method to II2cBus interface:**
   ```cpp
   class II2cBus : public core::IService {
       virtual esp_err_t probeDevice(I2cDeviceHandle dev) = 0;
       virtual bool isDeviceHealthy(I2cDeviceHandle dev) = 0;
       virtual void getDeviceStatus(I2cDeviceHandle dev, uint8_t* out_addr, uint32_t* out_lastSuccess) = 0;
   };
   ```

3. **Update read/write to track health:**
   ```cpp
   esp_err_t I2cBusImpl::readReg(I2cDeviceHandle dev, uint8_t reg, uint8_t* data, size_t len) {
       esp_err_t err = i2c_master_transmit(...);
       if (err == ESP_OK) {
           dev->lastSuccessMs = esp_timer_get_time() / 1000;
           dev->failureCount = 0;
       } else {
           dev->failureCount++;
       }
       return err;
   }
   ```

4. **Add I2C_STATUS command:**
   ```
   I2C_STATUS - Show health status of all I2C devices
   ```

## References

- I2C bus troubleshooting: https://www.espressif.com/sites/default/files/documentation/esp32-i2c-design-guide.pdf
- ESP32 I2C driver: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2c.html
