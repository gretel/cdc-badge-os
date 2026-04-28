---
title: "[MEDIUM] Feature Envy: ModuleRegistry::getMenuItems accesses module internals extensively"
severity: MEDIUM
domain: core
lens: code-smells
labels:
  - "feature-envy"
  - "cdc_core"
---

## Summary
`ModuleRegistry::getMenuItems()` in `components/cdc_core/src/ModuleRegistry.cpp:218-249` extensively accesses module data through `modules_[i]->getMenuItems()`, then manipulates the returned `ModuleMenuItem` structures by setting `moduleName` and checking `isVisible()`. The method does more aggregation logic that arguably belongs in the module itself.

## Impact
**High coupling**: `ModuleRegistry` knows too much about `ModuleMenuItem` internals and module behavior.

**Violation of Tell-Don't-Ask**: Instead of asking modules to provide ready-to-use data, the registry asks for raw data and manipulates it.

**Maintenance burden**: Any change to menu item structure requires updates in both the module interface AND the registry aggregation logic.

**Scalability**: Adding new menu properties (e.g., icons, keyboard shortcuts) requires modifying the registry's aggregation loop.

## Evidence
`components/cdc_core/src/ModuleRegistry.cpp:218-249`:
```cpp
uint8_t ModuleRegistry::getMenuItems(MenuLocation location, ModuleMenuItem* items, uint8_t maxItems) {
    if (!items || maxItems == 0) return 0;

    uint8_t totalCount = 0;

    // Collect items from all modules
    for (uint8_t i = 0; i < count_; i++) {
        if (modules_[i]->getState() != ServiceState::STARTED) continue;

        ModuleMenuItem moduleItems[8] = {};
        uint8_t count = modules_[i]->getMenuItems(moduleItems, 8);

        for (uint8_t j = 0; j < count && totalCount < maxItems; j++) {
            // Check visibility
            if (moduleItems[j].isVisible && !moduleItems[j].isVisible()) {
                continue;
            }
            // Set module name - registry modifying module data!
            moduleItems[j].moduleName = modules_[i]->getName();
            items[totalCount++] = moduleItems[j];
        }
    }

    // Sort by priority (bubble sort, small array)
    for (uint8_t i = 0; i < totalCount; i++) {
        for (uint8_t j = i + 1; j < totalCount; j++) {
            if (items[j].priority < items[i].priority) {
                ModuleMenuItem tmp = items[i];
                items[i] = items[j];
                items[j] = tmp;
            }
        }
    }

    return totalCount;
}
```

The same pattern appears in `getLockScreenContextItems()` at lines 318-349.

## Recommended Fix
1. **Move aggregation logic into modules**: Add a method like `ModuleRegistry::collectMenuItems()` that delegates more responsibility to modules.

2. **Use builder pattern**: Have modules build fully-formed menu items with `moduleName` already set:
```cpp
// In IModule.h
struct MenuItemBuilder {
    void setModuleName(const char* name);
    ModuleMenuItem build();
};
uint8_t getMenuItems(MenuItemBuilder& builder, uint8_t maxItems);
```

3. **Encapsulate menu aggregation**: Create a `MenuBuilder` class that handles sorting, filtering, and collection logic separately from `ModuleRegistry`.

**Estimated effort**: ~1 hour to refactor the aggregation logic and update module interfaces.

## References
- Refactoring.com: "Feature Envy" - https://refactoring.com/catalog/extractMethod
- Martin Fowler, "Refactoring: Improving the Design of Existing Code", Chapter 7
