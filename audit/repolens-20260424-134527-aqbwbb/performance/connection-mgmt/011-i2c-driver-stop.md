---
title: "[LOW] I2C bus driver never stopped in stop() method"
severity: LOW
domain: connection-mgmt
lens: performance/connection-mgmt
labels:
  - "audit:performance/connection-mgmt"
---

## Summary

In `components/cdc_hal/src/I2cBus.cpp`, the `stop()` method is empty and doesn't call `i2c_driver_stop()` to properly stop the I2C driver. The `init()` method calls `i2c_driver_install()` to initialize the I2C driver, but there's no corresponding cleanup. This means the I2C driver remains active even after the service is stopped.

**Location:** `components/cdc_hal/src/I2cBus.cpp:38`

## Impact

1. **Resource leak**: The I2C driver remains initialized, keeping the I2C peripheral active and consuming power.

2. **GPIO state**: The SDA and SCL pins remain configured for I2C even after the service is stopped.

3. **Re-initialization issues**: If `init()` is called again after `stop()`, the driver might already be installed, potentially causing conflicts.

4. **Power consumption**: The I2C peripheral stays active, preventing the system from entering lowest power modes.

## Evidence

**File: `components/cdc_hal/src/I2cBus.cpp`**

1. **Empty stop() method (line 38-40):**
```cpp
bool I2cBusImpl::stop() {
    state_ = core::ServiceState::STOPPED;
    // No i2c_driver_stop() call!
    return true;
}
```

2. **init() installs driver (line 23-36):**
```cpp
bool I2cBusImpl::init() {
    if (state_ == core::ServiceState::RUNNING) return true;

    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = sda_;
    conf.scl_io_num = scl_;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_FREQ_HZ;

    esp_err_t err = i2c_param_config(port_, &conf);
    if (err == ESP_OK) {
        err = i2c_driver_install(port_, conf.mode, 0, 0, 0);  // Driver installed
    }

    if (err == ESP_OK) {
        state_ = core::ServiceState::INITIALIZED;
        LOG_I(TAG, "%s initialized (SDA=%d, SCL=%d)", name_, sda_, scl_);
    } else {
        LOG_E(TAG, "I2C init failed: %s", esp_err_to_name(err));
    }

    return err == ESP_OK;
}
```

3. **ESP-IDF API requires matching calls:**
- `i2c_driver_install()` creates the driver
- `i2c_driver_stop()` should be called to stop and free the driver

## Recommended Fix

Add `i2c_driver_stop()` call to the `stop()` method:

1. **Update stop() method:**
```cpp
bool I2cBusImpl::stop() {
    if (state_ == core::ServiceState::RUNNING || 
        state_ == core::ServiceState::INITIALIZED) {
        i2c_driver_stop(port_);  // Stop the driver
    }
    state_ = core::ServiceState::STOPPED;
    LOG_I(TAG, "%s stopped", name_);
    return true;
}
```

2. **Add guard to init() for re-initialization:**
```cpp
bool I2cBusImpl::init() {
    if (state_ == core::ServiceState::RUNNING) return true;
    
    // If previously stopped, ensure driver is clean
    if (state_ == core::ServiceState::STOPPED) {
        i2c_driver_stop(port_);  // Clean up any stale state
    }

    // ... rest of init ...
}
```

## References

- [ESP-IDF I2C Driver API](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/i2c.html) - i2c_driver_install and i2c_driver_stop
- [I2C Peripheral Lifecycle](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/i2c.html#overview) - Proper initialization and cleanup

</content>