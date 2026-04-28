---
title: "[LOW] Silent failure in module initializer registration when registry is full"
severity: LOW
domain: core-services
lens: error-handling
labels:
  - "audit:error-handling/unhandled-errors"
---

## Summary
In `components/cdc_core/src/ModuleRegistry.cpp`, the `registerInitializer()` function silently returns when the initializer registry is full (line 33-35). Callers have no way to know if their initializer was registered or dropped.

## Impact
If more than `MAX_INITIALIZERS` modules are registered:
- Later modules silently fail to initialize
- No error is logged (only "Module initializer registry full" at debug level)
- Modules may start without proper setup, causing runtime errors
- Hard to debug since the registration appears to succeed

## Evidence
File: `components/cdc_core/src/ModuleRegistry.cpp:28-37`

```cpp
/**
 * \brief Registers a deferred module initializer callback.
 * \param initFunc Initializer function to execute during startup.
 */
void ModuleRegistry::registerInitializer(ModuleInitFunc initFunc) {
    if (!initFunc) return;

    if (initCount_ >= MAX_INITIALIZERS) {
        LOG_E(TAG, "Module initializer registry full");
        return;  // <-- Silent failure, caller has no way to know
    }

    initializers_[initCount_++] = initFunc;
}
```

The function returns `void`, so callers cannot check if registration succeeded.

## Recommended Fix
Change the function to return a boolean indicating success:

```cpp
/**
 * \brief Registers a deferred module initializer callback.
 * \param initFunc Initializer function to execute during startup.
 * \return `true` if registration succeeded, `false` if registry is full.
 */
bool ModuleRegistry::registerInitializer(ModuleInitFunc initFunc) {
    if (!initFunc) return false;

    if (initCount_ >= MAX_INITIALIZERS) {
        LOG_E(TAG, "Module initializer registry full");
        return false;
    }

    initializers_[initCount_++] = initFunc;
    return true;
}
```

Then update callers to check the return value:
```cpp
if (!ModuleRegistry::instance().registerInitializer(initModuleGpg)) {
    LOG_E(TAG, "Failed to register GPG module initializer");
}
```

## References
- C++ Core Guidelines: F.4 - If a function can fail, it should return a status
- Module architecture documentation in CLAUDE.md
