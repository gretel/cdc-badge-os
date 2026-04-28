---
title: "[MEDIUM] getMenuItems uses fixed-size stack array that may overflow"
severity: MEDIUM
domain: architecture/api-contract
lens: api-contract
labels:
  - "audit:architecture/api-contract"
---

## Summary
The `ModuleRegistry::getMenuItems()` method uses a fixed-size stack array:

```cpp
// components/cdc_core/src/ModuleRegistry.cpp:225-228
uint8_t ModuleRegistry::getMenuItems(MenuLocation location, ModuleMenuItem* items, uint8_t maxItems) {
    // ...
    for (uint8_t i = 0; i < count_; i++) {
        ModuleMenuItem moduleItems[8] = {};  // Fixed size!
        uint8_t count = modules_[i]->getMenuItems(moduleItems, 8);
        // ...
    }
}
```

Each module is asked to fill up to 8 items, but there's no guarantee that:
1. The module actually has 8 or fewer items
2. The module respects the `maxItems` parameter
3. Overflow won't occur if a module returns more than 8 items

Looking at implementations:
```cpp
// components/mod_gpg/src/GpgModule.cpp:628
uint8_t GpgModule::getMenuItems(core::ModuleMenuItem* items, uint8_t maxItems) {
    if (!items || maxItems == 0) return 0;
    items[0] = {...};  // Only fills 1 item
    return 1;
}
```

Most modules only return 1 item, but the contract isn't enforced.

## Impact
- **Stack overflow**: If a module returns >8 items, stack is corrupted
- **Silent truncation**: Items beyond 8 are lost without warning
- **No validation**: No check that `count <= maxItems`

## Evidence
- ModuleRegistry.cpp:225-228 (fixed array size)
- All module implementations return 1-5 items (no one uses full 8)

## Recommended Fix
Add bounds checking:

```cpp
uint8_t ModuleRegistry::getMenuItems(MenuLocation location, ModuleMenuItem* items, uint8_t maxItems) {
    // ...
    for (uint8_t i = 0; i < count_; i++) {
        ModuleMenuItem moduleItems[8] = {};
        uint8_t count = modules_[i]->getMenuItems(moduleItems, 8);
        
        // Validate module respected maxItems
        if (count > 8) {
            LOG_W(TAG, "Module '%s' returned %d items, truncating to 8", 
                  modules_[i]->getName(), count);
            count = 8;
        }
        
        for (uint8_t j = 0; j < count && totalCount < maxItems; j++) {
            // ...
        }
    }
    // ...
}
```

Or use dynamic allocation:
```cpp
// Allocate based on actual module count
std::vector<ModuleMenuItem> moduleItems(count_);
```

## References
- ModuleRegistry: components/cdc_core/src/ModuleRegistry.cpp
- IModule::getMenuItems: components/cdc_core/include/cdc_core/IModule.h:75-81
