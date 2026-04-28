---
title: "[MEDIUM] EventBus::eventMask shifts 1u by EventType which may overflow for high values"
severity: MEDIUM
domain: cdc_core/EventBus
lens: edge-case-testing
labels:
  - "audit:testing/edge-cases"
---

## Summary
In `EventBus.h` (file: `components/cdc_core/include/cdc_core/EventBus.h:123-126`), the `eventMask` function shifts `1u` (32-bit unsigned) by the `EventType` enum value. If `EventType` values exceed 31, this will cause a shift overflow.

Lines 123-126:
```cpp
static constexpr uint32_t eventMask(EventType type) {
    return 1u << static_cast<uint8_t>(type);
}
```

The `EventType` enum at lines 11-44 has values up to `EVENT_COUNT`, which could potentially exceed 31 as new event types are added. The cast to `uint8_t` at line 125 doesn't prevent the overflow because the shift is still performed on a 32-bit value.

Current EventType values (lines 11-44):
```cpp
enum class EventType : uint8_t {
    KEY_PRESSED,           // 0
    KEY_RELEASED,          // 1
    KEY_LONG_PRESS,        // 2
    POWER_USB_CONNECTED,   // 3
    // ... more values ...
    EVENT_COUNT            // ~20-30 (depends on count)
};
```

## Impact
- **Undefined behavior**: Shifting a 32-bit value by >= 32 is undefined in C++
- **Silent failure**: If new event types are added beyond index 31, the mask will wrap to 0 or a wrong value
- **Bitmask collision**: Two different event types could produce the same mask if the shift overflows

## Evidence
File: `components/cdc_core/include/cdc_core/EventBus.h`, lines 11-44 and 123-126

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

    // Module error
    MODULE_ERROR,

    EVENT_COUNT  // Total count
};
```

Lines 123-126:
```cpp
static constexpr uint32_t eventMask(EventType type) {
    return 1u << static_cast<uint8_t>(type);  // Line 125
}
```

The shift at line 125:
- If `type >= 32`, the result is undefined behavior (shift >= width of type)
- The `uint8_t` cast doesn't help because `1u` is still 32-bit

## Recommended Fix
Add bounds checking and use a safer shift:

```cpp
static constexpr uint32_t eventMask(EventType type) {
    uint8_t index = static_cast<uint8_t>(type);
    if (index >= 32) return 0;  // Invalid mask for out-of-range types
    return (1u << index);
}

// Alternative: Use static_assert for compile-time check
static_assert(static_cast<uint8_t>(EventType::EVENT_COUNT) <= 32,
              "EventType values exceed 32-bit mask capacity");
```

Or use a larger bitmask type if expansion is expected:

```cpp
static constexpr uint64_t eventMask64(EventType type) {
    uint8_t index = static_cast<uint8_t>(type);
    if (index >= 64) return 0;
    return (1ULL << index);
}
```

Also update the handler mask comparison at `EventBus.cpp:132`:
```cpp
// Check mask (0 = receive all)
if (handlers_[i].mask == 0 ||
    (handlers_[i].mask & typeMask)) {
    handlers_[i].handler(event);
}
```

Add a compile-time assertion in the header:
```cpp
static_assert(static_cast<size_t>(EventType::EVENT_COUNT) <= 32,
    "Maximum event types exceeded 32-bit bitmask capacity");
```

## References
- C++ standard [expr.shift]: Shift operators
- CWE-190: Integer Overflow or Wraparound
- ESP32-CDC-Badge event system design
