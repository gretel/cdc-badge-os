---
title: "[LOW] Enum member naming: inconsistent prefix patterns"
severity: LOW
domain: naming-conventions
lens: code-quality
labels:
  - "audit:code-quality/naming"
---

## Summary
Enum members use different prefix patterns. Some enums use type-based prefixes (e.g., `KEY_`), while others use value-based prefixes or no prefix at all.

**Evidence:**

1. **components/cdc_hal/include/cdc_hal/IKeypad.h** (lines 16-22):
   ```cpp
   enum class Key : char {
       KEY_1 = '1', KEY_2 = '2', KEY_3 = '3',
       KEY_4 = '4', KEY_5 = '5', KEY_6 = '6',
       KEY_7 = '7', KEY_8 = '8', KEY_9 = '9',
       KEY_NO = 'N', KEY_0 = '0', KEY_YES = 'Y',
       KEY_NONE = 0
   };
   ```
   Uses `KEY_` prefix for all members (scoped enum, prefix is redundant).

2. **components/cdc_core/include/cdc_core/EventBus.h** (lines 11-45):
   ```cpp
   enum class EventType : uint8_t {
       // Input events
       KEY_PRESSED,
       KEY_RELEASED,
       KEY_LONG_PRESS,

       // Power events
       POWER_USB_CONNECTED,
       POWER_USB_DISCONNECTED,
       POWER_CHARGING,

       // System events
       SYSTEM_UNLOCK,
       SYSTEM_LOCK,
       SYSTEM_SLEEP,
       SYSTEM_WAKE,

       // Bluetooth events
       BLE_CONNECTED,
       BLE_DISCONNECTED,
       BLE_PAIRING_REQUEST,

       // Timer
       TIMER_TICK,

       // Custom module events
       MODULE_EVENT,
       MODULE_ERROR,

       EVENT_COUNT
   };
   ```
   Uses category-based prefixes (POWER_, SYSTEM_, BLE_, etc.).

3. **components/cdc_hal/include/cdc_hal/ISecureElement.h** (lines 12-28):
   ```cpp
   enum class EccCurve : uint8_t {
       P256,       // NIST P-256 (secp256r1)
       ED25519     // Ed25519
   };

   enum class SeResult : uint8_t {
       OK,
       ERROR,
       SESSION_REQUIRED,
       SLOT_EMPTY,
       SLOT_OCCUPIED,
       INVALID_PARAM,
       ALARM_MODE,
       NOT_SUPPORTED
   };
   ```
   No prefix for `EccCurve`, descriptive prefixes for `SeResult`.

4. **components/cdc_ui/include/cdc_ui/I18n.h** (lines 11-15):
   ```cpp
   enum class Language : uint8_t {
       EN = 0,     // English (default/fallback)
       DE = 1,     // German
       COUNT
   };
   ```
   Uses short abbreviations (EN, DE) and `COUNT` sentinel.

## Impact
- **Minor inconsistency**: Since enums are scoped (`enum class`), prefixes are less necessary
- **Readability**: `Key::KEY_1` vs `EventType::POWER_USB_CONNECTED` - different patterns
- **Autocomplete**: Prefixes can help with autocomplete but add verbosity

## Evidence
Comparison:
- `Key::KEY_1` - redundant prefix (type name already `Key`)
- `EventType::POWER_USB_CONNECTED` - descriptive category prefix
- `EccCurve::P256` - no prefix, clear value name
- `SeResult::SLOT_EMPTY` - descriptive prefix for context

## Recommended Fix
Since C++11 scoped enums (`enum class`) provide namespace, avoid redundant prefixes:

**Update Key enum:**
```cpp
enum class Key : char {
    _1 = '1', _2 = '2', _3 = '3',
    _4 = '4', _5 = '5', _6 = '6',
    _7 = '7', _8 = '8', _9 = '9',
    NO = 'N', _0 = '0', YES = 'Y',
    NONE = 0
};
// Usage: Key::_1, Key::NO, Key::YES
```

**Or use more descriptive names:**
```cpp
enum class Key : char {
    One = '1', Two = '2', Three = '3',
    Four = '4', Five = '5', Six = '6',
    Seven = '7', Eight = '8', Nine = '9',
    No = 'N', Zero = '0', Yes = 'Y',
    None = 0
};
```

**Steps:**
1. Decide on enum naming convention (PascalCase without prefix recommended)
2. Update `Key` enum in `IKeypad.h`
3. Ensure consistency across all enums
4. Update all usage sites

## References
- [C++ Core Guidelines - Enumerations](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Renum-enums)
- [Google C++ Style Guide - Enumerations](https://google.github.io/styleguide/cppguide.html#Enumerations)
