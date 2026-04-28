---
title: "[019] [LOW] Bit-packing macros without documentation in TCA9535Keypad"
severity: LOW
domain: code-readability
lens: code-quality/readability
labels:
  - "audit:code-quality/readability"
---

## Summary
In `components/cdc_hal/src/TCA9535Keypad.cpp:46-79`, the `rawToKey` and `keyToMask` functions use binary literals for bit patterns without explaining what each bit represents:

```cpp
static Key rawToKey(uint16_t raw) {
    switch (raw & 0x0FFF) {
        case 0b111111111110: return Key::KEY_0;
        case 0b111111111101: return Key::KEY_1;
        case 0b111111111011: return Key::KEY_2;
        // ...
    }
}

static uint16_t keyToMask(Key key) {
    switch (key) {
        case Key::KEY_0: return 0b111111111110;
        case Key::KEY_1: return 0b111111111101;
        // ...
    }
}
```

## Impact
- **Hardware coupling**: The bit patterns correspond to a specific keypad matrix wiring but this is not documented
- **Maintenance risk**: A developer cannot easily verify which bit corresponds to which key without tracing the hardware schematic
- **Onboarding friction**: New developers must ask "which bit is which key?"

## Evidence
**File**: `components/cdc_hal/src/TCA9535Keypad.cpp:46-79`

The binary literals show which bit is low for each key, but don't explain:
- Which physical key position corresponds to which bit
- Why the patterns are active-low
- How the 12 keys map to the TCA9535's 16 GPIO pins

## Recommended Fix
Add a diagram comment and named constants:

```cpp
/**
 * \brief Keypad matrix bit mapping (active-low, 12 keys on TCA9535 port 0).
 *
 * Bit layout (port 0, bits 0-11):
 *
 *        |  Bit 11  |  Bit 10  |  Bit 9   |  Bit 8   |  Bit 7   |  Bit 6   |
 *        |   NO     |   YES    |    9     |    8     |    7     |    6     |
 *        +----------+----------+----------+----------+----------+----------+
 *        |  Bit 5   |  Bit 4   |  Bit 3   |  Bit 2   |  Bit 1   |  Bit 0   |
 *        |    5     |    4     |    3     |    2     |    1     |    0     |
 *
 * Each key pulls one bit low when pressed.
 */

// Named constants for clarity
static constexpr uint16_t BIT_0 = 1 << 0;  // Key 0
static constexpr uint16_t BIT_1 = 1 << 1;  // Key 1
// ...

static Key rawToKey(uint16_t raw) {
    switch (raw & 0x0FFF) {
        case (0xFFF ^ BIT_0): return Key::KEY_0;  // Bit 0 low
        case (0xFFF ^ BIT_1): return Key::KEY_1;  // Bit 1 low
        // ...
    }
}
```

## References
- [C++ Core Guidelines - I.1: Use clear naming](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#I1-use-clear-naming)
- [C++ Core Guidelines - ES.4: Use meaningful names](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#es4-use-meaningful-names)
