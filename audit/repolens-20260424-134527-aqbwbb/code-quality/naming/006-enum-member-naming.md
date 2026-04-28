---
title: "[LOW] Inconsistent enum member naming: mixed patterns in EventType"
severity: LOW
domain: cdc_core
lens: naming-conventions
labels:
  - "audit:code-quality/naming"
---

## Summary
In `components/cdc_core/include/cdc_core/EventBus.h`, enum members use inconsistent naming patterns. Some use `PREFIX_VALUE` format (`POWER_USB_CONNECTED`), while others use simple names (`TIMER_TICK`, `MODULE_EVENT`).

**Evidence** (lines 10-40):
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
    POWER_BATTERY_LOW,
    POWER_BATTERY_CRITICAL,

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

    // Module error
    MODULE_ERROR,

    EVENT_COUNT
};
```

## Impact
- **Consistency**: No single pattern for enum naming (e.g., `POWER_` prefix vs `TIMER_` vs `MODULE_`)
- **Discoverability**: Developers may search for `USB_CONNECTED` instead of `POWER_USB_CONNECTED`
- **Maintainability**: Unclear which pattern to follow when adding new events

## Evidence
- File: `components/cdc_core/include/cdc_core/EventBus.h`
- Lines: 10-40
- Inconsistent patterns: `POWER_USB_CONNECTED` (2-word prefix), `KEY_PRESSED` (1-word prefix), `BLE_CONNECTED` (abbreviation prefix)

## Recommended Fix
Standardize enum naming to use consistent prefix patterns:

**Option 1: Use single-word prefixes**
```cpp
enum class EventType : uint8_t {
    // Input events
    KEY_PRESSED,
    KEY_RELEASED,
    KEY_LONG_PRESS,

    // Power events
    USB_CONNECTED,
    USB_DISCONNECTED,
    CHARGING,
    BATTERY_LOW,
    BATTERY_CRITICAL,

    // System events
    UNLOCK,
    LOCK,
    SLEEP,
    WAKE,

    // Bluetooth events
    BLE_CONNECTED,
    BLE_DISCONNECTED,
    BLE_PAIRING,

    // Timer
    TICK,

    // Module events
    MODULE_EVENT,
    MODULE_ERROR,

    EVENT_COUNT
};
```

**Option 2: Use consistent multi-word prefixes**
```cpp
enum class EventType : uint8_t {
    INPUT_KEY_PRESSED,
    INPUT_KEY_RELEASED,
    INPUT_KEY_LONG_PRESS,
    POWER_USB_CONNECTED,
    POWER_USB_DISCONNECTED,
    POWER_CHARGING,
    SYSTEM_UNLOCK,
    SYSTEM_LOCK,
    BT_CONNECTED,
    BT_DISCONNECTED,
    TIMER_TICK,
    MODULE_EVENT,
    MODULE_ERROR,
    EVENT_COUNT
};
```

## References
- C++ Core Guidelines: [I6: Capitalize macros](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#I6)
- ESP-IDF convention: Uses `ESP_` prefix consistently for all events
