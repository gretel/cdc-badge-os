---
title: "[LOW] Long Method: ModuleRegistry::applySlotRequest() is 50+ lines with 5 distinct steps"
severity: LOW
domain: core
lens: code-smells
labels:
  - "long-method"
  - "cdc_core"
---

## Summary
`components/cdc_core/src/ModuleRegistry.cpp:865-914` contains `applySlotRequest()` which is 50 lines long and performs 5 distinct validation steps. While the method has good comments, it would be more maintainable if the step logic was extracted into smaller methods.

## Impact
**Readability**: Developers must scan through 50 lines to understand the flow.

**Testing**: Hard to test individual validation steps without testing the whole method.

**Modification risk**: Changes to one step can accidentally affect others.

## Evidence
`components/cdc_core/src/ModuleRegistry.cpp:865-914`:
```cpp
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

The method has 5 distinct steps that could each be a helper method.

## Recommended Fix
Extract step logic into private helper methods:
```cpp
bool ModuleRegistry::applySlotRequest(IModule* module, uint8_t index) {
    if (!module) return false;

    const char* moduleName = module->getName();

    // Step 1: Validate slot map
    if (!validateSlotMap(moduleName)) {
        return false;
    }

    // Step 2: Get and validate request
    IModule::SlotRequest req = module->getSlotRequest();
    if (!validateSlotRequest(req, moduleName)) {
        return true; // No slots required
    }

    // Step 3-4: Validate slot ranges
    IModule::SlotRange range = {};
    uint8_t moduleId = 0;
    if (!validateSlotRanges(req, moduleName, range, moduleId)) {
        return false;
    }

    // Step 5: Apply
    return applyValidatedRange(module, range, moduleId);
}

// New helper methods
bool ModuleRegistry::validateSlotRequest(const IModule::SlotRequest& req, const char* moduleName);
bool ModuleRegistry::validateSlotRanges(const IModule::SlotRequest& req, const char* moduleName,
                                        IModule::SlotRange& range, uint8_t& moduleId);
bool ModuleRegistry::applyValidatedRange(IModule* module, const IModule::SlotRange& range, uint8_t moduleId);
```

**Estimated effort**: ~1 hour to extract methods and update the main method.

## References
- Refactoring.com: "Long Method" - https://refactoring.com/catalog/extractMethod
- Martin Fowler, "Refactoring: Improving the Design of Existing Code", Chapter 6
