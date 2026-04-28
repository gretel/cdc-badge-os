---
title: "[MEDIUM] Feature flags create compile-time coupling between modules"
severity: MEDIUM
domain: architecture/coupling
lens: feature-flags
labels:
  - "audit:architecture/coupling"
---

## Summary
Feature flags in `components/cdc_core/include/cdc_core/feature_flags.h` control whether entire modules are compiled in. Modules check these flags in their initializer functions to decide whether to register. This creates compile-time coupling: changing a flag requires rebuilding all modules, and flags become a shared global state.

**Evidence:**
- `components/cdc_core/include/cdc_core/feature_flags.h` (lines 16-32): Feature flag definitions
- `components/mod_nvsedit/src/NvsEditModule.cpp` (line 590): Checks `FEATURE_NVS_EDIT` flag
- `components/grove_led/src/GroveLedModule.cpp` (line 636-650): Conditional registration
- `components/mod_hid/src/HidModule.cpp` (line 374): Conditional registration

## Impact
**Global state:** All modules check the same feature flags. If you want to disable `mod_nvsedit`, you must recompile with `-DFEATURE_NVS_EDIT=0`.

**Hidden dependencies:** A module might depend on another module being enabled. Example: `mod_hid` might need `mod_gpg` to be enabled for key storage.

**Testing complexity:** To test with different feature combinations, you must rebuild the entire firmware.

**Example fragility:**
```cpp
// mod_nvsedit/src/NvsEditModule.cpp (lines 590-595)
cdc::core::ModuleRegistry::instance().registerInitializer([]() {
    auto& moduleReg = cdc::core::ModuleRegistry::instance();
    
    #ifdef FEATURE_NVS_EDIT
    moduleReg.registerInitializer([]() {
        // Initialize NVS edit module
    });
    #endif
});
```

If `FEATURE_NVS_EDIT` is defined but `mod_nvsedit` is removed from `CMakeLists.txt`, the code compiles but nothing happens.

## Evidence
**File: `components/cdc_core/include/cdc_core/feature_flags.h` (lines 16-32)**
```cpp
// Secure Serial (require PIN for serial commands)
#ifdef CONFIG_SECURE_SERIAL
#define FEATURE_SECURE_SERIAL 1
#else
#ifndef FEATURE_SECURE_SERIAL
#define FEATURE_SECURE_SERIAL 0
#endif
#endif

// NVS Editor destructive actions (privileged tool)
#ifndef FEATURE_NVS_EDIT
#define FEATURE_NVS_EDIT 0
#endif

// Debug Mode (disables lockouts, useful for development)
#ifndef DEBUG_MODE
#define DEBUG_MODE 1
#endif
```

**File: `components/mod_nvsedit/src/NvsEditModule.cpp` (lines 590-600)**
```cpp
static void registerModule() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        auto& moduleReg = cdc::core::ModuleRegistry::instance();
        
        #ifdef FEATURE_NVS_EDIT
        moduleReg.registerModule(&NvsEditModule::instance());
        #endif
    });
}
```

**File: `components/grove_led/src/GroveLedModule.cpp` (lines 636-650)**
```cpp
static void initModule() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        auto& moduleReg = cdc::core::ModuleRegistry::instance();
        
        // Check if module is enabled in NVS
        if (moduleReg.isModuleEnabledByName("grove_led")) {
            moduleReg.registerModule(&GroveLedModule::instance());
        }
    });
}
```

## Recommended Fix
**Option 1: Runtime feature flags**
Move feature flags to runtime (stored in NVS). Modules check runtime flags instead of compile-time flags.

**Option 2: Module dependency graph**
Define module dependencies explicitly in each module's `getDependencies()` method. Module registry validates dependencies at boot.

**Option 3: Remove feature flags entirely**
Use CMake to include/exclude modules. If a module is in `CMakeLists.txt`, it's compiled in. If not, it's not. Simpler and clearer.

## References
- `components/cdc_core/include/cdc_core/feature_flags.h` - Feature flags
- `components/mod_nvsedit/src/NvsEditModule.cpp` - Example usage
- `main/CMakeLists.txt` - Module configuration
- `components/cdc_core/include/cdc_core/ModuleRegistry.h` - Module registry
