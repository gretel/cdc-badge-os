---
title: "[LOW] Service types are hardcoded enum limiting inter-module communication"
severity: LOW
domain: architecture/extensibility
lens: extensibility-plugin-points
labels:
  - "audit:architecture/extensibility"
---

## Summary
Service types for the ServiceRegistry are defined as a fixed enum in `components/cdc_core/include/cdc_core/ServiceRegistry.h:8-15`. Adding a new service type (e.g., `DATABASE`, `NOTIFICATION`) requires modifying the core `ServiceRegistry.h` file.

**Evidence:**
- `components/cdc_core/include/cdc_core/ServiceRegistry.h:8-15` - Hardcoded `ServiceType` enum with only 3 types

## Impact
- **Extension Barrier**: New service types require editing core code
- **Limited Scalability**: Only 3 service types defined, with room for only 5 more (MAX_TYPED_SERVICES = 8)
- **Coupling**: All modules using typed services depend on the core enum

## Evidence
File: `components/cdc_core/include/cdc_core/ServiceRegistry.h:8-15`
```cpp
enum class ServiceType {
    KEYBOARD,      // IKeyboardProvider - keyboard input (BLE HID, USB HID, etc.)
    CLIPBOARD,     // Future: clipboard access
    NOTIFICATION,  // Future: push notifications
};
```

File: `components/cdc_core/include/cdc_core/ServiceRegistry.h:134`
```cpp
// Typed services storage (indexed by ServiceType)
static constexpr size_t MAX_TYPED_SERVICES = 8;
void* typedServices_[MAX_TYPED_SERVICES] = {};
```

Note: Only 3 service types are used, but the array is sized to 8, limiting future growth.

## Recommended Fix
Implement dynamic service type registration:

1. **Remove fixed enum, use string-based types**:
   ```cpp
   class ServiceRegistry {
   public:
       template<typename T>
       void provide(const char* serviceName, T* service);
       
       template<typename T>
       T* request(const char* serviceName);
   };
   ```

2. **Use type-erased storage**:
   ```cpp
   struct ServiceEntry {
       const char* name;
       void* service;
       std::type_info* typeInfo;  // For runtime type checking
   };
   
   ServiceEntry services_[MAX_SERVICES] = {};
   ```

3. **Usage example**:
   ```cpp
   // Module provides a service
   ServiceRegistry::instance().provide<ISpeechProvider>("speech", &speech);
   
   // Module requests a service
   auto* speech = ServiceRegistry::instance().request<ISpeechProvider>("speech");
   ```

4. **Optional: Pre-register well-known services**:
   ```cpp
   namespace ServiceNames {
       constexpr const char* KEYBOARD = "keyboard";
       constexpr const char* CLIPBOARD = "clipboard";
       constexpr const char* NOTIFICATION = "notification";
       constexpr const char* DATABASE = "database";  // New services can be added
       constexpr const char* SCHEDULER = "scheduler";
   }
   ```

This removes the fixed enum and allows any service type to be registered dynamically.

## References
- Service Locator Pattern: Centralized service discovery
- Type Erasure: Store heterogeneous types uniformly
- Dependency Injection: Decouple service providers from consumers
