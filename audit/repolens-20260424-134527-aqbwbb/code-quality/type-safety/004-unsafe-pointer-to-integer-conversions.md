---
title: "[LOW] Implicit pointer-to-integer conversion in callback context"
severity: LOW
domain: type-safety
lens: c++-casting
labels:
  - "audit:code-quality/type-safety"
---

## Summary
The `EventBus` uses a union for event data (`char key`, `uint8_t value`, `void* ptr`), and handlers must manually interpret the correct field based on event type. This pattern lacks type safety - a handler could accidentally read the wrong union field.

**Location:** `components/cdc_core/include/cdc_core/EventBus.h:50-58`

## Impact
**Type confusion**: Handlers must know which union field is valid for each event type. A mistake leads to reading garbage data.

**No compile-time checking**: The compiler cannot verify that a handler reads the correct field.

**Example issue:**
```cpp
// Handler reads 'value' but publisher set 'ptr'
void handler(const Event& evt) {
    uint8_t index = evt.data.value;  // Garbage if ptr was set!
}
```

## Evidence
```cpp
// components/cdc_core/include/cdc_core/EventBus.h:50-58
struct Event {
    EventType type;
    uint32_t timestamp;
    union {
        char key;        // For KEY_* events
        uint8_t value;   // Generic value
        void* ptr;       // Pointer to extended data
    } data;
};

// Comment on line 42 documents intended usage:
// MODULE_ERROR (data.ptr = module name, data.value = index)

// components/cdc_os_ui/src/ExpertMenuUi.cpp:289
void onModuleErrorEvent(const core::Event& evt) {
    // Handler must know to read 'value' for MODULE_ERROR
    uint8_t index = static_cast<uint8_t>(evt.data.value);
}
```

## Recommended Fix
Replace the union with a type-safe variant:

```cpp
#include <variant>

struct Event {
    EventType type;
    uint32_t timestamp;
    // Type-safe variant for event payload
    std::variant<char, uint8_t, const char*> payload;
};

// Helper accessors
inline uint8_t getModuleIndex(const Event& evt) {
    if (evt.type == EventType::MODULE_ERROR) {
        return std::get<uint8_t>(evt.payload);
    }
    return 0;
}

// Or use a tagged struct for MODULE_ERROR specifically:
struct ModuleErrorEvent {
    const char* moduleName;
    uint8_t index;
};

// Event can hold different types
struct Event {
    EventType type;
    uint32_t timestamp;
    std::variant<
        char,           // KEY_PRESSED
        uint8_t,        // TIMER_TICK
        ModuleErrorEvent // MODULE_ERROR
    > payload;
};
```

For minimal changes, add inline helpers:
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
    uint8_t getModuleIndex() const {
        return (type == EventType::MODULE_ERROR) ? data.value : 0;
    }
};

// Usage:
uint8_t index = evt.getModuleIndex();
```

## References
- C++ Core Guidelines, Expr.13: "Use static_cast for numeric conversions"
- C.111: "Use enums to represent small integer values, not raw integers"
