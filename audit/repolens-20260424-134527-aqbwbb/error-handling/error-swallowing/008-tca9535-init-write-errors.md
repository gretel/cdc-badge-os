---
title: "[MEDIUM] TCA9535 keypad initialization writes without error checking"
severity: MEDIUM
domain: cdc_hal
lens: error-handling
labels:
  - "TCA9535"
  - "I2C"
  - "initialization"
---

## Summary
In `components/cdc_hal/src/TCA9535Keypad.cpp`, four I2C register write operations during keypad initialization (lines 182-187) do not check return values, potentially masking hardware failures.

## Impact
If I2C communication fails during initialization, the keypad may be misconfigured but the driver reports success. This can lead to:
- Silent keypad failures that are difficult to diagnose
- Incorrect key presses or missed key events
- Boot-time failures that appear intermittent

## Evidence
```cpp
// Lines 182-187 in TCA9535Keypad.cpp
// Set output registers high (for proper pull-up reading)
bus_->writeReg(device_, REG_OUTPUT_0, &allHigh, 1);  // No error check!
bus_->writeReg(device_, REG_OUTPUT_1, &allHigh, 1);  // No error check!

// No polarity inversion
bus_->writeReg(device_, REG_POLARITY_0, &noInvert, 1);  // No error check!
bus_->writeReg(device_, REG_POLARITY_1, &noInvert, 1);  // No error check!
```

In contrast, lines 190-191 correctly check errors:
```cpp
if (bus_->writeReg(device_, REG_CONFIG_0, &allInputs, 1) != ESP_OK ||
    bus_->writeReg(device_, REG_CONFIG_1, &allInputs, 1) != ESP_OK) {
    LOG_E(TAG, "Failed to configure TCA9535");
    state_ = core::ServiceState::ERROR;
    return false;
}
```

## Recommended Fix
Add error checking to all four write operations and return failure if any write fails:

```cpp
// Set output registers high (for proper pull-up reading)
if (bus_->writeReg(device_, REG_OUTPUT_0, &allHigh, 1) != ESP_OK ||
    bus_->writeReg(device_, REG_OUTPUT_1, &allHigh, 1) != ESP_OK) {
    LOG_E(TAG, "Failed to set output registers");
    state_ = core::ServiceState::ERROR;
    return false;
}

// No polarity inversion
if (bus_->writeReg(device_, REG_POLARITY_0, &noInvert, 1) != ESP_OK ||
    bus_->writeReg(device_, REG_POLARITY_1, &noInvert, 1) != ESP_OK) {
    LOG_E(TAG, "Failed to set polarity registers");
    state_ = core::ServiceState::ERROR;
    return false;
}
```

## References
- ESP32 I2C driver documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2c.html
- Similar pattern already used correctly in lines 190-191 of same file
