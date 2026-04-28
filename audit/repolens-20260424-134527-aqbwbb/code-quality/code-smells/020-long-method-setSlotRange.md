---
title: "[MEDIUM] Long Method: setSlotRange has excessive logic in one function"
severity: MEDIUM
domain: cdc_core
lens: code-smells
labels:
  - "refactor:extract-method"
  - "maintainability"
---

## Summary
The `setSlotRange` method in `ModuleRegistry` (and related slot validation methods) contains a long sequence of validation steps that could be broken into smaller, more focused methods for better readability and testability.

**Location:** `components/cdc_core/src/ModuleRegistry.cpp:752-914`

The `applySlotRequest` method (lines 869-914) chains multiple validation steps in a single long method:
1. Validate slot map
2. Get slot request from module
3. Validate ECC slot range
4. Validate RMEM slot range
5. Apply validated slot range

## Impact
- **Maintainability**: Long methods are harder to understand and modify
- **Testability**: Difficult to unit-test individual validation steps
- **Readability**: Developers must scroll through many lines to understand the flow
- **Error-prone**: Changes to one validation step may inadvertently affect others

## Evidence
```cpp
// components/cdc_core/src/ModuleRegistry.cpp:869-914
bool ModuleRegistry::applySlotRequest(IModule* module, uint8_t index) {
    if (!module) return false;

    const char* moduleName = module->getName();

    // Step 1: Validate slot map is initialized and valid
    if (!validateSlotMap(moduleName)) {
        return false;
    }

    // Step 2: Get slot request from module
    IModule::SlotRequest req = module->getSlotRequest();
    const char* mapName = (req.mapName && req.mapName[0] != '\0') ? req.mapName : moduleName;

    // No slots required - nothing to validate
    if (!mapName || (req.minEccSlots == 0 && req.minRmemSlots == 0)) {
        return true;
    }

    IModule::SlotRange range = {};
    uint8_t moduleId = 0;

    // Step 3: Validate ECC slot range if required
    if (req.minEccSlots > 0) {
        if (!validateEccRange(mapName, moduleName, req.minEccSlots, range, moduleId)) {
            return false;
        }
    }

    // Step 4: Validate RMEM slot range if required
    if (req.minRmemSlots > 0) {
        if (!validateRmemRange(mapName, moduleName, req.minRmemSlots, range, moduleId)) {
            return false;
        }
    }

    // Step 5: Apply validated slot range to module
    range.moduleId = moduleId;
    module->setSlotRange(range);
    return true;
}
```

## Recommended Fix
Extract each validation step into a well-named private method:

```cpp
bool ModuleRegistry::applySlotRequest(IModule* module, uint8_t index) {
    if (!module) return false;

    const char* moduleName = module->getName();
    
    // Orchestrate validation steps
    return validateAndApplySlotRequest(moduleName, module->getSlotRequest(), module);
}

bool ModuleRegistry::validateAndApplySlotRequest(const char* moduleName,
                                                  IModule::SlotRequest req,
                                                  IModule* module) {
    // Step 1: Validate slot map
    if (!validateSlotMap(moduleName)) return false;

    const char* mapName = getSlotMapName(req, moduleName);
    if (isEmptySlotRequest(mapName, req)) return true;  // Nothing to validate

    IModule::SlotRange range = {};
    uint8_t moduleId = 0;

    // Step 2-3: Validate and apply ECC slots
    if (!validateAndApplyEccSlots(mapName, moduleName, req.minEccSlots, range, moduleId)) {
        return false;
    }

    // Step 4-5: Validate and apply RMEM slots
    if (!validateAndApplyRmemSlots(mapName, moduleName, req.minRmemSlots, range, moduleId)) {
        return false;
    }

    return applySlotRangeToModule(module, range, moduleId);
}
```

## References
- Martin Fowler, "Refactoring: Improving the Design of Existing Code", Chapter 11: "Large Classes" - Extract Method pattern
- Clean Code by Robert C. Martin: Methods should do one thing and be small enough to understand at a glance
