---
title: "[006] [LOW] Redundant boolean returns in TCA9535Keypad::init() and similar functions"
severity: LOW
domain: code-readability
lens: code-quality/readability
labels:
  - "audit:code-quality/readability"
---

## Summary
In `components/cdc_hal/src/TCA9535Keypad.cpp:151-156`, the `init()` function uses a verbose return pattern:

```cpp
bool TCA9535Keypad::init() {
    if (state_ != core::ServiceState::UNINITIALIZED) {
        return state_ == core::ServiceState::INITIALIZED ||
               state_ == core::ServiceState::STARTED;
    }
    // ...
}
```

The return expression `state_ == core::ServiceState::INITIALIZED || state_ == core::ServiceState::STARTED` is a boolean expression that could be returned directly without wrapping.

## Impact
- **Visual noise**: The extra comparison creates unnecessary indentation
- **Cognitive overhead**: Reader must mentally evaluate the boolean expression

## Evidence
**File**: `components/cdc_hal/src/TCA9535Keypad.cpp:151-156`

This pattern appears in multiple files:
- `TCA9535Keypad::init()` (line 151-156)
- `BQ25895Power::init()` (line 194-199)
- `Esp32SleepController::init()` (line 88-93)
- `Esp32Rtc::init()` (line 66-71)
- `I2cBusImpl::init()` (line 64-69)

## Recommended Fix
Simplify the return expression:

```cpp
bool TCA9535Keypad::init() {
    if (state_ != core::ServiceState::UNINITIALIZED) {
        return state_ == core::ServiceState::INITIALIZED ||
               state_ == core::ServiceState::STARTED;
    }
    // ...
}
```

Actually, this is already reasonably clear. A better improvement would be to use an enum method or helper:

```cpp
// Add to ServiceState enum or as a helper
static bool isInitializedOrStarted(core::ServiceState state) {
    return state == core::ServiceState::INITIALIZED ||
           state == core::ServiceState::STARTED;
}

// Usage
bool TCA9535Keypad::init() {
    if (state_ != core::ServiceState::UNINITIALIZED) {
        return isInitializedOrStarted(state_);
    }
}
```

Or even simpler with a switch:
```cpp
bool TCA9535Keypad::init() {
    switch (state_) {
        case core::ServiceState::INITIALIZED:
        case core::ServiceState::STARTED:
            return true;
        case core::ServiceState::UNINITIALIZED:
            break;  // Continue with init
        default:
            return false;
    }
}
```

## References
- [C++ Core Guidelines - RES.1: Write clear code](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#res1-write-clear-code)
