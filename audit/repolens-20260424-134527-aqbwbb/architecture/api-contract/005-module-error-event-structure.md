---
title: "[MEDIUM] EventBus MODULE_ERROR event lacks structured payload"
severity: MEDIUM
domain: architecture/api-contract
lens: api-contract
labels:
  - "audit:architecture/api-contract"
---

## Summary
The `EventBus` defines a `MODULE_ERROR` event type but uses an unstructured payload format:

```cpp
// components/cdc_core/include/cdc_core/EventBus.h:43
// MODULE_ERROR: data.ptr = module name, data.value = index

struct Event {
    EventType type;
    uint32_t timestamp;
    union {
        char key;        // For KEY_* events
        uint8_t value;   // Generic value
        void* ptr;       // Pointer to extended data
    } data;
};
```

The `MODULE_ERROR` event is dispatched in `ModuleRegistry::reportModuleError()` (components/cdc_core/src/ModuleRegistry.cpp:686-696):
```cpp
Event evt;
evt.type = EventType::MODULE_ERROR;
evt.data.value = i;  // Module index
EventBus::instance().publish(evt);
```

But the module name is stored in `evt.data.ptr` according to the comment, yet the code uses `evt.data.value`. This inconsistency means subscribers cannot reliably get both the module name and index.

## Impact
- **Data loss**: Subscribers can only get index OR name, not both
- **Fragile assumptions**: Code that assumes `data.ptr` is the name will fail
- **No extensibility**: Cannot add error message or error code without breaking changes
- **Type safety**: `void*` pointer could point to anything

## Evidence
- Event definition: components/cdc_core/include/cdc_core/EventBus.h:43, 51-57
- Event dispatch: components/cdc_core/src/ModuleRegistry.cpp:686-696
- Comment says `data.ptr = module name` but code uses `data.value`

## Recommended Fix
Define a structured event payload:

1. **Add event data struct** (components/cdc_core/include/cdc_core/EventBus.h):
```cpp
struct ModuleErrorEvent {
    const char* moduleName;
    uint8_t moduleIndex;
    const char* errorMessage;
    uint32_t timestamp;
};
```

2. **Update Event union**:
```cpp
union {
    char key;
    uint8_t value;
    void* ptr;
    ModuleErrorEvent moduleError;  // Structured payload
};
```

3. **Update dispatch code** (components/cdc_core/src/ModuleRegistry.cpp):
```cpp
void ModuleRegistry::reportModuleError(const char* name, const char* message) {
    // ... existing code ...
    
    Event evt;
    evt.type = EventType::MODULE_ERROR;
    evt.timestamp = esp_timer_get_time() / 1000;
    evt.data.moduleError = {
        moduleName: name,
        moduleIndex: i,
        errorMessage: message,
        timestamp: evt.timestamp
    };
    EventBus::instance().publish(evt);
}
```

4. **Update subscribers** to use structured access:
```cpp
void onModuleError(const Event& evt) {
    const char* name = evt.data.moduleError.moduleName;
    uint8_t index = evt.data.moduleError.moduleIndex;
    const char* msg = evt.data.moduleError.errorMessage;
}
```

## References
- EventBus: components/cdc_core/include/cdc_core/EventBus.h
- ModuleRegistry dispatch: components/cdc_core/src/ModuleRegistry.cpp
