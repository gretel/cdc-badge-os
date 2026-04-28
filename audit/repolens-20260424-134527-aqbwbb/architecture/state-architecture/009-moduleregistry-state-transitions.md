---
title: "[LOW] ModuleRegistry static arrays lack module state transition validation"
severity: LOW
domain: State Management Architecture
lens: state-architecture
labels:
  - "audit:architecture/state-architecture"
---

## Summary
The `ModuleRegistry` singleton (`components/cdc_core/include/cdc_core/ModuleRegistry.h`) uses static arrays (`modules_[MAX_MODULES]`, `moduleErrors_[MAX_MODULES]`) to store module state. Modules transition through states (registered, initialized, started, stopped) but there is no explicit state machine validation for these transitions.

### Evidence:
- `ModuleRegistry.h:201-202`: Static arrays `modules_[MAX_MODULES]`, `moduleErrors_[MAX_MODULES]`
- `ModuleRegistry.h:66-78`: `initAll()`, `startAll()` methods with no per-module state tracking
- `ModuleRegistry.h:81-88`: `stopAll()` stops in reverse order but no validation
- `ModuleRegistry.h:163-174`: `reportModuleError()`, `clearModuleError()` modify error state but no state transition validation

### State Access Pattern:
```cpp
// Module registered
registry.registerModule(&totpModule);

// Module initialized and started
registry.initAll();
registry.startAll();

// Module error reported
registry.reportModuleError("totp", "Slot conflict");

// But no way to query:
// - Is module currently initialized?
// - Is module currently started?
// - What is the current state of module X?
```

## Impact
1. **State confusion**: Can't tell if module is initialized but not started, or started but then stopped
2. **Error state persistence**: `moduleErrors_` persists across retry but no clear state diagram
3. **No state query**: Debugging requires checking module state manually
4. **Stop/start race**: `stopAll()` can be called while module is initializing

## Recommended Fix
1. Add module state enum:
   ```cpp
   enum class ModuleState {
       NOT_REGISTERED,
       REGISTERED,
       INITIALIZED,
       STARTED,
       STOPPED,
       ERROR
   };
   
   typedef struct {
       const char* name;
       ModuleState state;
       const char* errorMessage;
       uint8_t slotRange;
   } ModuleInfo;
   
   ModuleInfo getModuleInfo(const char* name) const;
   ```

2. Add state transition validation:
   ```cpp
   bool startModule(uint8_t index) {
       ModuleState state = getModuleState(index);
       if (state != ModuleState::INITIALIZED) {
           LOG_W("ModuleRegistry", "Module %u not initialized", index);
           return false;
       }
       // ... rest of logic
   }
   ```

3. Add state query API:
   ```cpp
   ModuleState getModuleState(uint8_t index) const;
   ModuleState getModuleStateByName(const char* name) const;
   ```

## References
- `components/cdc_core/include/cdc_core/ModuleRegistry.h` - Full header
- `components/cdc_core/src/ModuleRegistry.cpp` - Implementation
- `main/app.cpp` - Module lifecycle usage
