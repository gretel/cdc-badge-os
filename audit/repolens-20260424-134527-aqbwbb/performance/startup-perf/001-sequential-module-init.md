---
title: "[MEDIUM] Sequential module initialization blocks startup completion"
severity: MEDIUM
domain: startup-performance
lens: startup-perf
labels:
  - "audit:performance/startup-perf"
---

## Summary
All modules are initialized **sequentially** in `main.cpp:231` via `ModuleRegistry::runAllInitializers()`. Each module's `init()` and `start()` methods run to completion before the next module begins, preventing parallel initialization of independent hardware resources.

**Location:** `main/main.cpp:231`, `components/cdc_core/src/ModuleRegistry.cpp:45-58`

## Impact
- **Increased boot time**: Modules that could initialize in parallel (e.g., WiFi, Bluetooth, I2C devices) wait for previous modules to complete
- **No responsiveness during boot**: UI and user feedback are blocked until all modules are ready
- **Scalability issue**: Adding more modules linearly increases boot time

For example, if FIDO2 (ECC operations), TOTP (storage init), and GPG (key loading) all require TROPIC01 access, they currently serialize even though only the secure element session needs coordination.

## Evidence
From `main/main.cpp:226-234`:
```cpp
// === MODULE INITIALIZATION ===
// Register all modules (configured in main/CMakeLists.txt)
modules_register_all();

// Run all registered module initializers
LOG_I(TAG, "Initializing modules...");
cdc::core::ModuleRegistry::instance().runAllInitializers();
```

From `components/cdc_core/src/ModuleRegistry.cpp:45-58`:
```cpp
void ModuleRegistry::runAllInitializers() {
    LOG_I(TAG, "Running %d module initializers", initCount_);

    for (uint8_t i = 0; i < initCount_; i++) {
        if (initializers_[i]) {
            initializers_[i]();  // Executes synchronously
        }
    }
    // ... post-registration housekeeping
}
```

Each initializer calls `module.init()` then `module.start()` synchronously:
```cpp
// From mod_fido2_register() in Fido2Module.cpp:288-294
extern "C" void mod_fido2_register() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        auto& module = cdc::mod_fido2::Fido2Module::instance();
        if (module.init()) {
            module.start();  // Blocking call
        }
    });
}
```

## Recommended Fix
Implement **two-phase initialization** with parallel execution:

1. **Phase 1 (Parallel)**: All modules call `init()` only (prepare resources, don't start)
2. **Phase 2 (Parallel)**: All modules call `start()` only (activate resources)

Use FreeRTOS tasks or ESP-IDF async init to run independent initializers concurrently.

**Implementation steps:**
1. Add `registerDeferredInitializer()` to ModuleRegistry for Phase 2 (start) callbacks
2. Modify `modules_register_all()` to generate both init and start registration functions
3. In `main.cpp`, after all `init()` calls complete, start a worker task that runs all `start()` callbacks
4. Group modules by dependency (e.g., TROPIC01-dependent modules can run in parallel with USB-dependent modules)

**Alternative (simpler):** At minimum, split into two sequential phases:
```cpp
// Phase 1: Init all modules
modules_register_all();
ModuleRegistry::instance().initAll();  // New method

// Phase 2: Start all modules  
ModuleRegistry::instance().startAll();  // Existing method, already implemented
```

This requires no parallelism but allows better error handling and progress reporting.

## References
- ESP-IDF Programming Guide: [Initialization Order](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guide/idf.html#initialization-order)
- FreeRTOS: [Creating Tasks for Parallel Execution](https://www.freertos.org/a00111.html)
