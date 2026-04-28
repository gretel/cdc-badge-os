---
title: "[LOW] Nested loops and conditions in getMenuItems function"
severity: LOW
domain: code-quality
lens: cyclomatic-complexity
labels:
  - "complexity:moderate"
---

## Summary
The `getMenuItems` function in `components/cdc_core/src/ModuleRegistry.cpp:218` uses nested loops with multiple conditions, creating moderate complexity. The function iterates modules, then their menu items, then performs a bubble sort.

**Evidence:**
- File: `components/cdc_core/src/ModuleRegistry.cpp`
- Lines: 218-255 (37 lines)
- Nested structure: 2 levels deep (modules → menu items)
- Conditions per inner iteration: 4 (state check, location match, visibility check, maxItems limit)
- Additional nested loop: bubble sort (2 more levels)

## Impact
- **Readability**: Multiple conditions in inner loop make logic harder to follow
- **Performance**: O(n²) bubble sort (acceptable for small arrays but could be clearer)
- **Maintainability**: Adding new filtering criteria increases nesting depth

## Evidence
```cpp
uint8_t ModuleRegistry::getMenuItems(MenuLocation location, ModuleMenuItem* items, uint8_t maxItems) {
    if (!items || maxItems == 0) return 0;

    uint8_t totalCount = 0;

    // Outer loop: iterate modules
    for (uint8_t i = 0; i < count_; i++) {
        if (modules_[i]->getState() != ServiceState::STARTED) continue;

        ModuleMenuItem moduleItems[8] = {};
        uint8_t count = modules_[i]->getMenuItems(moduleItems, 8);

        // Inner loop: iterate menu items
        for (uint8_t j = 0; j < count && totalCount < maxItems; j++) {
            if (moduleItems[j].location == location) {          // Condition 1
                // Check visibility
                if (moduleItems[j].isVisible &&                 // Condition 2
                    !moduleItems[j].isVisible()) {
                    continue;
                }
                moduleItems[j].moduleName = modules_[i]->getName();
                items[totalCount++] = moduleItems[j];
            }
        }
    }

    // Nested loop: bubble sort
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

## Recommended Fix
Extract filtering logic and use clearer iteration:

1. **Extract filtering helper:**
   ```cpp
   static bool shouldIncludeMenuItem(const ModuleMenuItem& item, 
                                      MenuLocation location) {
       if (item.location != location) return false;
       if (item.isVisible && !item.isVisible()) return false;
       return true;
   }
   ```

2. **Refactor main loop:**
   ```cpp
   uint8_t ModuleRegistry::getMenuItems(MenuLocation location, 
                                        ModuleMenuItem* items, 
                                        uint8_t maxItems) {
       if (!items || maxItems == 0) return 0;

       uint8_t totalCount = 0;

       for (uint8_t i = 0; i < count_ && totalCount < maxItems; i++) {
           if (modules_[i]->getState() != ServiceState::STARTED) continue;

           ModuleMenuItem moduleItems[8] = {};
           uint8_t count = modules_[i]->getMenuItems(moduleItems, 8);

           for (uint8_t j = 0; j < count; j++) {
               if (!shouldIncludeMenuItem(moduleItems[j], location)) continue;
               
               moduleItems[j].moduleName = modules_[i]->getName();
               items[totalCount++] = moduleItems[j];
           }
       }

       // Sort by priority (consider std::sort for clarity)
       std::sort(items, items + totalCount, 
                 [](const auto& a, const auto& b) { 
                     return a.priority < b.priority; 
                 });

       return totalCount;
   }
   ```

**Estimated effort**: 30-45 minutes

## References
- Cyclomatic Complexity thresholds: https://www.sonarsource.com/docs/CyclomaticComplexity.pdf
- C++ Core Guidelines: [F.20](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#ffor-use-for-loops) - Use for loops for iteration
- Refactoring: "Decompose Conditional" (Fowler)
