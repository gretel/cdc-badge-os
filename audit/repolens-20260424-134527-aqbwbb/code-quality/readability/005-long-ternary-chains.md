---
title: "[005] [MEDIUM] Long ternary chains instead of lookup tables"
severity: MEDIUM
domain: Code Readability
lens: code-quality/readability
labels:
  - "audit:code-quality/readability"
---

## Summary
Multiple functions use long chains of ternary operators or switch statements that could be replaced with lookup tables for better readability.

## Impact
- Each case must be manually read and understood
- Harder to verify correctness (off-by-one errors)
- More verbose than a data-driven approach
- Adding new cases requires modifying logic code instead of just data

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
        case 0b011111111111: return Key::KEY_NO;
        case 0b101111111111: return Key::KEY_YES;
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
        case Key::KEY_2: return 0b111111111011;
        case Key::KEY_3: return 0b111111110111;
        case Key::KEY_4: return 0b111111101111;
        case Key::KEY_5: return 0b111111011111;
        case Key::KEY_6: return 0b111110111111;
        case Key::KEY_7: return 0b111101111111;
        case Key::KEY_8: return 0b111011111111;
        case Key::KEY_9: return 0b110111111111;
        case Key::KEY_NO: return 0b011111111111;
        case Key::KEY_YES: return 0b101111111111;
        default: return 0xFFFF;
    }
}
```

**File: `components/cdc_hal/src/BleAdvParser.cpp:19-23`**
```cpp
static constexpr uint8_t AD_TYPE_SHORTENED_NAME            = 0x08;
static constexpr uint8_t AD_TYPE_COMPLETE_NAME             = 0x09;
static constexpr uint8_t AD_TYPE_INCOMPLETE_UUID128        = 0x06;
static constexpr uint8_t AD_TYPE_COMPLETE_UUID128          = 0x07;
static constexpr uint8_t AD_TYPE_MANUFACTURER_SPECIFIC     = 0xFF;
```
These are well-documented, but the pattern shows there are many AD types that could benefit from a table structure.

## Recommended Fix
Replace the switch statements with lookup tables:

```cpp
// Lookup table for raw bitmask to Key conversion
struct KeyMapping {
    uint16_t mask;
    Key key;
};

static constexpr KeyMapping KEY_LOOKUP[] = {
    {0b111111111110, Key::KEY_0},
    {0b111111111101, Key::KEY_1},
    {0b111111111011, Key::KEY_2},
    {0b111111110111, Key::KEY_3},
    {0b111111101111, Key::KEY_4},
    {0b111111011111, Key::KEY_5},
    {0b111110111111, Key::KEY_6},
    {0b111101111111, Key::KEY_7},
    {0b111011111111, Key::KEY_8},
    {0b110111111111, Key::KEY_9},
    {0b011111111111, Key::KEY_NO},
    {0b101111111111, Key::KEY_YES},
};

static Key rawToKey(uint16_t raw) {
    uint16_t mask = raw & 0x0FFF;
    for (const auto& mapping : KEY_LOOKUP) {
        if (mask == mapping.mask) {
            return mapping.key;
        }
    }
    return Key::KEY_NONE;
}

// Reverse lookup: Key to bitmask
static constexpr KeyMapping KEY_TO_MASK_LOOKUP[] = {
    {Key::KEY_0, 0b111111111110},
    {Key::KEY_1, 0b111111111101},
    // ... same pattern
};

static uint16_t keyToMask(Key key) {
    for (const auto& mapping : KEY_TO_MASK_LOOKUP) {
        if (key == mapping.key) {
            return mapping.mask;
        }
    }
    return 0xFFFF;
}
```

Alternatively, use a compile-time hash or direct index calculation if the mapping is dense enough.

## References
- Refactoring: "Replace Conditional with Table" - Martin Fowler
- C++ Core Guidelines: I.17 - Use constexpr for lookup tables
