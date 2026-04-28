---
title: "[MEDIUM] Missing audit events for module lifecycle and state transitions"
severity: MEDIUM
domain: observability
lens: audit-trail
labels:
  - "audit:observability/audit-trail"
---

## Summary

Module initialization, start, stop, and error events lack structured audit records. The `ModuleRegistry` logs these operations but without audit-specific context for tracking module lifecycle, state transitions, and error recovery.

**Files affected:**
- `components/cdc_core/src/ModuleRegistry.cpp:28-58` - `registerInitializer()`, `runAllInitializers()`
- `components/cdc_core/src/ModuleRegistry.cpp:65-90` - `registerModule()`
- `components/cdc_core/src/ModuleRegistry.cpp:141-150` - `initAll()`
- `components/cdc_core/src/ModuleRegistry.cpp:156-198` - `startAll()`, `startModule()`
- `components/cdc_core/src/ModuleRegistry.cpp:664-696` - `reportModuleError()`

## Impact

1. **Module Tracking**: Cannot reconstruct when modules were initialized, started, or stopped.
2. **Error Recovery**: No audit trail for module errors and retry attempts.
3. **State Transitions**: No record of module state machine transitions (UNINITIALIZED → INITIALIZED → STARTED).
4. **Debugging**: Hard to diagnose module startup failures or unexpected stops.

## Evidence

### Module registration lacks audit

In `ModuleRegistry.cpp:28-58`:
```cpp
void ModuleRegistry::runAllInitializers() {
    LOG_I(TAG, "Running %d module initializers", initCount_);

    for (uint8_t i = 0; i < initCount_; i++) {
        if (initializers_[i]) {
            initializers_[i]();
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

**Missing audit data:**
- No record of which modules were initialized
- No record of initialization success/failure per module
- No timestamp for boot sequence tracking
- No record of disabled module changes

### Module start lacks audit

In `ModuleRegistry.cpp:178-198`:
```cpp
bool ModuleRegistry::startModule(uint8_t index) {
    if (index >= count_) return false;
    IModule* module = modules_[index];
    if (!module) return false;

    if (hasModuleSlotError(index)) {
        LOG_E(TAG, "Module '%s' blocked: %s", module->getName(),
              getModuleSlotError(index) ? getModuleSlotError(index) : "slot map error");
        return false;
    }

    if (module->getState() == ServiceState::INITIALIZED ||
        module->getState() == ServiceState::STOPPED) {
        if (!module->start()) {
            LOG_E(TAG, "Failed to start module '%s'", module->getName());
            return false;
        }
    }

    return true;
}
```

**Missing audit data:**
- No record of successful module start
- No record of which state transition occurred
- No record of slot error conditions
- No session context (USB, boot, runtime?)

### Module error reporting lacks audit

In `ModuleRegistry.cpp:664-696`:
```cpp
void ModuleRegistry::reportModuleError(const char* name, const char* message) {
    if (!name) return;

    // Find module by name
    for (uint8_t i = 0; i < count_; i++) {
        if (strcmp(modules_[i]->getName(), name) == 0) {
            // Stop the module if it's running
            if (modules_[i]->getState() == ServiceState::STARTED) {
                modules_[i]->stop();
                LOG_W(TAG, "Module '%s' stopped due to error", name);
            }
            setModuleError(i, message);
            LOG_E(TAG, "Module '%s' error: %s", name, message ? message : "(null)");

            // Publish error event for UI notification
            Event evt;
            evt.type = EventType::MODULE_ERROR;
            evt.data.value = i;
            EventBus::instance().publish(evt);
            return;
        }
    }
    LOG_W(TAG, "reportModuleError: module '%s' not found", name);
}
```

**Missing audit data:**
- No structured record of error type
- No record of module state before error
- No record of automatic stop action
- No correlation to error recovery attempts

### Module enable/disable lacks audit

In `ModuleRegistry.cpp:551-608`:
```cpp
void ModuleRegistry::setModuleEnabled(uint8_t index, bool enabled) {
    if (index >= count_) return;

    const char* name = modules_[index]->getName();
    bool currentlyEnabled = isModuleEnabledByName(name);

    if (enabled == currentlyEnabled) return;  // No change needed

    if (enabled) {
        // Remove name from disabled list
        // ... code to update disabled list ...
    } else {
        // Add name to disabled list
        // ... code to update disabled list ...
    }

    saveDisabledList();
}
```

**Missing audit data:**
- No record of which module was enabled/disabled
- No record of who initiated the change
- No timestamp for tracking configuration changes

## Recommended Fix

### Step 1: Add module init/start/stop audit (15 min)

Update `ModuleRegistry.cpp`:

```cpp
// In runAllInitializers():
void ModuleRegistry::runAllInitializers() {
    LOG_I(TAG, "Running %d module initializers", initCount_);

    for (uint8_t i = 0; i < initCount_; i++) {
        if (initializers_[i]) {
            initializers_[i]();
            // Audit module initialization
            core::audit_record(core::AuditEventType::MODULE_INIT,
                              i, 0, 0, 0, 0);
        }
    }

    saveModuleList();
}

