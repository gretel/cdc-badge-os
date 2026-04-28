---
title: "[MEDIUM] ServiceRegistry typed services lack type erasure safety"
severity: MEDIUM
domain: architecture/api-contract
lens: api-contract
labels:
  - "audit:architecture/api-contract"
---

## Summary
The `ServiceRegistry` uses `void*` for typed service storage, requiring manual type casting that can fail silently:

```cpp
// components/cdc_core/include/cdc_core/ServiceRegistry.h
template<typename T>
bool provide(ServiceType type, T* service) {
    return registerTypedService(type, service);  // Casts to void*
}

template<typename T>
T* request(ServiceType type) {
    return static_cast<T*>(getTypedService(type));  // No type check!
}
```

The internal storage is:
```cpp
void* typedServices_[MAX_TYPED_SERVICES] = {};
```

There's no runtime type checking. If a consumer requests the wrong type:
```cpp
auto* display = CDC_REQUEST_SERVICE(IDisplay, ServiceType::KEYBOARD);
// Returns nullptr silently, or worse - wrong type cast!
```

## Impact
- **Type safety**: Wrong type requests return garbage without warning
- **Debugging**: Hard to find type mismatches (nullptr dereferences)
- **No compile-time guarantee**: Template doesn't validate against ServiceType enum

## Evidence
- ServiceRegistry implementation: components/cdc_core/include/cdc_core/ServiceRegistry.h:68-83
- Typed service storage: components/cdc_core/include/cdc_core/ServiceRegistry.h:138
- Usage macros: components/cdc_core/include/cdc_core/ServiceRegistry.h:146-151

## Recommended Fix
Add type-safe service registry:

1. **Use std::type_info for runtime type checking**:
```cpp
#include <typeinfo>

class ServiceRegistry {
private:
    struct TypedService {
        void* ptr;
        const std::type_info* type;
    };
    TypedService typedServices_[MAX_TYPED_SERVICES] = {};
    
    template<typename T>
    bool registerTypedService(ServiceType type, T* service) {
        size_t idx = static_cast<size_t>(type);
        typedServices_[idx].ptr = service;
        typedServices_[idx].type = &typeid(T);
        return true;
    }
    
    template<typename T>
    T* getTypedService(ServiceType type) {
        size_t idx = static_cast<size_t>(type);
        if (typedServices_[idx].type != &typeid(T)) {
            LOG_W(TAG, "Type mismatch for service %d", type);
            return nullptr;
        }
        return static_cast<T*>(typedServices_[idx].ptr);
    }
};
```

2. **Or use compile-time mapping** (if ServiceType and types always match):
```cpp
template<typename T>
struct ServiceTypeMap;

template<>
struct ServiceTypeMap<cdc::hal::IKeypad> {
    static constexpr ServiceType type = ServiceType::KEYBOARD;
};
```

## References
- ServiceRegistry: components/cdc_core/include/cdc_core/ServiceRegistry.h
