---
title: "[MEDIUM] ServiceRegistry stores services as void* pointers"
severity: MEDIUM
domain: type-safety
lens: c++-casting
labels:
  - "audit:code-quality/type-safety"
---

## Summary
The `ServiceRegistry` component stores typed services in a `void*` array (`typedServices_[MAX_TYPED_SERVICES]`), requiring `static_cast` on retrieval. While this is a common pattern for type erasure, the current implementation lacks compile-time guarantees that the cast matches the original type.

**Location:** `components/cdc_core/include/cdc_core/ServiceRegistry.h:139`

## Impact
**Runtime type confusion**: If a developer calls `request<ServiceA>()` but the service was registered as `ServiceB`, the cast succeeds but dereferencing yields undefined behavior.

**No exhaustiveness checking**: The `ServiceType` enum doesn't guarantee a one-to-one mapping with template instantiations.

**Example vulnerability:**
```cpp
// Register IKeyboardProvider
registry.provide(ServiceType::KEYBOARD, &keyboard);

// Correct retrieval
auto* kb = registry.request<cdc::core::IKeyboardProvider>(ServiceType::KEYBOARD);

// Wrong type, no compile error!
auto* wrong = registry.request<cdc::core::IDisplay>(ServiceType::KEYBOARD);  // Compiles!
```

## Evidence
```cpp
// components/cdc_core/include/cdc_core/ServiceRegistry.h:138-139
static constexpr size_t MAX_TYPED_SERVICES = 8;
void* typedServices_[MAX_TYPED_SERVICES] = {};

// Line 74-77 (provide template)
template<typename T>
bool provide(ServiceType type, T* service) {
    return registerTypedService(type, service);  // Casts to void*
}

// Line 83-87 (request template)
template<typename T>
T* request(ServiceType type) {
    return static_cast<T*>(getTypedService(type));  // Casts back from void*
}
```

## Recommended Fix
Store type-erased wrappers with runtime type checking:

```cpp
#include <typeinfo>

class ServiceRegistry {
private:
    struct TypedService {
        ServiceType type;
        void* ptr;
        const std::type_info* typeInfo;
    };
    TypedService typedServices_[MAX_TYPED_SERVICES] = {};

public:
    template<typename T>
    bool provide(ServiceType type, T* service) {
        auto* ptr = reinterpret_cast<void*>(service);
        for (auto& s : typedServices_) {
            if (s.type == type) {
                s.ptr = ptr;
                s.typeInfo = &typeid(T);
                return true;
            }
        }
        // ...
    }

    template<typename T>
    T* request(ServiceType type) {
        for (const auto& s : typedServices_) {
            if (s.type == type && s.typeInfo) {
                if (*s.typeInfo == typeid(T)) {
                    return static_cast<T*>(s.ptr);
                }
                // Optional: assert or log type mismatch
            }
        }
        return nullptr;
    }
};
```

Alternatively, use `std::any` (C++17) for simpler type-safe storage:
```cpp
#include <any>
std::any typedServices_[MAX_TYPED_SERVICES] = {};

template<typename T>
T* request(ServiceType type) {
    auto& entry = typedServices_[static_cast<size_t>(type)];
    if (entry.has_value() && entry.type() == typeid(T)) {
        return std::any_cast<T*>(entry);
    }
    return nullptr;
}
```

## References
- C++ Core Guidelines, C.17: "For type-erasure, use `std::any`, `std::optional`, `std::variant`, or `std::unique_ptr`"
- C++ Core Guidelines, C.20: "Use `std::any` for heterogeneous collections"
