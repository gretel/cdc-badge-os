---
title: "[MEDIUM] LockScreenContextItem callback lacks context parameter"
severity: MEDIUM
domain: architecture/api-contract
lens: api-contract
labels:
  - "audit:architecture/api-contract"
---

## Summary
The `LockScreenContextItem` structure stores callbacks without context:

```cpp
// components/cdc_core/include/cdc_core/IModule.h:41-46
struct LockScreenContextItem {
    const char* (*getLabel)();      // No context!
    void (*callback)();             // No context!
    uint8_t priority;
    const char* moduleName;
};
```

Usage in GpgModule (components/mod_gpg/src/GpgModule.cpp):
```cpp
// Must use lambda capturing by reference to access module state
items[0] = {
    getLabel: []() -> const char* { return mstr(STR_USER_PIN); },
    callback: []() { /* Must access module via singleton */ },
    priority: 10,
    moduleName: getName()
};
```

The callbacks have no way to receive context, so they must:
1. Use global state
2. Access module via singleton
3. Capture in lambdas (but lambdas can't be stored as function pointers)

## Impact
- **Limited functionality**: Callbacks can't access module instance state
- **Singleton dependency**: Forces use of singletons instead of instances
- **No polymorphism**: Can't have different behavior for same item type

## Evidence
- Structure: components/cdc_core/include/cdc_core/IModule.h:41-46
- Usage: components/mod_gpg/src/GpgModule.cpp (context needed but not available)

## Recommended Fix
Add context parameter:

```cpp
struct LockScreenContextItem {
    const char* (*getLabel)(void* ctx);  // Context parameter
    void (*callback)(void* ctx);         // Context parameter
    uint8_t priority;
    const char* moduleName;
    void* context;                       // Context data (e.g., module pointer)
};
```

Usage:
```cpp
items[0] = {
    getLabel: [](void* ctx) -> const char* { 
        auto* module = static_cast<GpgModule*>(ctx);
        return module->getStatusLabel(); 
    },
    callback: [](void* ctx) { 
        auto* module = static_cast<GpgModule*>(ctx);
        module->showUserPinDialog();
    },
    priority: 10,
    moduleName: "mod_gpg",
    context: &module  // Pass module instance
};
```

## References
- IModule: components/cdc_core/include/cdc_core/IModule.h
- GpgModule usage: components/mod_gpg/src/GpgModule.cpp
