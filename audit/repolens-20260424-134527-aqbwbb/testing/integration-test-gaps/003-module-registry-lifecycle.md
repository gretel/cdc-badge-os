---
title: "[HIGH] ModuleRegistry lacks integration tests for module lifecycle management"
severity: HIGH
domain: core
lens: integration-test-gaps
labels:
  - "audit:testing/integration-test-gaps"
  - "component:cdc_core"
  - "area:module-registry"
---

## Summary
The `ModuleRegistry` component manages the lifecycle of all 12+ modules (init, start, stop, event dispatch), but **no integration tests** verify that modules correctly receive lifecycle events and transition through states.

## Evidence

**ModuleRegistry API** (`components/cdc_core/include/cdc_core/ModuleRegistry.h:28-188`):
```cpp
class ModuleRegistry {
    bool registerModule(IModule* module);
    bool initAll();
    bool startAll();
    void stopAll();
    
    void dispatchUnlock();
    void dispatchLock();
    void dispatchUsbConnect();
    void dispatchUsbDisconnect();
    void dispatchTick(uint32_t nowMs);
};
```

**IModule interface** (`components/cdc_core/include/cdc_core/IModule.h:53-128`):
```cpp
class IModule : public IService {
    virtual uint8_t getMenuItems(ModuleMenuItem* items, uint8_t maxItems);
    virtual void onUnlock() {}
    virtual void onLock() {}
    virtual void onUsbConnect() {}
    virtual void onUsbDisconnect() {}
    virtual void onTick(uint32_t nowMs) {}
};
```

**Usage in main** (`main/main.cpp:220-245`):
```cpp
modules_register_all();
ModuleRegistry::instance().runAllInitializers();
ModuleRegistry::instance().dispatchTick(nowMs);
```

**Current test coverage**: Only `test_vcard_module_link` which just calls register function

## Impact
- **Lifecycle bugs**: Modules may not receive `onUnlock()`, `onTick()`, etc.
- **State inconsistencies**: Modules may not transition correctly through UNINITIALIZED → INITIALIZED → STARTED
- **No module error handling**: `reportModuleError()`, `retryModule()` not tested

## Recommended Fix

Create integration test `test_module_registry_integration/` that verifies:

1. **Module registration**: Modules register correctly and don't duplicate
2. **Lifecycle transitions**: initAll() → startAll() → stopAll() works correctly
3. **Event dispatch**: onUnlock(), onLock(), onUsbConnect() called on all modules
4. **Tick dispatch**: onTick() called with correct timestamps
5. **Module error handling**: reportModuleError() stops module, retryModule() restarts

**Test structure** (example):
```cpp
// test/test_module_registry_integration/test_module_lifecycle.cpp
#include "cdc_core/ModuleRegistry.h"
#include "mod_gpg/GpgModule.h"

void test_module_lifecycle_transitions() {
    ModuleRegistry& registry = ModuleRegistry::instance();
    mod_gpg::GpgModule& gpg = mod_gpg::GpgModule::instance();
    
    registry.registerModule(&gpg);
    registry.initAll();
    
    ASSERT_EQ(gpg.getState(), ServiceState::INITIALIZED);
    
    registry.startAll();
    ASSERT_EQ(gpg.getState(), ServiceState::STARTED);
}

void test_module_event_dispatch() {
    bool unlockCalled = false;
    // Set up module to track onUnlock() calls
    registry.dispatchUnlock();
    ASSERT_TRUE(unlockCalled);
}
```

## References
- [ModuleRegistry header](components/cdc_core/include/cdc_core/ModuleRegistry.h)
- [ModuleRegistry implementation](components/cdc_core/src/ModuleRegistry.cpp)
- [IModule interface](components/cdc_core/include/cdc_core/IModule.h)
