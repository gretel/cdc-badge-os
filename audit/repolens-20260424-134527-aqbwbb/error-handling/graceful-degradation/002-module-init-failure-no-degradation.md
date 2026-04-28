---
title: "[MEDIUM] Module Initialization Failure Stops Entire Module with No Degraded Mode"
severity: MEDIUM
domain: module-system
lens: graceful-degradation
labels:
  - "audit:error-handling/graceful-degradation"
---

## Summary
In `components/cdc_core/src/ModuleRegistry.cpp`, when a module's slot validation fails (e.g., `applySlotRequest()` at line 880-910), the module is marked as error and blocked entirely. The module cannot function at all even if partial operation would be possible (e.g., TOTP module could still display cached codes without new writes). Similarly, in `TotpModule.cpp:929-945`, if slot range assignment fails, the entire module is marked as error with no fallback.

Lines of interest:
- `ModuleRegistry.cpp:880-910` - `applySlotRequest()` marks module error on slot validation failure
- `ModuleRegistry.cpp:671-696` - `reportModuleError()` stops module and marks it as failed
- `TotpModule.cpp:929-945` - TOTP module fails entirely if slot range missing

## Impact
When a module fails initialization:
1. **All module functionality is lost** - even features that don't require the failed resource
2. **No partial data access** - e.g., TOTP could display cached codes even if new entries can't be added
3. **User experience degradation** - entire feature appears broken instead of showing "limited mode"
4. **No graceful fallback** - e.g., display last-known-good data from R-Memory cache if secure element unavailable

## Evidence
```cpp
// ModuleRegistry.cpp:880-910
bool ModuleRegistry::applySlotRequest(IModule* module, uint8_t index) {
    // ...
    if (!validateSlotMap(moduleName)) {
        return false;  // Module blocked entirely
    }
    // ...
    if (!validateEccRange(mapName, moduleName, req.minEccSlots, range, moduleId)) {
        return false;  // Module blocked entirely
    }
    // ...
}

// ModuleRegistry.cpp:671-696
void ModuleRegistry::reportModuleError(const char* name, const char* message) {
    // ...
    if (modules_[i]->getState() == ServiceState::STARTED) {
        modules_[i]->stop();
        LOG_W(TAG, "Module '%s' stopped due to error", name);
    }
    setModuleError(i, message);  // Module is now blocked
    // ...
}

// TotpModule.cpp:929-945
bool TotpModule::init() {
    // ...
    if (slotRange_.hasRmem) {
        TotpStore::instance().setSlotRange(slotRange_.rmemStart, slotRange_.rmemEnd, slotRange_.moduleId);
        core::ModuleRegistry::instance().clearModuleErrorByName(getName());
    } else {
        core::ModuleRegistry::instance().reportModuleError(getName(), "TOTP slot range missing");
        state_ = core::ServiceState::ERROR;
        return false;  // Entire module fails
    }
    // ...
}
```

## Recommended Fix
Implement degraded mode support for modules:

1. **Add degraded state to module lifecycle**:
   ```cpp
   enum class ServiceState {
       UNINITIALIZED,
       INITIALIZED,
       STARTED,
       STOPPED,
       DEGRADED,  // New: partial functionality
       ERROR      // Full failure
   };
   ```

2. **Allow modules to operate in degraded mode**:
   ```cpp
   // TotpModule.cpp
   bool TotpModule::init() {
       // ...
       if (slotRange_.hasRmem) {
           TotpStore::instance().setSlotRange(slotRange_.rmemStart, slotRange_.rmemEnd, slotRange_.moduleId);
       } else {
           // Try degraded mode with cached data only
           LOG_W(TAG, "Slot range missing - entering degraded mode");
           state_ = core::ServiceState::DEGRADED;
           // Still register module, but mark as degraded
       }
       // ...
   }
   ```

3. **Add degraded mode check to module operations**:
   ```cpp
   // In module methods, check state before full operations
   if (getState() == ServiceState::DEGRADED) {
       // Use cached data, skip write operations
       return getFromCache();
   }
   ```

4. **Display degraded mode status in UI** - show "Limited Mode" indicator instead of hiding module entirely.

## References
- [Module Architecture Documentation](/input/20260423-132359-oj8ayc/cdc-badge-os/CLAUDE.md#module-architecture)
- [Graceful Degradation in Embedded Systems](https://www.embedded.com/design/prototyping-and-development/4024616/Graceful-degradation-in-embedded-systems)
