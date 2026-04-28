---
title: "[MEDIUM] ServiceRegistry Typed Services Use Fixed Enum"
severity: MEDIUM
domain: extensibility
lens: service-discovery
labels:
  - "audit:architecture/extensibility"
---

## Summary
The `ServiceType` enum in `components/cdc_core/include/cdc_core/ServiceRegistry.h:11-16` defines a fixed list of typed services (KEYBOARD, CLIPBOARD, NOTIFICATION). Adding new typed services requires modifying the core enum.

**Evidence:**
- `components/cdc_core/include/cdc_core/ServiceRegistry.h:11-16`:
  ```cpp
  enum class ServiceType {
      KEYBOARD,      // IKeyboardProvider - keyboard input (BLE HID, USB HID, etc.)
      CLIPBOARD,     // Future: clipboard access
      NOTIFICATION,  // Future: push notifications
  };
  ```

- `components/cdc_core/src/ServiceRegistry.cpp:14-20`: Switch statement for service type names:
  ```cpp
  static const char* serviceTypeName(ServiceType type) {
      switch (type) {
          case ServiceType::KEYBOARD:     return "keyboard";
          case ServiceType::CLIPBOARD:    return "clipboard";
          case ServiceType::NOTIFICATION: return "notification";
          default:                        return "unknown";
      }
  }
  ```

- `components/cdc_core/include/cdc_core/ServiceRegistry.h:75-87`: Template methods use enum:
  ```cpp
  template<typename T>
  bool provide(ServiceType type, T* service) { ... }
  
  template<typename T>
  T* request(ServiceType type) { ... }
  ```

## Impact
**Limited Service Discovery:** New service types (e.g., `TIME_PROVIDER`, `STORAGE_PROVIDER`) require:
1. Modifying the `ServiceType` enum in core
2. Updating the `serviceTypeName()` switch statement
3. Potentially increasing `MAX_TYPED_SERVICES`

The enum-based approach doesn't scale well for a modular system where modules should discover services dynamically.

## Evidence
Files affected:
- `components/cdc_core/include/cdc_core/ServiceRegistry.h:11-16` (enum definition)
- `components/cdc_core/src/ServiceRegistry.cpp:14-20` (switch for names)
- `components/cdc_core/include/cdc_core/ServiceRegistry.h:132-136` (typed services storage)

Storage limitation in `ServiceRegistry.h:132-136`:
```cpp
static constexpr size_t MAX_TYPED_SERVICES = 8;
void* typedServices_[MAX_TYPED_SERVICES] = {};
```
This limits to 8 service types based on enum values (assumes enum fits in 0-7 range).

## Recommended Fix
Implement a map-based service registry:

1. **Use type_id or string keys:**
   ```cpp
   class ServiceRegistry {
   private:
       struct TypedService {
           const char* key;  // "keyboard", "time_provider", etc.
           void* service;
       };
       TypedService typedServices_[MAX_TYPED_SERVICES];
   public:
       template<typename T>
       bool provide(const char* key, T* service) {
           return registerTypedService(key, service);
       }
       
       template<typename T>
       T* request(const char* key) {
           return static_cast<T*>(getTypedService(key));
       }
   };
   ```

2. **Modules define their own service keys:**
   ```cpp
   // In module header
   static constexpr const char* SERVICE_TIME_PROVIDER = "time_provider";
   
   // In module init
   registry.provide<ITimeProvider>(SERVICE_TIME_PROVIDER, &timeProvider);
   ```

3. **Or use type-based lookup:**
   ```cpp
   template<typename T>
   bool provide(T* service) {
       const char* key = typeid(T).name();  // Or custom trait
       return registerTypedService(key, service);
   }
   ```

## References
- Service Locator Pattern
- Dependency Injection
- Type erasure for heterogeneous storage
