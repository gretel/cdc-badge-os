---
title: "[LOW] ModuleRegistry menu-item collection iterates all modules each time without caching"
severity: LOW
domain: performance/caching
lens: embedded-firmware
labels:
  - "menu-cache"
  - "iteration-optimization"
---

## Summary
The `ModuleRegistry::getMenuItems()` function (`components/cdc_core/src/ModuleRegistry.cpp`) iterates all registered modules and collects menu items on every call without caching the result. This happens multiple times per second during UI updates.

**Evidence:**
- File: `components/cdc_core/src/ModuleRegistry.cpp`
- Function: `getMenuItems(MenuLocation location, ModuleMenuItem* items, uint8_t maxItems)` (line 218-251)
- Called from UI code to populate menus (Tools menu, main menu, etc.)
- Iterates all `count_` modules, calls `getMenuItems()` on each, sorts results

## Impact
**Performance Cost:**
- Menu collection happens on UI refresh (every few seconds for dynamic menus)
- For 20 modules: 20 function calls + string comparisons + sorting
- Menu items often static between module state changes
- No caching of menu structure

**Redundant Operations:**
- Menu items for each module are typically constant
- Same menu built repeatedly while no modules added/removed
- Sorting (bubble sort) runs every time even if order unchanged

## Evidence
From `components/cdc_core/src/ModuleRegistry.cpp`:

```cpp
/**
 * \brief Collects menu items from started modules for a given location.
 */
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

Every menu display triggers full re-collection and re-sorting.

## Recommended Fix
Implement menu cache with invalidation:

1. **Add cache structures**:
```cpp
static constexpr uint8_t MAX_MENU_CACHE = 5;  // Cache per location
struct MenuCache {
    MenuLocation location;
    ModuleMenuItem items[MAX_MENU_CACHE];
    uint8_t count;
    uint32_t version;  // Invalidate on module changes
};

static MenuCache s_menuCaches[4];  // One per menu location
static uint32_t s_menuVersion = 0;
```

2. **Increment version on module changes**:
```cpp
void ModuleRegistry::registerInitializer(ModuleInitFunc initFunc) {
    // ... existing code ...
    s_menuVersion++;  // Invalidate caches
}

bool ModuleRegistry::registerModule(IModule* module) {
    // ... existing code ...
    s_menuVersion++;  // Invalidate caches
}

void ModuleRegistry::unregisterModule(const char* name) {
    // ... existing code ...
    s_menuVersion++;  // Invalidate caches
}
```

3. **Check cache in getMenuItems()**:
```cpp
uint8_t ModuleRegistry::getMenuItems(MenuLocation location, ModuleMenuItem* items, uint8_t maxItems) {
    // Check cache first
    for (uint8_t i = 0; i < 4; i++) {
        if (s_menuCaches[i].location == location && 
            s_menuCaches[i].version == s_menuVersion &&
            s_menuCaches[i].count <= maxItems) {
            memcpy(items, s_menuCaches[i].items, sizeof(ModuleMenuItem) * s_menuCaches[i].count);
            return s_menuCaches[i].count;
        }
    }
    
    // Cache miss - collect and cache
    // ... existing collection code ...
    
    // Store in cache
    for (uint8_t i = 0; i < 4; i++) {
        if (s_menuCaches[i].location == location) {
            memcpy(s_menuCaches[i].items, items, sizeof(ModuleMenuItem) * totalCount);
            s_menuCaches[i].count = totalCount;
            s_menuCaches[i].version = s_menuVersion;
            break;
        }
    }
    
    return totalCount;
}
```

## References
- Menu UI refresh rate: Typically 1-2 Hz for dynamic displays
- Bubble sort overhead: O(n²) for small arrays (n < 20)
- Module registration frequency: Once at startup, rarely changes
