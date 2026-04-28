---
title: "[LOW] ServiceRegistry::getService doesn't validate service name length"
severity: LOW
domain: cdc_core/ServiceRegistry
lens: edge-case-testing
labels:
  - "audit:testing/edge-cases"
---

## Summary
In `ServiceRegistry::getService()` (file: `components/cdc_core/src/ServiceRegistry.cpp:70-80`), the function checks if `name` is null but doesn't validate the length of the name string before using `strcmp()`.

Lines 70-80:
```cpp
IService* ServiceRegistry::getService(const char* name) {
    if (!name) return nullptr;  // Only null check, no length validation

    for (size_t i = 0; i < count_; i++) {
        if (strcmp(services_[i].name, name) == 0) {
            return services_[i].service;
        }
    }

    return nullptr;
}
```

While this works correctly for normal strings, extremely long service names (e.g., 1000+ characters) would cause unnecessary iterations through the service array.

## Impact
- **Performance**: Long service names cause many unnecessary string comparisons
- **Denial of service**: Malicious input with very long names could slow down service lookup
- **Memory**: Stack buffer for service name could overflow if not properly sized

## Evidence
File: `components/cdc_core/src/ServiceRegistry.cpp`, lines 70-80

```cpp
IService* ServiceRegistry::getService(const char* name) {
    if (!name) return nullptr;

    for (size_t i = 0; i < count_; i++) {
        if (strcmp(services_[i].name, name) == 0) {  // Line 74
            return services_[i].service;
        }
    }

    return nullptr;
}
```

Line 57 (registerService):
```cpp
bool ServiceRegistry::registerService(const char* name, IService* service) {
    if (!name || !service) {
        LOG_E(TAG, "Invalid parameters");
        return false;
    }
    // ...
}
```

The `registerService` function also lacks length validation. Service names are stored as pointers (`const char* name;`), so the actual length depends on where the string is allocated.

## Recommended Fix
Add reasonable length limits for service names:

```cpp
static constexpr size_t MAX_SERVICE_NAME_LEN = 64;

IService* ServiceRegistry::getService(const char* name) {
    if (!name) return nullptr;
    
    // Check for reasonable length (avoid long strings)
    size_t len = 0;
    while (len < MAX_SERVICE_NAME_LEN && name[len] != '\0') {
        len++;
    }
    if (name[len] != '\0') {  // String too long
        return nullptr;
    }

    for (size_t i = 0; i < count_; i++) {
        if (strcmp(services_[i].name, name) == 0) {
            return services_[i].service;
        }
    }

    return nullptr;
}

bool ServiceRegistry::registerService(const char* name, IService* service) {
    if (!name || !service) {
        LOG_E(TAG, "Invalid parameters");
        return false;
    }
    
    // Check for reasonable length
    size_t len = strlen(name);
    if (len == 0 || len > MAX_SERVICE_NAME_LEN) {
        LOG_E(TAG, "Service name too long (%zu chars, max %zu)", len, MAX_SERVICE_NAME_LEN);
        return false;
    }

    if (count_ >= MAX_SERVICES) {
        LOG_E(TAG, "Registry full, cannot register '%s'", name);
        return false;
    }

    // Check for duplicate name
    for (size_t i = 0; i < count_; i++) {
        if (strcmp(services_[i].name, name) == 0) {
            LOG_E(TAG, "Service '%s' already registered", name);
            return false;
        }
    }

    services_[count_].name = name;
    services_[count_].service = service;
    count_++;

    LOG_I(TAG, "Registered service '%s'", name);
    return true;
}
```

## References
- CWE-131: Incorrect Calculation of Multi-Byte String Length
- OWASP: Input Validation Cheat Sheet
