---
title: "[MEDIUM] I2cBusImpl::addDevice() doesn't verify device presence"
severity: MEDIUM
domain: cdc_hal
lens: error-handling
labels:
  - "I2C"
  - "addDevice"
  - "device-detection"
---

## Summary
In `components/cdc_hal/src/I2cBus.cpp`, the `addDevice()` method (lines 104-117) only allocates a static device entry without verifying the device actually exists on the I2C bus.

## Impact
- Devices that don't exist are silently "added"
- Subsequent I2C operations fail but caller may not realize the root cause
- Hardware issues (wrong address, disconnected device) are masked

## Evidence
```cpp
// Lines 104-117 in I2cBus.cpp
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
    return ESP_OK;  // Returns success without verifying device!
}
```

Compare to `TCA9535Keypad::init()` (lines 166-173) which does verify:
```cpp
if (bus_->addDevice(EXPANDER_ADDR, &device_) != ESP_OK) {
    LOG_E(TAG, "Failed to add TCA9535 device");
    state_ = core::ServiceState::ERROR;
    return false;
}
// Then verifies chip ID
uint8_t vendor = 0;
if (!readReg(BQ_REG_VENDOR, &vendor)) {
    LOG_E(TAG, "Failed to read vendor register");
    // ...
}
```

## Recommended Fix
Add optional device verification after `addDevice()`:

```cpp
// Option 1: Add a verify parameter
esp_err_t addDevice(uint8_t addr, I2cDeviceHandle* out_dev, bool verify = false) {
    // ... existing allocation code ...
    
    if (verify) {
        uint8_t dummy;
        esp_err_t err = readReg(dev, 0, &dummy, 1);
        if (err != ESP_OK) {
            LOG_E(TAG, "%s: device at 0x%02X not found", name_, addr);
            deviceCount_--;  // Rollback
            return err;
        }
    }
    
    return ESP_OK;
}

// Option 2: Add a separate verifyDevice() method
esp_err_t verifyDevice(I2cDeviceHandle dev, uint8_t reg, uint8_t expected);
```

Usage:
```cpp
if (bus_->addDevice(EXPANDER_ADDR, &device_, true) != ESP_OK) {
    LOG_E(TAG, "Failed to add TCA9535 device");
    state_ = core::ServiceState::ERROR;
    return false;
}
```

## References
- I2C device detection best practices
- Similar verification pattern in `BQ25895Power::init()` (lines 214-229)
