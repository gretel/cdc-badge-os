---
title: "[MEDIUM] EventBus Event union lacks type-safe accessors"
severity: MEDIUM
domain: architecture/api-contract
lens: type-safety
labels:
  - "audit:architecture/api-contract"
---

## Summary
The `Event` struct in `EventBus.h` uses a C-style union for event data with no type-safe accessors. Consumers must know which union member to access based on the event type, with no compile-time checking.

**Evidence location:** `components/cdc_core/include/cdc_core/EventBus.h:53-58`

```cpp
struct Event {
    EventType type;
    uint32_t timestamp;  // millis since boot
    union {
        char key;        // For KEY_* events
        uint8_t value;   // Generic value
        void* ptr;       // Pointer to extended data
    } data;
};
```

## Impact
1. **No compile-time type checking**: Wrong union member can be accessed without compiler warnings
2. **Memory layout confusion**: `char` (1 byte), `uint8_t` (1 byte), and `void*` (4 or 8 bytes) have different sizes
3. **Alignment issues**: Reading `data.ptr` after writing `data.key` may read garbage
4. **Maintainability**: Adding new event types requires updating all switch statements manually

**Problematic usage:**
```cpp
Event event = { .type = EventType::KEY_PRESSED, .data = { .key = 'A' } };
// Later...
if (event.type == EventType::KEY_PRESSED) {
    char key = event.data.key;  // Correct
}
if (event.type == EventType::SYSTEM_UNLOCK) {
    void* ptr = event.data.ptr;  // What if someone wrote value before?
}
// No compiler error if you mix these up!
```

## Evidence
The `EventHandler` typedef (line 62):
```cpp
using EventHandler = void(*)(const Event&);
```

Handlers receive the raw `Event` and must use switch/case to determine which union member to access. No helper functions exist to safely extract data.

## Recommended Fix
Add type-safe accessors:

```cpp
struct Event {
    EventType type;
    uint32_t timestamp;
    union {
        char key;
        uint8_t value;
        void* ptr;
    } data;

    // Type-safe accessors
    char getKey() const {
        assert(type == EventType::KEY_PRESSED ||
               type == EventType::KEY_RELEASED ||
               type == EventType::KEY_LONG_PRESS);
        return data.key;
    }

    uint8_t getValue() const {
        assert(type >= EventType::POWER_USB_CONNECTED &&
               type <= EventType::MODULE_ERROR);
        return data.value;
    }

    void* getPtr() const {
        assert(type == EventType::MODULE_ERROR);
        return data.ptr;
    }

    // Template helpers for generic access
    template<typename T>
    T get() const;  // Specialize for char, uint8_t, void*
};

// Or use std::variant (C++17)
#include <variant>

struct Event {
    EventType type;
    uint32_t timestamp;
    std::variant<char, uint8_t, void*> data;

    char getKey() const { return std::get<char>(data); }
    uint8_t getValue() const { return std::get<uint8_t>(data); }
    void* getPtr() const { return std::get<void*>(data); }
};
```

## References
- C++ Core Guidelines T.31: Use `std::variant` and `std::visit` for type-safe unions
- C++ Core Guidelines E.10: Don't use base-type conversions
- ISO C++ Standard §12.2 (std::variant): Type-safe union
