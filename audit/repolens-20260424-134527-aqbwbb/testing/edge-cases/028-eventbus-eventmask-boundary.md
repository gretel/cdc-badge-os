---
title: "[MEDIUM] EventBus event mask overflow when EventType exceeds 32 values"
severity: MEDIUM
domain: cdc_core/EventBus
lens: edge-case-testing
labels:
  - "audit:testing/edge-cases"
---

## Summary
In `EventBus.h` (file: `components/cdc_core/include/cdc_core/EventBus.h`), the event mask uses a 32-bit integer (`uint32_t`) to store subscriptions, but the `eventMask()` function at line 125 shifts by `static_cast<uint8_t>(type)`. If `EventType` enum grows beyond 32 values, the shift will overflow.

```cpp
static constexpr uint32_t eventMask(EventType type) {
    return 1u << static_cast<uint8_t>(type);
}
```

The `EventType` enum currently has about 20 values (lines 11-44), but as the system grows, adding more event types could silently overflow the mask.

## Impact
- **Silent data loss**: Events beyond index 31 will produce incorrect masks
- **Subscription failures**: Handlers might receive wrong events or miss their events
- **Hard to debug**: Overflow produces valid-looking but incorrect bit patterns

## Evidence
File: `components/cdc_core/include/cdc_core/EventBus.h`

Lines 11-44 (EventType enum):
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

    // Custom module events (use data.value for sub-type)
    MODULE_EVENT,

    // Module error (data.ptr = module name, data.value = index)
    MODULE_ERROR,

    EVENT_COUNT  // Current: ~20 values
};
```

Line 125 (eventMask):
```cpp
static constexpr uint32_t eventMask(EventType type) {
    return 1u << static_cast<uint8_t>(type);
}
```

Line 132 (Subscription mask):
```cpp
struct Subscription {
    EventHandler handler;
    uint32_t mask;  // 32-bit mask
    bool active;
};
```

With 32-bit mask and up to 32 events, adding more than 32 event types will cause overflow.

## Recommended Fix
Option 1: Use a larger mask type (uint64_t) for up to 64 events:
```cpp
static constexpr uint64_t eventMask(EventType type) {
    return 1u << static_cast<uint8_t>(type);
}

struct Subscription {
    EventHandler handler;
    uint64_t mask;  // 64-bit mask
    bool active;
};
```

Option 2: Add compile-time check to prevent overflow:
```cpp
static constexpr uint32_t eventMask(EventType type) {
    static_assert(static_cast<uint8_t>(EventType::EVENT_COUNT) < 32,
                  "EventType count exceeds 32, need uint64_t for mask");
    return 1u << static_cast<uint8_t>(type);
}
```

Option 3: Use an array of booleans for masks (unbounded):
```cpp
static constexpr size_t MAX_EVENT_TYPES = 64;
struct Subscription {
    EventHandler handler;
    bool mask[MAX_EVENT_TYPES];  // Boolean array
    bool active;
};
```

## References
- CWE-190: Integer overflow or wraparound
- C++ Core Guidelines, [INT.13: Avoid bitwise operations on signed integers](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#int13-avoid-bitwise-operations-on-signed-integers)
