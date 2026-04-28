---
title: "[LOW] I2C read/write errors not checked in TCA9535Keypad::readInputs()"
severity: LOW
domain: error-handling
lens: unhandled-return-values
labels:
  - "audit:error-handling/unhandled-errors"
---

## Summary
In `TCA9535Keypad.cpp` at lines 268-276, the `readInputs()` method calls `bus_->readReg()` but only checks for `ESP_OK` return - it returns `0xFFFF` on any error. This is reasonable but the error is not logged, making debugging difficult.

**Location:** `components/cdc_hal/src/TCA9535Keypad.cpp:268-276`
```cpp
uint16_t TCA9535Keypad::readInputs() {
    if (!device_) return 0xFFFF;

    uint8_t lo = 0xFF, hi = 0xFF;
    if (bus_->readReg(device_, REG_INPUT_0, &lo, 1) != ESP_OK) return 0xFFFF;  // Error not logged
    if (bus_->readReg(device_, REG_INPUT_1, &hi, 1) != ESP_OK) return 0xFFFF;  // Error not logged

    return (uint16_t)((hi << 8) | lo);
}
```

## Impact
- I2C bus errors (device not responding, bus contention, etc.) are silently masked
- Keypad task will see `0xFFFF` and may misinterpret as "all keys released"
- Debugging hardware issues requires adding manual logging
- Similar pattern in `BQ25895Power::readReg()` and `writeReg()` also doesn't log individual failures

## Evidence
**TCA9535Keypad.cpp:268-276**
```cpp
uint16_t TCA9535Keypad::readInputs() {
    if (!device_) return 0xFFFF;

    uint8_t lo = 0xFF, hi = 0xFF;
    if (bus_->readReg(device_, REG_INPUT_0, &lo, 1) != ESP_OK) return 0xFFFF;
    if (bus_->readReg(device_, REG_INPUT_1, &hi, 1) != ESP_OK) return 0xFFFF;

    return (uint16_t)((hi << 8) | lo);
}
```

**BQ25895Power.cpp:131-156** - Similar pattern:
```cpp
bool BQ25895Power::readReg(uint8_t reg, uint8_t* value) const {
    if (!device_ || !value) return false;
    return bus_->readReg(device_, reg, value, 1) == ESP_OK;  // Error not logged
}

bool BQ25895Power::writeReg(uint8_t reg, uint8_t value) {
    if (!device_) return false;
    return bus_->writeReg(device_, reg, &value, 1) == ESP_OK;  // Error not logged
}
```

**EpaperDisplay.cpp:206-214** - Similar pattern for SPI:
```cpp
// LAZY create display objects
if (!s_epd_spi) {
    s_epd_spi = new EpdSpi();
}
if (!s_epd_display) {
    s_epd_display = new Gdey029T94(*s_epd_spi);
}
// No error checking on constructor!
```

## Recommended Fix
Add debug-level logging for I2C errors to aid debugging:

```cpp
uint16_t TCA9535Keypad::readInputs() {
    if (!device_) {
        LOG_D(TAG, "No device handle");
        return 0xFFFF;
    }

    uint8_t lo = 0xFF, hi = 0xFF;
    esp_err_t err = bus_->readReg(device_, REG_INPUT_0, &lo, 1);
    if (err != ESP_OK) {
        LOG_D(TAG, "Read REG_INPUT_0 failed: %s", esp_err_to_name(err));
        return 0xFFFF;
    }
    err = bus_->readReg(device_, REG_INPUT_1, &hi, 1);
    if (err != ESP_OK) {
        LOG_D(TAG, "Read REG_INPUT_1 failed: %s", esp_err_to_name(err));
        return 0xFFFF;
    }

    return (uint16_t)((hi << 8) | lo);
}
```

Alternatively, add periodic error counting and log summary:

```cpp
// Add to class:
mutable int errorCount_ = 0;

// In readInputs():
if (bus_->readReg(device_, REG_INPUT_0, &lo, 1) != ESP_OK) {
    errorCount_++;
    if (errorCount_ % 10 == 0) {  // Log every 10 errors
        LOG_W(TAG, "I2C read errors: %d", errorCount_);
    }
    return 0xFFFF;
}
```

## References
- I2C bus implementation: `components/cdc_hal/src/I2cBus.cpp`
- ESP-IDF I2C driver: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/i2c.html
