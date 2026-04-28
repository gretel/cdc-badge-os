---
title: "[012] [LOW] Unclear boolean parameter in TCA9535Keypad::setLongPressEnabled"
severity: LOW
domain: code-readability
lens: code-quality/readability
labels:
  - "audit:code-quality/readability"
---

## Summary
In `components/cdc_hal/src/TCA9535Keypad.cpp:325-328`, the `setLongPressEnabled` function takes a boolean parameter followed by a threshold:

```cpp
void TCA9535Keypad::setLongPressEnabled(bool enabled, uint32_t thresholdMs) {
    longPressEnabled_ = enabled;
    longPressThresholdMs_ = thresholdMs;
}
```

At call sites, the boolean is not self-documenting:
```cpp
keypad->setLongPressEnabled(true, 800);  // What does 'true' mean here?
keypad->setLongPressEnabled(false, 800); // Does threshold still matter?
```

## Impact
- **Call site clarity**: `setLongPressEnabled(true, 800)` requires looking up the definition to understand what `true` means
- **API discoverability**: A developer reading code must jump to the function definition to understand the parameter

## Evidence
**File**: `components/cdc_hal/src/TCA9535Keypad.cpp:325-328`

**Declaration**: Line 107 in header
**Implementation**: Lines 325-328

The function sets two related properties (enable flag and threshold) but uses a boolean instead of separating concerns.

## Recommended Fix
Split into two focused methods:

```cpp
void TCA9535Keypad::enableLongPress(bool enabled) {
    longPressEnabled_ = enabled;
}

void TCA9535Keypad::setLongPressThreshold(uint32_t thresholdMs) {
    longPressThresholdMs_ = thresholdMs;
}

// Usage - much clearer
keypad->enableLongPress(true);
keypad->setLongPressThreshold(800);
```

Or use an options struct if the parameters are always set together:
```cpp
struct LongPressConfig {
    bool enabled = false;
    uint32_t thresholdMs = 800;
};

void TCA9535Keypad::setLongPressConfig(LongPressConfig config);

// Usage
keypad->setLongPressConfig({.enabled = true, .thresholdMs = 800});
```

## References
- [C++ Core Guidelines - F.24: Use a single-argument constructor to define implicit conversion](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#f24-use-a-single-argument-constructor-to-define-implicit-conversion)
- [Effective C++ - Item 33: Use explicit constructors to prevent implicit conversions](https://www.oreilly.com/library/view/effective-modern-c/9781491903995/)
