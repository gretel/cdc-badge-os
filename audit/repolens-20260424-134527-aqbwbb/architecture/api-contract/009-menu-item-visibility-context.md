---
title: "[MEDIUM] ModuleMenuItem::isVisible callback lacks const-correctness and context"
severity: MEDIUM
domain: architecture/api-contract
lens: api-contract
labels:
  - "audit:architecture/api-contract"
---

## Summary
The `ModuleMenuItem::isVisible` callback is defined as a bare function pointer without context:

```cpp
// components/cdc_core/include/cdc_core/IModule.h:33
struct ModuleMenuItem {
    const char* label;
    uint8_t priority;
    ui::IView* (*getView)();
    bool (*isVisible)();  // No context parameter!
    const char* moduleName;
    MenuLocation location;
    void (*onSelect)();
};
```

Usage in `ModuleRegistry::getMenuItems()` (components/cdc_core/src/ModuleRegistry.cpp:234):
```cpp
if (moduleItems[j].isVisible && !moduleItems[j].isVisible()) {
    continue;
}
```

This design has issues:
1. **No context**: The callback can't access module state to determine visibility
2. **No const-correctness**: Can't be called on const objects
3. **Global state dependency**: Must rely on global variables or singletons

## Impact
- **Limited functionality**: Visibility can only be based on global state
- **Fragile**: If module state changes, visibility logic must use singletons
- **Hard to test**: Requires global state manipulation

## Evidence
- Structure definition: components/cdc_core/include/cdc_core/IModule.h:28-35
- Usage: components/cdc_core/src/ModuleRegistry.cpp:234

## Recommended Fix
Add context parameter and const-correctness:

```cpp
struct ModuleMenuItem {
    const char* label;
    uint8_t priority;
    ui::IView* (*getView)();
    bool (*isVisible)(void* ctx);  // Context parameter
    const char* moduleName;
    MenuLocation location;
    void (*onSelect)(void* ctx);   // Also add context here
    void* context;                 // Context data (e.g., module pointer)
};
```

Usage:
```cpp
// In module
items[0] = {
    label: "My Menu",
    priority: 50,
    getView: []() -> ui::IView* { return &view; },
    isVisible: [](void* ctx) -> bool { 
        auto* module = static_cast<MyModule*>(ctx);
        return module->isFeatureEnabled();
    },
    moduleName: getName(),
    location: MenuLocation::MAIN_MENU,
    onSelect: [](void* ctx) { 
        auto* module = static_cast<MyModule*>(ctx);
        module->onMenuSelect();
    },
    context: this  // Pass module instance
};
```

## References
- IModule: components/cdc_core/include/cdc_core/IModule.h
- ModuleRegistry: components/cdc_core/src/ModuleRegistry.cpp
