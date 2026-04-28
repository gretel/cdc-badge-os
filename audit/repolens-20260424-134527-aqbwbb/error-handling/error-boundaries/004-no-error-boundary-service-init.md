---
title: "[MEDIUM] No error boundary during service initialization"
severity: MEDIUM
domain: error-handling
lens: error-boundaries
labels:
  - "error-handling"
  - "service-lifecycle"
---

## Summary
The `ServiceRegistry::initAll()` and `ServiceRegistry::startAll()` functions in `components/cdc_core/src/ServiceRegistry.cpp` initialize services sequentially without error isolation. A single service failing to initialize can leave the system in a partially-initialized state with unclear error handling.

**Evidence:**
- `components/cdc_core/src/ServiceRegistry.cpp:130-147` (`initAll()`):
```cpp
bool ServiceRegistry::initAll() {
    LOG_I(TAG, "Initializing %u services...", count_);

    for (size_t i = 0; i < count_; i++) {
        LOG_I(TAG, "  [%u/%u] %s", i + 1, count_, services_[i].name);

        if (!services_[i].service->init()) {
            LOG_E(TAG, "Failed to initialize '%s'", services_[i].name);
            return false;  // Stops all initialization!
        }
    }

    LOG_I(TAG, "All services initialized");
    return true;
}
```

- `components/cdc_core/src/ServiceRegistry.cpp:153-168` (`startAll()`):
```cpp
bool ServiceRegistry::startAll() {
    LOG_I(TAG, "Starting %u services...", count_);

    for (size_t i = 0; i < count_; i++) {
        if (services_[i].service->getState() == ServiceState::INITIALIZED ||
            services_[i].service->getState() == ServiceState::STOPPED) {

            if (!services_[i].service->start()) {
                LOG_E(TAG, "Failed to start '%s'", services_[i].name);
                return false;  // Stops all start!
            }
        }
    }

    LOG_I(TAG, "All services started");
    return true;
}
```

- Called from `main/main.cpp:58-68` with no recovery:
```cpp
if (!EventBus::instance().init()) {
    // Can't log yet, USB not ready
    return;  // Silent failure!
}
```

## Impact
- **Partial initialization**: Services initialized before the failure have no coordinated shutdown
- **Silent failures**: Early boot failures may not be logged (before logging is ready)
- **No retry mechanism**: No attempt to recover from transient failures
- **Unclear state**: System may run with incomplete service set

## Recommended Fix
1. Add error boundaries and continue initialization for non-critical services:

```cpp
bool ServiceRegistry::initAll() {
    LOG_I(TAG, "Initializing %u services...", count_);
    bool allOk = true;

    for (size_t i = 0; i < count_; i++) {
        LOG_I(TAG, "  [%u/%u] %s", i + 1, count_, services_[i].name);

        try {
            if (!services_[i].service->init()) {
                LOG_E(TAG, "Failed to initialize '%s'", services_[i].name);
                allOk = false;
            }
        } catch (...) {
            LOG_E(TAG, "Exception initializing '%s'", services_[i].name);
            allOk = false;
        }
    }

    LOG_I(TAG, "Service initialization complete: %s", allOk ? "OK" : "with errors");
    return allOk;
}
```

2. Add error boundary in main boot sequence:
```cpp
// Stage 1: Core Services
try {
    if (!EventBus::instance().init()) {
        LOG_E(TAG, "EventBus init failed");
        return;
    }
} catch (...) {
    LOG_E(TAG, "EventBus init exception");
    return;
}
```

## References
- [Service Lifecycle Patterns](https://opencode.ai/guides/architecture/service-lifecycle/)
- [Boot Sequence Error Handling](https://martinfowler.com/articles/boot-sequence/)
