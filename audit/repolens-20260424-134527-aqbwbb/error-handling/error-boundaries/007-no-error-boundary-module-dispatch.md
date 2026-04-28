---
title: "[MEDIUM] No error boundary in module lifecycle event dispatch"
severity: MEDIUM
domain: error-handling
lens: error-boundaries
labels:
  - "error-handling"
  - "module-lifecycle"
---

## Summary
The `ModuleRegistry` dispatches lifecycle events (unlock, lock, USB connect/disconnect) to all modules without error isolation. A single module throwing during these callbacks will stop dispatch to remaining modules.

**Evidence:**
- `components/cdc_core/src/ModuleRegistry.cpp:259-299`:
```cpp
void ModuleRegistry::dispatchUnlock() {
    for (uint8_t i = 0; i < count_; i++) {
        if (modules_[i]->getState() == ServiceState::STARTED) {
            modules_[i]->onUnlock();  // No error boundary!
        }
    }
}

void ModuleRegistry::dispatchLock() {
    for (uint8_t i = 0; i < count_; i++) {
        if (modules_[i]->getState() == ServiceState::STARTED) {
            modules_[i]->onLock();  // No error boundary!
        }
    }
}

void ModuleRegistry::dispatchUsbConnect() {
    for (uint8_t i = 0; i < count_; i++) {
        if (modules_[i]->getState() == ServiceState::STARTED) {
            modules_[i]->onUsbConnect();  // No error boundary!
        }
    }
}

void ModuleRegistry::dispatchUsbDisconnect() {
    for (uint8_t i = 0; i < count_; i++) {
        if (modules_[i]->getState() == ServiceState::STARTED) {
            modules_[i]->onUsbDisconnect();  // No error boundary!
        }
    }
}
```

## Impact
- **Partial lifecycle**: Some modules receive unlock/USB events, others don't
- **State inconsistency**: Modules may have different states after event
- **No error visibility**: Failed callbacks not logged

## Recommended Fix
Add error boundaries to all lifecycle dispatch methods:

```cpp
void ModuleRegistry::dispatchUnlock() {
    for (uint8_t i = 0; i < count_; i++) {
        if (modules_[i]->getState() == ServiceState::STARTED) {
            try {
                modules_[i]->onUnlock();
            } catch (const std::exception& e) {
                LOG_E(TAG, "Module '%s' onUnlock exception: %s",
                      modules_[i]->getName(), e.what());
            } catch (...) {
                LOG_E(TAG, "Module '%s' onUnlock exception (unknown)",
                      modules_[i]->getName());
            }
        }
    }
}

void ModuleRegistry::dispatchUsbConnect() {
    for (uint8_t i = 0; i < count_; i++) {
        if (modules_[i]->getState() == ServiceState::STARTED) {
            try {
                modules_[i]->onUsbConnect();
            } catch (const std::exception& e) {
                LOG_E(TAG, "Module '%s' onUsbConnect exception: %s",
                      modules_[i]->getName(), e.what());
            } catch (...) {
                LOG_E(TAG, "Module '%s' onUsbConnect exception (unknown)",
                      modules_[i]->getName());
            }
        }
    }
}

// Apply same pattern to dispatchLock() and dispatchUsbDisconnect()
```

## References
- [Lifecycle Event Patterns](https://opencode.ai/guides/architecture/lifecycle-events/)
- [Observer Pattern Best Practices](https://en.cppreference.com/w/cpp/language/derived_class)