// In startModule():
bool ModuleRegistry::startModule(uint8_t index) {
    if (index >= count_) return false;
    IModule* module = modules_[index];
    if (!module) return false;

    if (hasModuleSlotError(index)) {
        core::audit_record(core::AuditEventType::MODULE_START,
                          index, 0, 1, 0, 0);  // 1 = failed
        LOG_E(TAG, "Module '%s' blocked: %s", module->getName(),
              getModuleSlotError(index) ? getModuleSlotError(index) : "slot map error");
        return false;
    }

    if (module->getState() == ServiceState::INITIALIZED ||
        module->getState() == ServiceState::STOPPED) {
        bool success = module->start();
        core::audit_record(core::AuditEventType::MODULE_START,
                          index, 0, success ? 0 : 1, 0, module->getState());
        if (!success) {
            LOG_E(TAG, "Failed to start module '%s'", module->getName());
            return false;
        }
    }

    return true;
}
```

### Step 2: Add module error audit (5 min)

Update `reportModuleError()`:
```cpp
void ModuleRegistry::reportModuleError(const char* name, const char* message) {
    if (!name) return;

    for (uint8_t i = 0; i < count_; i++) {
        if (strcmp(modules_[i]->getName(), name) == 0) {
            uint8_t oldState = modules_[i]->getState();
            if (modules_[i]->getState() == ServiceState::STARTED) {
                modules_[i]->stop();
            }
            setModuleError(i, message);

            // Audit error with before-state
            core::audit_record(core::AuditEventType::MODULE_EVENT,
                              i, 0, 1, 0, oldState);  // 1 = error

            Event evt;
            evt.type = EventType::MODULE_ERROR;
            evt.data.value = i;
            EventBus::instance().publish(evt);
            return;
        }
    }
}
```

### Step 3: Add module enable/disable audit (5 min)

Update `setModuleEnabled()`:
```cpp
void ModuleRegistry::setModuleEnabled(uint8_t index, bool enabled) {
    if (index >= count_) return;

    const char* name = modules_[index]->getName();
    bool currentlyEnabled = isModuleEnabledByName(name);

    if (enabled == currentlyEnabled) return;

    // ... existing code to update list ...

    saveDisabledList();

    // Audit configuration change
    core::audit_record(core::AuditEventType::CONFIG_CHANGE,
                      index, 0, 0, 0, enabled ? 1 : 2);  // 1 = enabled, 2 = disabled
}
```

## References

- Systemd Journal Format - structured logging for service management
- NIST SP 800-92 - Guide to Computer Security Log Management
- Common Criteria EAL2+ - FAU_GEN.1 audit data generation
