---
title: "[004] [MEDIUM] Magic bitmasks without named constants"
severity: MEDIUM
domain: Code Readability
lens: code-quality/readability
labels:
  - "audit:code-quality/readability"
---

## Summary
Binary literals and bitmasks are used directly without named constants, making the code difficult to understand at a glance.

## Impact
- Developers must manually count bits to understand what each mask represents
- Increases likelihood of errors during modification
- Reduces self-documentation of the code

## Evidence
**File: `components/cdc_hal/src/TCA9535Keypad.cpp:48-58`**
```cpp
static Key rawToKey(uint16_t raw) {
    switch (raw & 0x0FFF) {
        case 0b111111111110: return Key::KEY_0;
        case 0b111111111101: return Key::KEY_1;
        case 0b111111111011: return Key::KEY_2;
        case 0b111111110111: return Key::KEY_3;
        case 0b111111101111: return Key::KEY_4;
        case 0b111111011111: return Key::KEY_5;
        case 0b111110111111: return Key::KEY_6;
        case 0b111101111111: return Key::KEY_7;
        case 0b111011111111: return Key::KEY_8;
        case 0b110111111111: return Key::KEY_9;
        case 0b011111111111: return Key::KEY_NO;    // Cancel/N
        case 0b101111111111: return Key::KEY_YES;   // OK/Y
        default: return Key::KEY_NONE;
    }
}
```

**File: `components/cdc_hal/src/TCA9535Keypad.cpp:66-79`**
```cpp
static uint16_t keyToMask(Key key) {
    switch (key) {
        case Key::KEY_0: return 0b111111111110;
        case Key::KEY_1: return 0b111111111101;
        // ... repeated binary literals
```

**File: `components/cdc_hal/src/TCA9535Keypad.cpp:313`**
```cpp
bool TCA9535Keypad::anyKeyDown() const {
    uint16_t current = const_cast<TCA9535Keypad*>(this)->readInputs();
    // 0x0FFF = all 12 keys released (bits 0-11 high, bits 12-15 don't care)
    return (current & 0x0FFF) != 0x0FFF;
}
```

While there is a comment on line 314, the binary literals in `rawToKey()` and `keyToMask()` have no explanation.

## Recommended Fix
Replace magic binary literals with named constants:

```cpp
// Named constants for 12-key keypad bitmask
static constexpr uint16_t KEY_MASK_ALL_RELEASED = 0b111111111111;
static constexpr uint16_t KEY_MASK_0 =         0b111111111110;
static constexpr uint16_t KEY_MASK_1 =         0b111111111101;
static constexpr uint16_t KEY_MASK_2 =         0b111111111011;
static constexpr uint16_t KEY_MASK_3 =         0b111111110111;
static constexpr uint16_t KEY_MASK_4 =         0b111111101111;
static constexpr uint16_t KEY_MASK_5 =         0b111111011111;
static constexpr uint16_t KEY_MASK_6 =         0b111110111111;
static constexpr uintly_t KEY_MASK_7 =         0b111101111111;
static constexpr uint16_t KEY_MASK_8 =         0b111011111111;
static constexpr uint16_t KEY_MASK_9 =         0b110111111111;
static constexpr uint16_t KEY_MASK_NO =        0b011111111111;
static constexpr uint16_t KEY_MASK_YES =       0b101111111111;

static constexpr uint16_t KEY_ALL_12_BITS = 0x0FFF;

static Key rawToKey(uint16_t raw) {
    switch (raw & KEY_ALL_12_BITS) {
        case KEY_MASK_0: return Key::KEY_0;
        case KEY_MASK_1: return Key::KEY_1;
        // ...
    }
}
```

## References
- C++ Core Guidelines: ES.46 - Avoid magic numbers
- Microsoft C++ Style: "Use named constants for bitmasks to improve readability"
