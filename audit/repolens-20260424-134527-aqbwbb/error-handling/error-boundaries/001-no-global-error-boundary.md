---
title: "[HIGH] No global error boundary around module tick dispatch"
severity: HIGH
domain: error-handling
lens: error-boundaries
labels:
  - "error-handling"
  - "fault-isolation"
---

## Summary
The main loop in `main/main.cpp:240-254` dispatches ticks to all modules via `ModuleRegistry::dispatchTick()`. A single module throwing an exception or crashing during its `onTick()` handler will bring down the entire system with no recovery mechanism.

**Evidence:**
- `main/main.cpp:247`: `cdc::core::ModuleRegistry::instance().dispatchTick(nowMs);`
- `components/cdc_core/src/ModuleRegistry.cpp:305-311`: `dispatchTick()` iterates through all modules without try-catch or error recovery:
```cpp
void ModuleRegistry::dispatchTick(uint32_t nowMs) {
    for (uint8_t i = 0; i < count_; i++) {
        if (modules_[i]->getState() == ServiceState::STARTED) {
            modules_[i]->onTick(nowMs);  // No error boundary!
        }
    }
}
```

## Impact
- **Cascading failure**: One buggy module (e.g., TOTP, Password) can crash the entire badge
- **Poor fault isolation**: Modules are meant to be independent but share a single failure domain
- **Debug difficulty**: No error capture or logging when a module crashes during tick
- **User experience**: Single module error = total device reboot

## Recommended Fix
Add individual error boundaries around each module's `onTick()` call in `ModuleRegistry::dispatchTick()`:

```cpp
void ModuleRegistry::dispatchTick(uint32_t nowMs) {
    for (uint8_t i = 0; i < count_; i++) {
        if (modules_[i]->getState() == ServiceState::STARTED) {
            try {
                modules_[i]->onTick(nowMs);
            } catch (const std::exception& e) {
                LOG_E(TAG, "Module '%s' tick exception: %s",
                      modules_[i]->getName(), e.what());
                reportModuleError(modules_[i]->getName(), e.what());
            } catch (...) {
                LOG_E(TAG, "Module '%s' tick exception (unknown)",
                      modules_[i]->getName());
                reportModuleError(modules_[i]->getName(), "Unknown exception");
            }
        }
    }
}
```

## References
- [Error Boundary Architecture](https://opencode.ai/guides/error-handling/error-boundaries/)
- [C++ Exception Handling Best Practices](https://en.cppreference.com/w/cpp/language/try)
