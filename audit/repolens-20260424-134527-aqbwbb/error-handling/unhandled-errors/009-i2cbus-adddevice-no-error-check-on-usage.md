---
title: "[LOW] I2C bus addDevice() returns error but callers don't check in all paths"
severity: LOW
domain: error-handling
lens: unhandled-return-values
labels:
  - "audit:error-handling/unhandled-errors"
---

## Summary
In `I2cBus.cpp` at lines 104-116, `addDevice()` returns `ESP_ERR_NO_MEM` when device pool is full. This is checked in most callers but the error handling assumes the device handle is valid even after failure.

**Location:** `components/cdc_hal/src/I2cBus.cpp:104-116`
```cpp
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

## Evidence
**BQ25895Power.cpp:202-210** - Proper check:
```cpp
// Add BQ25895 to I2C bus
if (bus_->addDevice(BQ25895_ADDR, &device_) != ESP_OK) {
    LOG_E(TAG, "Failed to add BQ25895 to I2C0");
    state_ = core::ServiceState::ERROR;
    return false;
}
```

**TCA9535Keypad.cpp:167-173** - Proper check:
```cpp
// Add TCA9535 device
if (bus_->addDevice(EXPANDER_ADDR, &device_) != ESP_OK) {
    LOG_E(TAG, "Failed to add TCA9535 device");
    state_ = core::ServiceState::ERROR;
    return false;
}
```

**I2cBus.cpp:104-116** - Returns error but doesn't null out output param:
```cpp
esp_err_t I2cBusImpl::addDevice(uint8_t addr, I2cDeviceHandle* out_dev) {
    if (deviceCount_ >= MAX_DEVICES) {
        LOG_E(TAG, "%s: device pool full", name_);
        return ESP_ERR_NO_MEM;
        // BUG: out_dev not set to nullptr!
    }
    // ...
    *out_dev = dev;  // Only set on success
}
```

## Impact
- If `addDevice()` fails, `out_dev` may contain garbage (uninitialized memory)
- Callers check return value but don't verify `out_dev` is valid
- If error check is accidentally removed, code may crash on `device_` usage
- `MAX_DEVICES = 4` might be too small for future expansion

## Recommended Fix
Null out the output parameter on failure:

```cpp
esp_err_t I2cBusImpl::addDevice(uint8_t addr, I2cDeviceHandle* out_dev) {
    if (!out_dev) {
        return ESP_ERR_INVALID_ARG;
    }
    if (deviceCount_ >= MAX_DEVICES) {
        LOG_E(TAG, "%s: device pool full", name_);
        *out_dev = nullptr;  // Clear output param
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

Also add null check in callers as defense-in-depth:

```cpp
// In BQ25895Power::init()
if (bus_->addDevice(BQ25895_ADDR, &device_) != ESP_OK) {
    LOG_E(TAG, "Failed to add BQ25895 to I2C0");
    state_ = core::ServiceState::ERROR;
    return false;
}
if (!device_) {  // Defense-in-depth
    LOG_E(TAG, "BQ25895 device handle is null");
    state_ = core::ServiceState::ERROR;
    return false;
}
```

## References
- I2C bus implementation: `components/cdc_hal/src/I2cBus.cpp`
- Device pool size: `I2cBus.cpp:55` - `static constexpr size_t MAX_DEVICES = 4;`
