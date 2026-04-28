---
title: "[HIGH] ServiceRegistry lacks tests for typed service discovery between modules"
severity: HIGH
domain: core
lens: integration-test-gaps
labels:
  - "audit:testing/integration-test-gaps"
  - "component:cdc_core"
  - "area:service-registry"
---

## Summary
The `ServiceRegistry` component provides dependency injection for modules to discover services (e.g., keyboard, display), but **no integration tests** verify that modules can successfully discover and use services provided by other components.

## Impact
- **Silent service lookup failures**: Modules may request services before they're registered
- **Type safety not verified**: Typed service pattern (`provide<T>()`, `request<T>()`) has no tests
- **No lifecycle verification**: Services may be stopped while modules still reference them

## Evidence

**ServiceRegistry API** (`components/cdc_core/include/cdc_core/ServiceRegistry.h:28-140`):
```cpp
class ServiceRegistry {
    bool registerService(const char* name, IService* service);
    IService* getService(const char* name);
    
    template<typename T>
    bool provide(ServiceType type, T* service);
    
    template<typename T>
    T* request(ServiceType type);
};
```

**Service types** (`components/cdc_core/include/cdc_core/ServiceRegistry.h:12-16`):
```cpp
enum class ServiceType {
    KEYBOARD,      // IKeyboardProvider
    CLIPBOARD,     // Future
    NOTIFICATION,  // Future
};
```

**Module usage example** (`components/mod_totp/src/TotpModule.cpp:5`):
```cpp
#include "cdc_core/IKeyboardProvider.h"
// Module requests keyboard service but no test verifies this works
```

**Current test coverage**: None for ServiceRegistry

## Recommended Fix

Create integration test `test_service_registry_integration/` that verifies:

1. **Named service registration**: Services can be registered and retrieved by name
2. **Typed service pattern**: `provide<T>()` and `request<T>()` work correctly
3. **Service lifecycle**: Services are initialized/started/stopped in correct order
4. **Module service discovery**: Modules can find services they depend on

**Test structure** (example):
```cpp
// test/test_service_registry_integration/test_service_registry.cpp
#include "cdc_core/ServiceRegistry.h"
#include "cdc_core/IKeyboardProvider.h"

class MockKeyboard : public IKeyboardProvider {
    // Implementation
};

void test_typed_service_provide_request() {
    ServiceRegistry& registry = ServiceRegistry::instance();
    MockKeyboard keyboard;
    
    registry.provide<IKeyboardProvider>(ServiceType::KEYBOARD, &keyboard);
    
    IKeyboardProvider* kb = registry.request<IKeyboardProvider>(ServiceType::KEYBOARD);
    ASSERT_NE(kb, nullptr);
}
```

## References
- [ServiceRegistry header](components/cdc_core/include/cdc_core/ServiceRegistry.h)
- [ServiceRegistry implementation](components/cdc_core/src/ServiceRegistry.cpp)
- [IKeyboardProvider interface](components/cdc_core/include/cdc_core/IKeyboardProvider.h)
