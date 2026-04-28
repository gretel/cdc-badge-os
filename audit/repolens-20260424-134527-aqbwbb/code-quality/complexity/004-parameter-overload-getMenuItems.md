---
title: "[LOW] Repeated parameter patterns in `ModuleRegistry::getMenuItems` and similar functions"
severity: LOW
domain: cdc_core
lens: cyclomatic-complexity
labels:
  - "audit:code-quality/complexity"
---

## Summary

The `ModuleRegistry::getMenuItems` function in `components/cdc_core/src/ModuleRegistry.cpp` (lines 218-255) and similar functions like `getLockScreenContextItems` follow identical patterns with multiple parameters that could be consolidated into a configuration struct.

**Location:** `components/cdc_core/src/ModuleRegistry.cpp:218-255`

## Impact

- **API consistency:** Multiple similar functions with slightly different signatures
- **Future maintenance:** Adding new options requires changing all similar function signatures
- **Readability:** Callers must remember parameter order for similar functions

## Evidence

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
            if (moduleItems[j].location == location) {
                // Check visibility
                if (moduleItems[j].isVisible && !moduleItems[j].isVisible()) {
                    continue;
                }
                // Set module name
                moduleItems[j].moduleName = modules_[i]->getName();
                items[totalCount++] = moduleItems[j];
            }
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

Similar pattern in `getLockScreenContextItems`:
```cpp
uint8_t ModuleRegistry::getLockScreenContextItems(LockScreenContextItem* items, uint8_t maxItems) {
    // Very similar structure
}
```

**Issues:**
- 3 parameters where 1 struct could suffice
- Hard-coded array size (8) in function body
- Bubble sort logic embedded in collection function

## Recommended Fix

1. **Create a configuration struct**:
```cpp
struct CollectionConfig {
    uint8_t maxItems;
    uint8_t tempBufferSize;
    MenuLocation location;  // Optional filter
    bool (*visibilityFilter)(const ModuleMenuItem&);  // Optional callback
};
```

2. **Simplify function signature**:
```cpp
uint8_t ModuleRegistry::collectItems(CollectionConfig config, 
                                      ModuleMenuItem* items);
```

3. **Extract sorting logic**:
```cpp
static void sortItemsByPriority(ModuleMenuItem* items, uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        for (uint8_t j = i + 1; j < count; j++) {
            if (items[j].priority < items[i].priority) {
                ModuleMenuItem tmp = items[i];
                items[i] = items[j];
                items[j] = tmp;
            }
        }
    }
}
```

**Estimated effort:** ~30 minutes (low priority - current code works)

## References

- Configuration object pattern: https://refactoring.com/catalog/introduceParameterObject.html
- Single responsibility principle
