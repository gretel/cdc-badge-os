---
title: "[MEDIUM] No MODULE_STARTED event for module lifecycle tracking"
severity: MEDIUM
domain: observability
lens: health-monitoring
labels:
  - "audit:observability/health-monitoring"
---

## Summary

The system publishes `MODULE_ERROR` events when modules fail, but **no event is published when a module successfully starts**. This creates a blind spot in module lifecycle observability.

**Current behavior:**
- `ModuleRegistry::reportModuleError()` publishes `MODULE_ERROR` events (in `components/cdc_core/src/ModuleRegistry.cpp:685-695`)
- `ModuleRegistry::startModule()` starts modules silently - no success event (in `components/cdc_core/src/ModuleRegistry.cpp:178-198`)
- Only errors are visible to subscribers; successful initialization is opaque

**Missing:**
- No `MODULE_STARTED` event when `module->start()` succeeds
- No `MODULE_INITIALIZED` event when `module->init()` succeeds
- No way for external systems to know when modules become ready

## Impact

1. **Incomplete health picture**: Subscribers only see failures, not successes
2. **Cannot track module readiness**: No way to know when all modules are initialized and started
3. **Debugging difficulty**: Cannot trace module startup sequence in logs/events
4. **No event-driven initialization**: External components cannot react to module availability
5. **Monitoring gap**: Health dashboards must poll instead of receiving push notifications

## Evidence

**EventBus enum** (`components/cdc_core/include/cdc_core/EventBus.h:11-45`):
```cpp
enum class EventType : uint8_t {
    // ...
    MODULE_EVENT,       // Generic module event
    MODULE_ERROR,       // Only error event exists
    EVENT_COUNT
};
```

**Module error reporting** (`components/cdc_core/src/ModuleRegistry.cpp:685-695`):
```cpp
void ModuleRegistry::reportModuleError(const char* name, const char* message) {
    uint8_t i = findModuleIndex(name);
    if (i != 255) {
        setModuleError(i, message);
        LOG_E(TAG, "Module '%s' error: %s", name, message ? message : "(null)");

        // Publish error event for UI notification
        Event evt;
        evt.type = EventType::MODULE_ERROR;
        evt.data.value = i;
        EventBus::instance().publish(evt);  // Only error published
        return;
    }
}
```

**Module start** (`components/cdc_core/src/ModuleRegistry.cpp:178-198`):
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

    return true;  // No event published on success!
}
```

**Event subscription** (`components/cdc_os_ui/src/AppUi.cpp:688`):
```cpp
core::EventBus::instance().subscribe(onModuleErrorEvent, 
    static_cast<uint32_t>(core::EventType::MODULE_ERROR));
```
Only subscribes to errors, no subscription for success events because they don't exist.

## Recommended Fix

Add `MODULE_STARTED` and `MODULE_INITIALIZED` events:

**Step 1: Add event types** (`components/cdc_core/include/cdc_core/EventBus.h`):
```cpp
enum class EventType : uint8_t {
    // ...
    MODULE_ERROR,       // Module failed (existing)
    MODULE_INITIALIZED, // Module initialized successfully (new)
    MODULE_STARTED,     // Module started successfully (new)
    EVENT_COUNT
};
```

**Step 2: Publish event on module start** (`components/cdc_core/src/ModuleRegistry.cpp:178-198`):
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

    // Publish success event
    Event evt;
    evt.type = EventType::MODULE_STARTED;
    evt.data.value = index;
    EventBus::instance().publish(evt);

    return true;
}
```

**Step 3: Publish event on module init** (add to `initModule` function):
```cpp
bool ModuleRegistry::initModule(uint8_t index) {
    // ... existing init logic ...
    
    // After successful init
    Event evt;
    evt.type = EventType::MODULE_INITIALIZED;
    evt.data.value = index;
    EventBus::instance().publish(evt);
    
    return true;
}
```

**Step 4: Update subscribers** (optional - for UI or monitoring):
```cpp
// AppUi.cpp or similar
void onModuleStartedEvent(const core::Event& evt) {
    if (evt.type != core::EventType::MODULE_STARTED) return;
    
    auto& moduleReg = core::ModuleRegistry::instance();
    uint8_t index = static_cast<uint8_t>(evt.data.value);
    core::IModule* module = moduleReg.getModuleAt(index);
    const char* name = module ? module->getName() : "?";
    
    LOG_I("MODULE", "Module started: %s", name);
    // Could show toast, update status, etc.
}

// Subscribe in init
core::EventBus::instance().subscribe(onModuleStartedEvent, 
    static_cast<uint32_t>(core::EventType::MODULE_STARTED));
```

**Expected event flow:**
```
BOOT → MODULE_INITIALIZED (gpg) → MODULE_INITIALIZED (fido2) → 
MODULE_STARTED (gpg) → MODULE_STARTED (fido2) → SYSTEM_READY
```

## References

- Kubernetes pod lifecycle: https://kubernetes.io/docs/concepts/workloads/pods/pod-lifecycle/
- Event-driven architecture patterns: https://www.enterpriseintegrationpatterns.com/patterns/messaging/
- ESP32 FreeRTOS event groups: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html#event-groups

</content>