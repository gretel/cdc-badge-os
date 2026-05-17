---
title: "[MEDIUM] Hardcoded long-press threshold value (800ms)"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "audit:code-quality/magic-values"
---

## Summary
A magic value `800` (milliseconds) is used as the default long-press threshold in `components/cdc_hal/src/TCA9535Keypad.cpp:143`. The value is stored in a private member variable without a named constant, making it unclear what the rationale is for this specific duration.

**Location:** `components/cdc_hal/src/TCA9535Keypad.cpp:143`
```cpp
uint32_t longPressThresholdMs_ = 800;
```

## Impact
- **Maintainability**: Developers tuning the long-press behavior must search for the value or trial-and-error to find appropriate settings.
- **Consistency**: If other parts of the codebase need similar thresholds, they may use different values, leading to inconsistent UX.
- **Documentation**: No explanation exists for why 800ms was chosen over alternatives (e.g., 500ms, 1000ms).

## Evidence
The value appears in the class definition:
```cpp
// Line 143
uint32_t longPressThresholdMs_ = 800;
```

It is used in the long-press detection logic at line 425:
```cpp
if (now - self->pressStartTime_ >= self->longPressThresholdMs_) {
```

The setter method at line 328 allows runtime configuration but provides no guidance on reasonable values:
```cpp
void TCA9535Keypad::setLongPressEnabled(bool enabled, uint32_t thresholdMs) {
    longPressEnabled_ = enabled;
    longPressThresholdMs_ = thresholdMs;
}
```

## Recommended Fix
1. Define a named constant for the default long-press threshold near other keypad constants (lines 30-37):
```cpp
/** \brief Default long-press detection threshold in milliseconds. */
static constexpr uint32_t DEFAULT_LONG_PRESS_MS = 800;
```

2. Update the member initialization to use the constant:
```cpp
uint32_t longPressThresholdMs_ = DEFAULT_LONG_PRESS_MS;
```

3. Add a comment explaining the rationale (e.g., "Long enough to avoid accidental triggers, short enough for responsive UI").

## References
- [Magic Numbers (Code Quality Best Practices)](https://en.wikipedia.org/wiki/Magic_number_(programming))
- ESP32-S3 Keypad HAL implementation: `components/cdc_hal/src/TCA9535Keypad.cpp`
