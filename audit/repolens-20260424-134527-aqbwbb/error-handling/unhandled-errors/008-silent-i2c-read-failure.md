---
title: "[MEDIUM] Unchecked return values from I2C read operations in TCA9535 keypad"
severity: MEDIUM
domain: hardware-abstraction
lens: error-handling
labels:
  - "audit:error-handling/unhandled-errors"
---

## Summary
In `components/cdc_hal/src/TCA9535Keypad.cpp`, the `readInputs()` function reads from I2C registers but returns a sentinel value (0xFFFF) on failure without logging the error. This makes it hard to distinguish between "no keys pressed" and "I2C communication failure".

## Impact
When I2C communication fails:
- Returns 0xFFFF which maps to `KEY_NONE` 
- No error is logged, so the failure is silent
- Keypad appears to work but actually has no input
- Hard to debug intermittent I2C issues

## Evidence
File: `components/cdc_hal/src/TCA9535Keypad.cpp:271-279`

```cpp
uint16_t TCA9535Keypad::readInputs() {
    if (!device_) return 0xFFFF;

    uint8_t lo = 0xFF, hi = 0xFF;
    if (bus_->readReg(device_, REG_INPUT_0, &lo, 1) != ESP_OK) return 0xFFFF;  // <-- Silent failure
    if (bus_->readReg(device_, REG_INPUT_1, &hi, 1) != ESP_OK) return 0xFFFF;  // <-- Silent failure

    return (uint16_t)((hi << 8) | lo);
}
```

The function returns 0xFFFF on any I2C error, but this is indistinguishable from all keys being released (which also produces 0xFFFF in normal operation).

## Recommended Fix
Add error logging and track consecutive failures:

```cpp
static constexpr uint8_t MAX_I2C_FAILURES = 5;
static uint8_t s_i2cFailureCount = 0;

uint16_t TCA9535Keypad::readInputs() {
    if (!device_) {
        return 0xFFFF;
    }

    uint8_t lo = 0xFF, hi = 0xFF;
    esp_err_t err;
    
    err = bus_->readReg(device_, REG_INPUT_0, &lo, 1);
    if (err != ESP_OK) {
        s_i2cFailureCount++;
        if (s_i2cFailureCount >= MAX_I2C_FAILURES) {
            LOG_E(TAG, "I2C read failed: %s (count: %u)", esp_err_to_name(err), s_i2cFailureCount);
        }
        return 0xFFFF;
    }
    
    err = bus_->readReg(device_, REG_INPUT_1, &hi, 1);
    if (err != ESP_OK) {
        s_i2cFailureCount++;
        if (s_i2cFailureCount >= MAX_I2C_FAILURES) {
            LOG_E(TAG, "I2C read failed: %s (count: %u)", esp_err_to_name(err), s_i2cFailureCount);
        }
        return 0xFFFF;
    }
    
    s_i2cFailureCount = 0;  // Reset on success
    return (uint16_t)((hi << 8) | lo);
}
```

Alternatively, use a different sentinel value or add a separate "read failures" counter that can be checked by the task.

## References
- TCA9535 datasheet: Input register layout
- I2C error handling best practices
