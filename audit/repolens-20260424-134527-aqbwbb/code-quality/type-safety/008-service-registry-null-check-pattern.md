---
title: "[MEDIUM] Missing null check for service name parameter in registerService"
severity: MEDIUM
domain: cdc_core
lens: type-safety
labels:
  - "audit:code-quality/type-safety"
---

## Summary
The `ServiceRegistry::registerService` function checks for null `name` and `service` parameters, but `getService` only checks for null `name` before use. While this is generally safe, the pattern should be consistent across all service lookup functions.

## Evidence
In `components/cdc_core/src/ServiceRegistry.cpp` line 38-42:
```cpp
bool ServiceRegistry::registerService(const char* name, IService* service) {
    if (!name || !service) {
        LOG_E(TAG, "Invalid parameters");
        return false;
    }
    // ...
}
```

In `components/cdc_core/src/ServiceRegistry.cpp` line 70-71:
```cpp
IService* ServiceRegistry::getService(const char* name) {
    if (!name) return nullptr;  // ✓ Checks for null
    // ...
}
```

The `getTypedService` function does NOT check for null type (though it converts to size_t):
```cpp
void* ServiceRegistry::getTypedService(ServiceType type) const {
    size_t idx = static_cast<size_t>(type);
    if (idx >= MAX_TYPED_SERVICES) {
        return nullptr;
    }
    return typedServices_[idx];
}
```

## Impact
1. **Inconsistent API**: Some functions validate inputs more strictly than others.
2. **Future risk**: If new lookup functions are added, developers may not follow the same pattern.

## Recommended Fix
Add a static constexpr for the default/null service type and document expected usage:

```cpp
static constexpr ServiceType SERVICE_TYPE_INVALID = static_cast<ServiceType>(-1);

void* ServiceRegistry::getTypedService(ServiceType type) const {
    size_t idx = static_cast<size_t>(type);
    if (idx >= MAX_TYPED_SERVICES) {
        return nullptr;
    }
    return typedServices_[idx];
}
```

Alternatively, add an enum value for `SERVICE_TYPE_NONE` to make invalid states unrepresentable.

## References
- C++ Core Guidelines F.41: "Use 'noexcept' for functions that won't call virtual functions"
- C++ Core Guidelines I.11: "Use 'noexcept' for functions that won't throw"

</content>