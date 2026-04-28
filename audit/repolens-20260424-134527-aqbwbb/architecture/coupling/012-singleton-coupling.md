---
title: "[MEDIUM] EventBus and ServiceRegistry singleton coupling"
severity: MEDIUM
domain: architecture/coupling
lens: coupling-analysis
labels:
  - "singleton-coupling"
  - "global-state"
---

## Summary
The core architecture uses singleton pattern for `EventBus` and `ServiceRegistry`, creating implicit global state that modules depend on. While convenient, this creates tight coupling where modules cannot be easily tested or used without the singleton instances being available.

**Evidence:**
- `components/cdc_core/src/EventBus.cpp:17-20`: `EventBus::instance()` uses static local singleton
- `components/cdc_core/src/ServiceRegistry.cpp:26-29`: `ServiceRegistry::instance()` uses static local singleton
- Modules call these directly without dependency injection (e.g., `EventBus::instance().publish()` in various places)

## Impact
1. **Testing difficulty**: Unit tests must initialize singletons before testing modules
2. **Hidden dependencies**: Modules depend on global state not visible in their interfaces
3. **Initialization order coupling**: Singletons must be initialized before modules use them
4. **Hard to swap implementations**: Cannot easily mock or replace for testing
5. **Lifecycle management**: Singletons live for entire program lifetime, cannot be cleaned up

## Evidence
File: `components/cdc_core/src/EventBus.cpp`
```cpp
// Lines 17-20: Singleton pattern
EventBus& EventBus::instance() {
    static EventBus instance;
    return instance;
}
```

File: `components/cdc_core/src/ServiceRegistry.cpp`
```cpp
// Lines 26-29: Singleton pattern
ServiceRegistry& ServiceRegistry::instance() {
    static ServiceRegistry instance;
    return instance;
}
```

Usage pattern (e.g., `main/main.cpp:242`):
```cpp
// Direct singleton access in main loop
EventBus::instance().process();
cdc::core::ModuleRegistry::instance().dispatchTick(nowMs);
```

## Recommended Fix
1. **Dependency injection for testability**:
   ```cpp
   class GroveLedModule {
   public:
       void setEventBus(core::EventBus* bus);  // Optional injection
       void setServiceRegistry(core::ServiceRegistry* registry);
   };
   ```

2. **Use interface instead of concrete singleton**:
   ```cpp
   class IEventBus {
   public:
       virtual ~IEventBus() = default;
       virtual void publish(const Event& event) = 0;
   };
   
   // In module
   void setEventBus(IEventBus* bus);
   ```

3. **Initialize singletons early**: Document in `main.cpp` that singletons are initialized in Stage 1, before modules

4. **Add null-safety**: Allow modules to work without event bus (graceful degradation)

5. **Consider constructor injection**: For modules that heavily depend on services, inject in constructor

## References
- Singleton implementation: `components/cdc_core/src/EventBus.cpp`, `components/cdc_core/src/ServiceRegistry.cpp`
- Module interface: `components/cdc_core/include/cdc_core/IModule.h`
- Initialization order: `main/main.cpp:54-235`
