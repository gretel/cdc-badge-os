---
title: "[MEDIUM] ModuleRegistry.cpp combines module lifecycle, NVS persistence, slot validation, and UI event dispatch"
severity: MEDIUM
domain: cdc_core
lens: single-responsibility
labels:
  - "audit:architecture/single-responsibility"
---

## Summary
`components/cdc_core/src/ModuleRegistry.cpp` (914 lines) handles multiple distinct responsibilities:
1. **Module lifecycle** - `initAll()`, `startAll()`, `stopAll()`, `retryModule()`
2. **NVS persistence** - `saveModuleList()`, `loadDisabledList()`, `saveDisabledList()`, `cleanupOrphanedModuleData()`
3. **Slot validation** - `validateSlotMap()`, `validateEccRange()`, `validateRmemRange()`, `applySlotRequest()`
4. **UI event dispatch** - `dispatchUnlock()`, `dispatchLock()`, `dispatchUsbConnect()`, `dispatchTick()`
5. **Menu aggregation** - `getMenuItems()`, `getLockScreenContextItems()`
6. **Error management** - `setModuleError()`, `reportModuleError()`, `clearModuleError()`

## Impact
- **High coupling**: NVS changes, slot validation logic, and module lifecycle all in same file
- **Complexity**: 914 lines makes it hard to find specific functionality
- **Testing difficulty**: Cannot test slot validation without NVS initialization
- **Merge conflicts**: Multiple developers working on different aspects conflict on same file

## Evidence
File: `components/cdc_core/src/ModuleRegistry.cpp`
- Lines 19-91: Module registration and lookup
- Lines 143-200: Module lifecycle (init/start/stop)
- Lines 203-352: Menu aggregation and event dispatch
- Lines 354-467: NVS persistence (cleanup, save/load module lists)
- Lines 469-625: Module enable/disable logic with NVS
- Lines 627-752: Error management
- Lines 754-914: Slot validation and application

Key function showing mixed concerns:
```cpp
bool ModuleRegistry::applySlotRequest(IModule* module, uint8_t index) {
    // Step 1: Validate slot map
    if (!validateSlotMap(moduleName)) { return false; }
    
    // Step 2: Get slot request
    IModule::SlotRequest req = module->getSlotRequest();
    
    // Step 3: Validate ECC slots
    if (req.minEccSlots > 0) {
        if (!validateEccRange(...)) { return false; }
    }
    
    // Step 4: Validate RMEM slots
    if (req.minRmemSlots > 0) {
        if (!validateRmemRange(...)) { return false; }
    }
    
    // Step 5: Apply validated range
    module->setSlotRange(range);
    return true;
}
```

## Recommended Fix
Split into focused modules:
1. **ModuleLifecycle** - Module init/start/stop/retry in `components/cdc_core/src/ModuleLifecycle.cpp`
2. **ModulePersistence** - NVS save/load/cleanup in `components/cdc_core/src/ModulePersistence.cpp`
3. **SlotValidator** - Slot map validation in `components/cdc_core/src/SlotValidator.cpp`
4. **ModuleRouter** - Event dispatch and menu aggregation in `components/cdc_core/src/ModuleRouter.cpp`

Each module should:
- Have its own header file
- Accept dependencies via constructor
- Be testable in isolation

Example split structure:
```
components/cdc_core/src/
  ModuleRegistry.cpp   // Main orchestration only
  ModuleLifecycle.cpp  // init/start/stop/retry
  ModulePersistence.cpp // NVS operations
  SlotValidator.cpp    // Slot validation
  ModuleRouter.cpp     // Event dispatch, menu aggregation
```

## References
- Single Responsibility Principle: https://en.wikipedia.org/wiki/Single-responsibility_principle
- Clean Architecture: https://blog.cleancoder.com/uncle-bob/2012/08/13/the-clean-architecture.html
