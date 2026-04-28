---
title: "[MEDIUM] No error boundary around module initializers"
severity: MEDIUM
domain: error-handling
lens: error-boundaries
labels:
  - "error-handling"
  - "module-lifecycle"
---

## Summary
The `ModuleRegistry::runAllInitializers()` function in `components/cdc_core/src/ModuleRegistry.cpp:41-59` executes all module initializers without error isolation. A single module initializer throwing or failing can prevent subsequent modules from initializing.

**Evidence:**
- `components/cdc_core/src/ModuleRegistry.cpp:41-59`:
```cpp
void ModuleRegistry::runAllInitializers() {
    LOG_I(TAG, "Running %d module initializers", initCount_);

    for (uint8_t i = 0; i < initCount_; i++) {
        if (initializers_[i]) {
            initializers_[i]();  // No error boundary!
        }
    }

    // Load disabled modules list from NVS
    loadDisabledList();

    // After all modules are registered, clean up orphaned NVS data
    cleanupOrphanedModuleData();

    // Save current module list for next boot
    saveModuleList();
}
```

- Called from `main/main.cpp:225`:
```cpp
// Run all registered module initializers
LOG_I(TAG, "Initializing modules...");
cdc::core::ModuleRegistry::instance().runAllInitializers();
```

- Module initializer pattern (from `components/mod_totp/src/TotpModule.cpp:1013-1020`):
```cpp
extern "C" void mod_totp_register() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        auto& module = cdc::mod_totp::TotpModule::instance();
        if (module.init()) {
            module.start();
        }
        // No error handling if init() throws!
    });
}
```

## Impact
- **Cascade failure**: One module can prevent all subsequent modules from loading
- **Partial feature set**: User gets incomplete functionality with no clear indication
- **Debug difficulty**: Hard to identify which module failed during boot
- **No recovery**: Failed modules not retried on next boot

## Recommended Fix
Add error boundaries around each initializer:

```cpp
void ModuleRegistry::runAllInitializers() {
    LOG_I(TAG, "Running %d module initializers", initCount_);

    for (uint8_t i = 0; i < initCount_; i++) {
        if (initializers_[i]) {
            try {
                initializers_[i]();
            } catch (const std::exception& e) {
                // Extract module name from initializer (would need tracking)
                LOG_E(TAG, "Module initializer exception: %s", e.what());
            } catch (...) {
                LOG_E(TAG, "Module initializer exception (unknown)");
            }
        }
    }

    loadDisabledList();
    cleanupOrphanedModuleData();
    saveModuleList();
}
```

Also add error handling in module initializers:
```cpp
extern "C" void mod_totp_register() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        try {
            auto& module = cdc::mod_totp::TotpModule::instance();
            if (module.init()) {
                module.start();
            }
        } catch (const std::exception& e) {
            LOG_E(TAG, "TOTP module init exception: %s", e.what());
        } catch (...) {
            LOG_E(TAG, "TOTP module init exception (unknown)");
        }
    });
}
```

## References
- [Module Lifecycle Patterns](https://opencode.ai/guides/architecture/module-lifecycle/)
- [Plugin Initialization Best Practices](https://martinfowler.com/articles/plugin-pattern/)
