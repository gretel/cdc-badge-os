---
title: "[002] [MEDIUM] Verbose boolean parameters without descriptive names"
severity: MEDIUM
domain: Code Readability
lens: code-quality/readability
labels:
  - "audit:code-quality/readability"
---

## Summary
Multiple functions use bare boolean parameters that make call sites difficult to understand without jumping to the function definition.

## Impact
- Call sites become cryptic: `setLongPressEnabled(true, 800)` requires looking up what `true` means
- Increases cognitive load for developers reading the code
- Makes refactoring harder as parameter order changes can silently break semantics

## Evidence
**File: `components/cdc_hal/src/TCA9535Keypad.cpp:322`**
```cpp
void TCA9535Keypad::setLongPressEnabled(bool enabled, uint32_t thresholdMs) {
    longPressEnabled_ = enabled;
    longPressThresholdMs_ = thresholdMs;
}
```

Call site at line 322:
```cpp
void TCA9535Keypad::setLongPressEnabled(bool enabled, uint32_t thresholdMs) {
    longPressEnabled_ = enabled;
    longPressThresholdMs_ = thresholdMs;
}
```

**File: `components/cdc_hal/src/BQ25895Power.cpp:523`**
```cpp
void BQ25895Power::setChargingEnabled(bool enabled) {
    if (enabled) {
        setChargeCurrentMa(fastChargeEnabled_ ? CHARGE_CURRENT_FAST : CHARGE_CURRENT_SLOW);
    } else {
        // Set minimum current to effectively disable
        setChargeCurrentMa(CHARGE_CURRENT_MIN);
    }
    LOG_I(TAG, "Charging %s", enabled ? "enabled" : "disabled");
}
```

Call site at line 523 uses bare `true/false`:
```cpp
void BQ25895Power::setChargingEnabled(bool enabled) { ... }
```

**File: `components/cdc_ui/src/I18n.cpp:58`**
```cpp
void I18n::setLanguage(Language lang) {
    if (lang >= Language::COUNT) {
        lang = Language::EN;  // Magic fallback without explanation
    }
    ...
}
```

## Recommended Fix
Replace boolean parameters with more descriptive alternatives:

1. **For `setLongPressEnabled(bool enabled, uint32_t thresholdMs)`**:
   - Consider using an enum or named struct for the parameters
   - Or split into two clearer methods: `enableLongPress(uint32_t thresholdMs)` and `disableLongPress()`

2. **For `setChargingEnabled(bool enabled)`**:
   - Rename to `setCharging(bool enable)` (slightly clearer)
   - Or provide convenience methods: `enableCharging()`, `disableCharging()`

3. **For the I18n fallback**:
   - Add a comment explaining why `Language::EN` is chosen as fallback

## References
- Clean Code: "Boolean parameters are often confusing" - Robert C. Martin
- C++ Core Guidelines: F.23 - Use a bool parameter only for a simple condition
