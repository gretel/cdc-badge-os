---
title: "[MEDIUM] Unchecked I2C write return values in TCA9535 keypad initialization"
severity: MEDIUM
domain: hardware-abstraction
lens: error-handling
labels:
  - "audit:error-handling/unhandled-errors"
---

## Summary
In `components/cdc_hal/src/TCA9535Keypad.cpp`, the `init()` function writes to TCA9535 registers but does not check return values for all `writeReg()` calls (lines 183-187). Only the final configuration write (lines 190-191) is checked, leaving early register writes unverified.

## Impact
If the I2C bus is noisy, the device is in reset, or there's a wiring issue, the first two `writeReg()` calls could fail silently:
- Output registers might not be set high (line 183-184)
- Polarity registers might not be configured (line 186-187)

This leads to incorrect keypad behavior (inverted readings, wrong pull-up state) that's hard to debug since initialization appears to succeed.

## Evidence
File: `components/cdc_hal/src/TCA9535Keypad.cpp:182-195`

```cpp
// Set output registers high (for proper pull-up reading)
bus_->writeReg(device_, REG_OUTPUT_0, &allHigh, 1);  // <-- UNCHECKED
bus_->writeReg(device_, REG_OUTPUT_1, &allHigh, 1);  // <-- UNCHECKED

// No polarity inversion
bus_->writeReg(device_, REG_POLARITY_0, &noInvert, 1);  // <-- UNCHECKED
bus_->writeReg(device_, REG_POLARITY_1, &noInvert, 1);  // UNCHECKED

// Configure all pins as inputs
if (bus_->writeReg(device_, REG_CONFIG_0, &allInputs, 1) != ESP_OK ||
    bus_->writeReg(device_, REG_CONFIG_1, &allInputs, 1) != ESP_OK) {
    LOG_E(TAG, "Failed to configure TCA9535");
    state_ = core::ServiceState::ERROR;
    return false;
}
```

## Recommended Fix
Add error checking to all `writeReg()` calls in the initialization sequence:

```cpp
// Set output registers high (for proper pull-up reading)
if (bus_->writeReg(device_, REG_OUTPUT_0, &allHigh, 1) != ESP_OK ||
    bus_->writeReg(device_, REG_OUTPUT_1, &allHigh, 1) != ESP_OK) {
    LOG_E(TAG, "Failed to set TCA9535 outputs");
    state_ = core::ServiceState::ERROR;
    return false;
}

// No polarity inversion
if (bus_->writeReg(device_, REG_POLARITY_0, &noInvert, 1) != ESP_OK ||
    bus_->writeReg(device_, REG_POLARITY_1, &noInvert, 1) != ESP_OK) {
    LOG_E(TAG, "Failed to set TCA9535 polarity");
    state_ = core::ServiceState::ERROR;
    return false;
}
```

## References
- ESP32-S3 Technical Reference Manual: I2C controller
- TCA9535 datasheet: Register initialization sequence
- C++ Core Guidelines: F.6 - Use `noexcept` for move constructors, F.7 for simple functions
