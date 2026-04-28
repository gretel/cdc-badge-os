---
title: "[LOW] Module Initialization Order is Non-Deterministic"
severity: LOW
domain: extensibility
lens: module-initialization
labels:
  - "audit:architecture/extensibility"
---

## Summary
Modules are initialized in the order they are registered via `registerInitializer()`, which depends on CMake build order in `main/CMakeLists.txt`. Modules that depend on other modules (e.g., a module needing `IKeyboardProvider`) may initialize before their dependencies are ready.

**Evidence:**
- `main/CMakeLists.txt`: Module order determined by `MODULES` list:
  ```cmake
  set(MODULES
      mod_gpg
      mod_fido2
      mod_totp
      mod_password
      # ...
  )
  ```

- `components/cdc_core/src/ModuleRegistry.cpp:40-55`:
  ```cpp
  void ModuleRegistry::runAllInitializers() {
      for (uint8_t i = 0; i < initCount_; i++) {
          if (initializers_[i]) {
              initializers_[i]();  // Called in registration order
          }
      }
  }
  ```

- `components/mod_gpg/src/GpgModule.cpp:655-661`:
  ```cpp
  extern "C" void mod_gpg_register() {
      cdc::core::ModuleRegistry::instance().registerInitializer([]() {
          auto& module = cdc::mod_gpg::GpgModule::instance();
          if (module.init()) {
              module.start();  // May request services before they're ready
          }
      });
  }
  ```

## Impact
**Race Conditions:** If module A depends on module B's service:
1. Module A may initialize before module B registers its service
2. `ServiceRegistry::request<T>()` returns `nullptr`
3. Module A fails silently or crashes

Example: A module that needs `IKeyboardProvider` might initialize before `mod_hid` registers it.

## Evidence
Files affected:
- `main/CMakeLists.txt` (module order)
- `components/cdc_core/include/cdc_core/ModuleRegistry.h` (initializer storage)
- `components/cdc_core/src/ModuleRegistry.cpp` (initialization loop)
- `components/mod_gpg/src/GpgModule.cpp` (module registration)

Module registration in `components/mod_totp/src/TotpModule.cpp:930-940`:
```cpp
bool TotpModule::init() {
    // Module logic here
    core::ModuleRegistry::instance().registerModule(this);
    // ...
}
```

Module starts immediately after registration - no dependency check.

## Recommended Fix
Implement dependency-aware initialization:

1. **Add dependency declaration:**
   ```cpp
   class IModule {
   public:
       // Add method to declare dependencies
       virtual std::vector<ServiceType> getDependencies() { return {}; }
   };
   ```

2. **Topological sort before initialization:**
   ```cpp
   void ModuleRegistry::runAllInitializers() {
       // Build dependency graph
       // Topologically sort modules
       // Initialize in dependency order
   }
   ```

3. **Or two-phase initialization:**
   ```cpp
   // Phase 1: Register all modules (init only)
   for (auto& m : modules_) m->init();
   
   // Phase 2: Start all modules (services ready)
   for (auto& m : modules_) m->start();
   ```

4. **Or lazy service lookup:**
   ```cpp
   // Modules check for services in start() not init()
   bool GpgModule::start() {
       auto* kb = ServiceRegistry::instance().request<IKeyboardProvider>(ServiceType::KEYBOARD);
       if (!kb) {
           LOG_W(TAG, "Keyboard provider not ready, retrying...");
           return false;  // Will be retried
       }
       // ...
   }
   ```

## References
- Dependency injection patterns
- Two-phase construction
- Topological sorting for initialization order
