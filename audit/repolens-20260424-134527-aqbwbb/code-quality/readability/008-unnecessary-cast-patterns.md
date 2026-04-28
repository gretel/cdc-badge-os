---
title: "[008] [LOW] Repeated unnecessary const_cast patterns"
severity: LOW
domain: Code Readability
lens: code-quality/readability
labels:
  - "audit:code-quality/readability"
---

## Summary
The code uses `const_cast` repeatedly in simple getter methods to call non-const member functions, which adds visual noise without clear benefit.

## Impact
- Makes simple read operations look more complex than they are
- Requires readers to understand why the cast is needed
- Could be simplified by making the underlying method const

## Evidence
**File: `components/cdc_hal/src/TCA9535Keypad.cpp:286-292`**
```cpp
bool TCA9535Keypad::isKeyPressed(Key key) const {
    uint16_t mask = keyToMask(key);
    if (mask == 0xFFFF) return false;

    uint16_t current = const_cast<TCA9535Keypad*>(this)->readInputs();
    return (current & 0x0FFF) == mask;
}
```

**File: `components/cdc_hal/src/TCA9535Keypad.cpp:310-315`**
```cpp
bool TCA9535Keypad::anyKeyDown() const {
    uint16_t current = const_cast<TCA9535Keypad*>(this)->readInputs();
    // 0x0FFF = all 12 keys released (bits 0-11 high, bits 12-15 don't care)
    return (current & 0x0FFF) != 0x0FFF;
}
```

The `readInputs()` method is defined at line 269:
```cpp
uint16_t TCA9535Keypad::readInputs() {
    if (!device_) return 0xFFFF;
    uint8_t lo = 0xFF, hi = 0xFF;
    if (bus_->readReg(device_, REG_INPUT_0, &lo, 1) != ESP_OK) return 0xFFFF;
    if (bus_->readReg(device_, REG_INPUT_1, &hi, 1) != ESP_OK) return 0xFFFF;
    return (uint16_t)((hi << 8) | lo);
}
```

`readInputs()` doesn't modify any state - it only reads I2C registers.

## Recommended Fix
Make `readInputs()` const and remove the casts:

```cpp
// In class definition:
uint16_t readInputs() const;  // Add const

// In implementation:
uint16_t TCA9535Keypad::readInputs() const {
    if (!device_) return 0xFFFF;
    uint8_t lo = 0xFF, hi = 0xFF;
    if (bus_->readReg(device_, REG_INPUT_0, &lo, 1) != ESP_OK) return 0xFFFF;
    if (bus_->readReg(device_, REG_INPUT_1, &hi, 1) != ESP_OK) return 0xFFFF;
    return (uint16_t)((hi << 8) | lo);
}

// Then the cast-free usage:
bool TCA9535Keypad::isKeyPressed(Key key) const {
    uint16_t mask = keyToMask(key);
    if (mask == 0xFFFF) return false;
    uint16_t current = readInputs();  // No cast needed
    return (current & 0x0FFF) == mask;
}

bool TCA9535Keypad::anyKeyDown() const {
    uint16_t current = readInputs();  // No cast needed
    return (current & 0x0FFF) != 0x0FFF;
}
```

## References
- C++ Core Guidelines: C.4 - Make functions const if possible
- Effective C++: Item 3 - Make const member functions behave like non-const
