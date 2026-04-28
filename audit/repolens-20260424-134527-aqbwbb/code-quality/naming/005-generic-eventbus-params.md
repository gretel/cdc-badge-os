---
title: "[LOW] Generic parameter names 'ptr' and 'value' in EventBus data union"
severity: LOW
domain: code-quality/naming
lens: naming-conventions
labels:
  - "audit:code-quality/naming"
---

## Summary

The `EventBus::Event` struct uses generic parameter names `ptr` and `value` in its data union, which don't convey the purpose or type of data being stored.

### Affected Files

| File | Location |
|------|----------|
| `components/cdc_core/include/cdc_core/EventBus.h:53-57` | `Event` struct definition |

## Impact

**Readability**: Developers must look at usage comments to understand what each field holds.

**Self-documentation**: The struct should be self-explanatory without requiring external documentation.

**Type safety**: Generic names encourage misuse (e.g., storing pointers where values are expected).

## Evidence

From `components/cdc_core/include/cdc_core/EventBus.h:48-58`:
```cpp
/**
 * Event data structure
 */
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

Usage comments (lines 38, 41):
```cpp
// Custom module events (use data.value for sub-type)
// Module error (data.ptr = module name, data.value = index)
```

The comments are required to understand the intended usage of `ptr` and `value`.

## Recommended Fix

Rename union members to be more descriptive:

**Option A (Recommended):**
```cpp
struct Event {
    EventType type;
    uint32_t timestamp;  // millis since boot
    union {
        char key;        // For KEY_* events
        uint8_t intValue;   // For numeric values
        void* ptrValue;     // For pointer values
    } data;
};
```

**Option B (More specific to actual use cases):**
```cpp
struct Event {
    EventType type;
    uint32_t timestamp;  // millis since boot
    union {
        char key;        // For KEY_* events
        uint8_t moduleIndex;  // For MODULE_EVENT
        const char* moduleName;  // For MODULE_ERROR
    } data;
};
```

**Option C (Named union with type hint):**
```cpp
struct Event {
    EventType type;
    uint32_t timestamp;  // millis since boot
    
    // For KEY_* events
    char keyChar() const { return data.key; }
    
    // For numeric values
    uint8_t intValue() const { return data.value; }
    
    // For pointer values
    void* ptrValue() const { return data.ptr; }
    
    union {
        char key;
        uint8_t value;
        void* ptr;
    } data;
};
```

Process:
1. Choose one of the above options
2. Update union member names in `EventBus.h`
3. Update all usage sites across the codebase
4. Verify compilation and test

## References

- [Clean Code: Meaningful Names](https://github.com/unclebob/clean-code-swift/blob/master/README.md)
- [Google C++ Style Guide - Parameter Names](https://google.github.io/styleguide/cppguide.html#Parameter_Names)

</content>