---
title: "[MEDIUM] Module initialization failures are logged but system continues"
severity: MEDIUM
domain: error-path-tests
lens: module-registraton
labels:
  - "module-lifecycle"
  - "error-handling"
  - "startup"
---

## Summary
In `components/cdc_core/src/ModuleRegistry.cpp` (lines 142-149), when a module's `initialize()` or `start()` method fails, the error is logged but the system continues booting. This can lead to modules being in an inconsistent state, with dependent modules failing later.

**Files:**
- `components/cdc_core/src/ModuleRegistry.cpp:142-149`
- `components/cdc_core/src/ModuleRegistry.cpp:178-198`

## Impact
1. **Cascading Failures**: Module A fails to initialize, Module B (which depends on A) fails later with confusing error
2. **Partial Functionality**: System appears to work but some features are silently broken
3. **Resource Leaks**: Failed modules may have partially allocated resources
4. **Debug Difficulty**: Hard to trace which module failed first
5. **Inconsistent State**: Slot allocation may be inconsistent after partial init

## Evidence
From `components/cdc_core/src/ModuleRegistry.cpp`:

```cpp
// Lines 142-149: initAll() - continues on module failure
void initAll() {
    for (int i = 0; i < moduleCount; i++) {
        if (modules[i] && modules[i]->init() != Module::State::INITIALIZED) {
            LOG_W("ModuleRegistry", "Module %s init failed", modules[i]->getName());
            // ❌ Continues to next module!
            // No error propagation, no state tracking
        }
    }
}

// Lines 178-198: startModule() - similar issue
Module::State startModule(int slot) {
    if (slot < 0 || slot >= moduleCount || !modules[slot]) {
        LOG_E("ModuleRegistry", "Invalid slot %d", slot);
        return Module::State::ERROR;
    }

    Module::State state = modules[slot]->start();
    if (state != Module::State::RUNNING) {
        LOG_W("ModuleRegistry", "Module %s start failed with state %d", 
              modules[slot]->getName(), state);
        // ❌ Returns state but doesn't stop other modules
        // ❌ Doesn't clean up partial initialization
        return state;
    }

    return Module::State::RUNNING;
}

// Lines 664-696: reportModuleError() - called but may be too late
void reportModuleError(int slot, Module::State errorState) {
    modules[slot]->setState(Module::State::ERROR);
    EventBus::getInstance()->publish("module:error", {
        {"slot", slot},
        {"state", errorState}
    });
    // Module is stopped but no recovery logic
}
```

**Problem:**
- `initAll()` logs warning but continues booting
- `startModule()` returns error state but caller may not check
- No mechanism to stop system if critical module fails
- No cleanup of partially initialized modules

## Recommended Fix
Implement proper module initialization error handling:

1. **Track initialization failures**:
   ```cpp
   class ModuleRegistry {
   private:
       int initFailures = 0;
       int startFailures = 0;
       // ...
   public:
       void initAll() {
           for (int i = 0; i < moduleCount; i++) {
               Module::State state = modules[i]->init();
               if (state != Module::State::INITIALIZED) {
                   LOG_E("ModuleRegistry", "Module %s init failed: %d", 
                         modules[i]->getName(), state);
                   reportModuleError(i, state);
                   initFailures++;
               }
           }
       }
       
       bool hasInitFailures() const { return initFailures > 0; }
       int getInitFailureCount() const { return initFailures; }
   };
   ```

2. **Add critical module designation**:
   ```cpp
   // In IModule.h
   virtual bool isCritical() const { return false; }
   
   // In specific modules (e.g., TropicStorage)
   bool isCritical() const override { return true; }
   
   // In ModuleRegistry
   void initAll() {
       // First pass: init all
       for (int i = 0; i < moduleCount; i++) {
           modules[i]->init();
       }
       
       // Second pass: check critical modules
       for (int i = 0; i < moduleCount; i++) {
           if (modules[i]->isCritical() && 
               modules[i]->getState() != Module::State::INITIALIZED) {
               LOG_E("ModuleRegistry", "Critical module %s failed", modules[i]->getName());
               // Stop boot sequence
               return;
           }
       }
   }
   ```

3. **Update callers to check errors**:
   ```cpp
   // In main.cpp
   ModuleRegistry::getInstance().initAll();
   
   // Check for critical failures
   if (ModuleRegistry::getInstance().hasInitFailures()) {
       LOG_W("Main", "Some modules failed to initialize");
       // Show error on display, wait for reset
   }
   ```

4. **Add test cases**:
   - Mock module that fails in `init()`
   - Verify error is logged and counted
   - Verify system behavior (continue vs. stop)
   - Mock critical module failure, verify system stops

## References
- Module Initialization Patterns: https://www.embedded.com/design/programming-and-development/4026249/Module-initialization-patterns
- ESP32 Task Lifecycle: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/task.html
- Error Handling Best Practices: https://www.oreilly.com/library/view/c-in-a/9781492053342/ch04.html

</content>