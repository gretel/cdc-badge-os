---
title: "[LOW] EventBus eventMask() vulnerable to overflow with future EventType additions"
severity: LOW
domain: events
lens: edge-cases
labels:
  - "audit:testing/edge-cases"
---

## Summary

The `EventBus::eventMask()` function (`components/cdc_core/include/cdc_core/EventBus.h:124-126`) creates a bitmask by shifting `1u` left by the `EventType` enum value. With 18+ event types currently defined and room for `EVENT_COUNT` (which could be up to 256 since `EventType` is `uint8_t`), there's a risk of overflow if more than 32 event types are added in the future.

**Location**: `components/cdc_core/include/cdc_core/EventBus.h:124-126`

The `subscribe()` function accepts a `uint32_t mask` parameter, and the mask is checked against event types using `(handlers_[i].mask & typeMask)`. If `EventType` values exceed 31, the shift `1u << static_cast<uint8_t>(type)` will overflow for values >= 32, producing incorrect masks.

Currently there are ~18 event types defined, so this is not yet a problem. However, as the system grows, adding new event types without considering this limit could silently break event filtering.

## Impact

**Silent Feature Breakage**: If developers add new `EventType` values beyond 31 (e.g., `EVENT_COUNT` grows to 40), the `eventMask()` function will produce incorrect bitmasks:
- `eventMask(EventType::EVENT_32)` → `1u << 32` = undefined behavior (shift >= width of uint32_t)
- `eventMask(EventType::EVENT_33)` → `1u << 33` = `1u << 1` = 2 (wraps)

This would cause event filtering to work incorrectly, potentially delivering events to wrong handlers or missing events entirely.

## Evidence

**Code at line 124-126** (`eventMask()`):
```cpp
static constexpr uint32_t eventMask(EventType type) {
    return 1u << static_cast<uint8_t>(type);
}
```

**Code at line 95** (`subscribe()`):
```cpp
uint8_t subscribe(EventHandler handler, uint32_t mask = 0);
```

**Code at line 130-135** (`EventBus.h` - mask usage in `process()`):
```cpp
// Check mask (0 = receive all)
if (handlers_[i].mask == 0 ||
    (handlers_[i].mask & typeMask)) {
    handlers_[i].handler(event);
}
```

**Current EventType count**: 18 defined (indices 0-17)
**Safe limit**: 32 event types (indices 0-31)
**Headroom**: ~14 event types before overflow

**Test case that triggers edge case**:
```cpp
// Hypothetical future code with 35 event types
enum class EventType : uint8_t {
    // ... 32 existing types ...
    TYPE_32,  // index 32
    TYPE_33,  // index 33
    TYPE_34,  // index 34
    EVENT_COUNT
};

// This would overflow:
uint32_t mask = EventBus::eventMask(EventType::TYPE_32);  // 1u << 32 = UB!
```

## Recommended Fix

Add a static assertion to enforce the limit and consider using `uint64_t` for the mask if more than 32 event types are needed:

**Option 1 - Add compile-time check**:
```cpp
static constexpr size_t MAX_EVENT_TYPES = 32;
static_assert(static_cast<size_t>(EventType::EVENT_COUNT) <= MAX_EVENT_TYPES,
              "EVENT_COUNT exceeds mask capacity (32 types max)");

static constexpr uint32_t eventMask(EventType type) {
    static_assert(static_cast<size_t>(type) < 32, "EventType index too large for uint32_t mask");
    return 1u << static_cast<uint8_t>(type);
}
```

**Option 2 - Use uint64_t for larger capacity**:
```cpp
// Change mask type to uint64_t
using EventMask = uint64_t;

struct Subscription {
    EventHandler handler;
    EventMask mask;  // uint64_t instead of uint32_t
    bool active;
};

static constexpr EventMask eventMask(EventType type) {
    return 1ULL << static_cast<uint8_t>(type);  // 64-bit shift
}

uint8_t subscribe(EventHandler handler, EventMask mask = 0);
```

**Option 3 - Document the limit**:
If changing types is too invasive, at minimum document the constraint:
```cpp
/**
 * Create event mask for subscribe()
 * \note Supports up to 32 event types (EventType indices 0-31)
 * \param type EventType to create mask for
 * \return Bitmask with bit set for \p type
 */
static constexpr uint32_t eventMask(EventType type) {
    return 1u << static_cast<uint8_t>(type);
}
```

## References

- [C++ Left Shift Undefined Behavior](https://en.cppreference.com/w/cpp/language/operator_arithmetic#Shift_operators)
- [CWE-190: Integer Overflow or Wraparound](https://cwe.mitre.org/data/definitions/190.html)
