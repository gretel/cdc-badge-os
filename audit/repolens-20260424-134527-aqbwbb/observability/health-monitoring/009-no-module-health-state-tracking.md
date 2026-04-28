---
title: "[LOW] No module health state tracking or summary"
severity: LOW
domain: observability
lens: health-monitoring
labels:
  - "audit:observability/health-monitoring"
---

## Summary

The ModuleRegistry tracks module errors via `reportModuleError()` and publishes `MODULE_ERROR` events, but there's no mechanism to:
- Get a summary of all module health states
- Track module startup completion
- Report which modules are ready vs. initializing vs. failed

**Current error tracking:**
```cpp
// ModuleRegistry.cpp:685-695
void ModuleRegistry::reportModuleError(const char* name, const char* message) {
    uint8_t i = findModuleIndex(name);
    if (i != 255) {
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
```

But there's no `getModuleHealthSummary()` or `isModuleReady()` query method.

## Impact

- **No health summary**: Cannot quickly see which modules are healthy
- **Debugging difficulty**: Hard to diagnose module startup issues
- **No readiness check**: Cannot verify all modules are ready before declaring system ready
- **Lost information**: Only errors are tracked, not successful initialization

## Evidence

**File:** `components/cdc_core/include/cdc_core/ModuleRegistry.h` - No health query methods

**File:** `components/cdc_core/src/ModuleRegistry.cpp:685-695` - Error tracking exists but no health summary

**File:** `components/cdc_os_ui/src/AppUi.cpp` - Only subscribes to MODULE_ERROR events, no health polling:
```cpp
core::EventBus::instance().subscribe(onModuleErrorEvent, 
    static_cast<uint32_t>(core::EventType::MODULE_ERROR));
```

**File:** `components/cdc_core/include/cdc_core/IModule.h` - Module interface has `getState()` but no `isReady()` or `getHealth()` method

## Recommended Fix

Add module health state tracking:

1. **Add health state enum to IModule:**
   ```cpp
   class IModule {
       enum class HealthState {
           UNKNOWN,
           INITIALIZING,
           READY,
           DEGRADED,    // Partial functionality
           FAILED       // Needs attention
       };
       virtual HealthState getHealth() const = 0;
   };
   ```

2. **Add health summary method to ModuleRegistry:**
   ```cpp
   class ModuleRegistry {
       struct ModuleHealth {
           const char* name;
           IModule::HealthState health;
           const char* error;  // If any
       };
       
       uint8_t getModuleHealth(ModuleHealth* out_health, uint8_t max_health);
       bool isAllModulesReady();
   };
   ```

3. **Add MODULES command to show health:**
   ```cpp
   static void cmdModules(const char* args) {
       Console::printf("=== Module Health ===\r\n");
       ModuleRegistry::ModuleHealth health[16];
       uint8_t count = ModuleRegistry::instance().getModuleHealth(health, 16);
       for (uint8_t i = 0; i < count; i++) {
           Console::printf("%s: %s", health[i].name, 
               health[i].health == IModule::HealthState::READY ? "OK" : "WARN");
           if (health[i].error) {
               Console::printf(" (%s)", health[i].error);
           }
           Console::printf("\r\n");
       }
   }
   ```

4. **Update STATUS command to include module summary:**
   ```
   STATUS - Include "Modules: 5/5 ready"
   ```

## References

- Kubernetes pod health: https://kubernetes.io/docs/concepts/workloads/pods/pod-lifecycle/
- ESP32 task health monitoring: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html
