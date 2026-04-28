---
title: "[MEDIUM] Hardcoded keypad binary bitmasks"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "audit:code-quality/magic-values"
---

## Summary
Binary bitmasks (e.g., `0b111111111110`, `0b111111111101`) are used directly in `components/cdc_hal/src/TCA9535Keypad.cpp` for key-to-mask conversion. While the binary format makes them somewhat readable, these values should be documented with their bit positions and key mapping.

**Location:** `components/cdc_hal/src/TCA9535Keypad.cpp:45-57,69-81`

## Impact
- **Clarity**: Binary literals are readable but don't explain which bit corresponds to which key.
- **Maintainability**: If the hardware wiring changes, updating these values requires understanding the bit-to-key mapping.
- **Debugging**: Hard to verify correctness without a key-to-bit reference table.

## Evidence

**Lines 45-57 - rawToKey function:**
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

**Lines 69-81 - keyToMask function:**
```cpp
static uint16_t keyToMask(Key key) {
    switch (key) {
        case Key::KEY_0: return 0b111111111110;
        case Key::KEY_1: return 0b111111111101;
        // ... etc
```

Note: The bitmasks are duplicated in two functions, increasing maintenance burden.

## Recommended Fix

1. Define named constants for each key's bitmask with clear documentation:
```cpp
/** \brief Keypad bitmasks (active-low, bits 0-11). */
static constexpr uint16_t KEYBIT_0   = 0b111111111110;  // Bit 0
static constexpr uint16_t KEYBIT_1   = 0b111111111101;  // Bit 1
static constexpr uint16_t KEYBIT_2   = 0b111111111011;  // Bit 2
static constexpr uint16_t KEYBIT_3   = 0b111111110111;  // Bit 3
static constexpr uint16_t KEYBIT_4   = 0b111111101111;  // Bit 4
static constexpr uint16_t KEYBIT_5   = 0b111111011111;  // Bit 5
static constexpr uint16_t KEYBIT_6   = 0b111110111111;  // Bit 6
static constexpr uint16_t KEYBIT_7   = 0b111101111111;  // Bit 7
static constexpr uint16_t KEYBIT_8   = 0b111011111111;  // Bit 8
static constexpr uint16_t KEYBIT_9   = 0b110111111111;  // Bit 9
static constexpr uint16_t KEYBIT_NO  = 0b011111111111;  // Bit 10 (Cancel)
static constexpr uint16_t KEYBIT_YES = 0b101111111111;  // Bit 11 (OK)
```

2. Use these constants in both functions:
```cpp
static Key rawToKey(uint16_t raw) {
    switch (raw & 0x0FFF) {
        case KEYBIT_0: return Key::KEY_0;
        case KEYBIT_1: return Key::KEY_1;
        // ...
    }
}

static uint16_t keyToMask(Key key) {
    switch (key) {
        case Key::KEY_0: return KEYBIT_0;
        case Key::KEY_1: return KEYBIT_1;
        // ...
    }
}
```

3. Consider adding a helper function to generate bitmasks programmatically:
```cpp
static constexpr uint16_t KEYBIT(size_t position) {
    return static_cast<uint16_t>(~(1 << position));
}
```

## References
- `components/cdc_hal/src/TCA9535Keypad.cpp` - TCA9535 keypad implementation
- [TCA9535 Datasheet](https://www.ti.com/product/TCA9535)
