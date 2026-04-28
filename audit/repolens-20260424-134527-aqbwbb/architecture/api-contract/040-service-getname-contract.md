---
title: "[LOW] IService getName() Contract Missing Null Safety"
severity: LOW
domain: API Contract Integrity
lens: interface-contracts
labels:
  - "audit:architecture/api-contract"
---

## Summary
The `IService::getName()` method is defined as returning `const char*` but doesn't specify whether it can return `nullptr`. Callers may assume it's always valid, leading to potential null pointer dereferences.

**Location**: `components/cdc_core/include/cdc_core/IService.h:50-53`

## Impact
1. **Null pointer dereferences**: Callers may not check for nullptr
2. **Inconsistent implementations**: Some services may return nullptr, others empty string
3. **Logging issues**: `LOG_E(TAG, "Service: %s", service->getName())` may crash

## Evidence

In `components/cdc_core/include/cdc_core/IService.h:47-53`:
```cpp
/**
 * Get service name (for logging/debugging)
 */
virtual const char* getName() const = 0;
```

**No specification** of:
- Can `getName()` return `nullptr`?
- What should be returned for unnamed services?
- Is the returned string null-terminated?
- Is the string static or dynamically allocated?

In `components/cdc_core/src/ServiceRegistry.cpp:45-50`:
```cpp
bool ServiceRegistry::registerService(const char* name, IService* service) {
    if (!name || !service) return false;
    if (count_ >= MAX_SERVICES) return false;
    // Check for duplicate name
    for (size_t i = 0; i < count_; i++) {
        if (strcmp(services_[i].name, name) == 0) return false;
    }
    services_[count_].name = name;
    services_[count_].service = service;
    count_++;
    return true;
}
```

The service registry stores the name pointer but doesn't copy it. If the original string is freed, the stored pointer is dangling.

In `components/mod_gpg/src/GpgModule.cpp:9`:
```cpp
const char* getName() const override { return "mod_gpg"; }
```

Returns string literal (safe).

In `components/cdc_hal/src/Tropic01Element.cpp:46`:
```cpp
const char* getName() const override { return "secure_element"; }
```

Returns string literal (safe).

But what if a module stores the name dynamically?

```cpp
class DynamicNameService : public IService {
    std::string name_;
public:
    const char* getName() const override { return name_.c_str(); }
};
```

If `name_` is modified after `getName()` is called, the returned pointer may be stale.

## Recommended Fix

1. **Document the getName() contract**:
   ```cpp
   /**
    * \brief Get service name (for logging/debugging).
    * \return Null-terminated string, never nullptr.
    * \note Returns static string literal or module name.
    * \note Caller must not free the returned string.
    * \note String remains valid for service lifetime.
    */
   virtual const char* getName() const = 0;
   ```

2. **Add validation** in ServiceRegistry:
   ```cpp
   bool ServiceRegistry::registerService(const char* name, IService* service) {
       if (!name || !service) return false;
       // Get service's own name for validation
       const char* serviceName = service->getName();
       if (!serviceName || strlen(serviceName) == 0) {
           return false;  // Service must have a name
       }
       // ... rest of registration
   }
   ```

3. **Add helper** for safe name access:
   ```cpp
   /**
    * \brief Get service name with null safety.
    * \param service Service to get name from.
    * \return Name string or "unnamed" if nullptr or empty.
    */
   static inline const char* getServiceName(const IService* service) {
       if (!service) return "nullptr";
       const char* name = service->getName();
       return name && name[0] ? name : "unnamed";
   }
   ```

4. **Update logging** to use safe helper:
   ```cpp
   // Instead of:
   LOG_I(TAG, "Service: %s", service->getName());

   // Use:
   LOG_I(TAG, "Service: %s", getServiceName(service));
   ```

## References
- `components/cdc_core/include/cdc_core/IService.h:50-53` - getName() definition
- `components/cdc_core/src/ServiceRegistry.cpp:45-50` - Service registration
- `components/mod_gpg/src/GpgModule.cpp:9` - Example implementation
