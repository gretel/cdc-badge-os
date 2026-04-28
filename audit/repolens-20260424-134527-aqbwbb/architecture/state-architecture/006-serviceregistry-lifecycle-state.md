---
title: "[MEDIUM] ServiceRegistry uses static arrays for service storage with no lifecycle validation"
severity: MEDIUM
domain: State Management Architecture
lens: state-architecture
labels:
  - "audit:architecture/state-architecture"
---

## Summary
The `ServiceRegistry` singleton (`components/cdc_core/include/cdc_core/ServiceRegistry.h`) uses static arrays (`services_[MAX_SERVICES]`, `typedServices_[MAX_TYPED_SERVICES]`) to store service pointers. There is no validation that services are initialized before use, no lifecycle tracking, and no way to query service state.

### Evidence:
- `ServiceRegistry.h:124-127`: Static arrays `services_[MAX_SERVICES]` and `typedServices_[MAX_TYPED_SERVICES]`
- `ServiceRegistry.h:43-60`: `registerService()` and `get()` methods with no initialization validation
- `ServiceRegistry.h:70-88`: `provide()` and `request()` methods with no lifecycle state check
- `ServiceRegistry.h:95-108`: `initAll()` and `startAll()` methods exist but no `isInitialized()` check before use

### State Access Pattern:
```cpp
// Services registered without validation
ServiceRegistry::instance().registerService("display", &display);

// Services can be requested before initAll() is called
auto* display = ServiceRegistry::instance().get<IDisplay>("display");

// No way to check if service is initialized
```

## Impact
1. **Use-before-init bugs**: Services can be requested before `initAll()`/`startAll()` is called
2. **No lifecycle tracking**: No way to know if a service is initialized, started, or stopped
3. **Static array limits**: `MAX_SERVICES = 24` is hardcoded; no dynamic growth
4. **Duplicate registration**: `registerService()` returns `false` on duplicate but no error logging
5. **No state query**: No way to list all registered services for debugging

## Recommended Fix
1. Add service state enum:
   ```cpp
   enum class ServiceState {
       NOT_REGISTERED,
       REGISTERED,
       INITIALIZED,
       STARTED,
       STOPPED
   };
   
   typedef struct {
       const char* name;
       IService* service;
       ServiceState state;
   } ServiceEntry;
   ```

2. Add state query API:
   ```cpp
   ServiceState getServiceState(const char* name) const;
   ServiceEntry getServiceInfo(const char* name) const;
   uint8_t getRegisteredServiceCount() const;
   ```

3. Add lifecycle validation:
   ```cpp
   template<typename T>
   T* request(ServiceType type, bool checkInitialized = true) {
       if (checkInitialized && !isServiceInitialized(type)) {
           return nullptr;  // Or log warning
       }
       return static_cast<T*>(getTypedService(type));
   }
   ```

4. Add duplicate registration logging:
   ```cpp
   bool registerService(const char* name, IService* service) {
       if (getService(name)) {
           LOG_W("ServiceRegistry", "Duplicate registration: %s", name);
           return false;
       }
       // ... rest of logic
   }
   ```

## References
- `components/cdc_core/include/cdc_core/ServiceRegistry.h` - Full header
- `components/cdc_core/src/ServiceRegistry.cpp` - Implementation
- `main/app.cpp` - Usage pattern (check init order)
