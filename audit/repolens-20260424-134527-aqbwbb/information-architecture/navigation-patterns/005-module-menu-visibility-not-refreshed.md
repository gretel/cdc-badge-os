---
title: "[MEDIUM] Module menu items don't refresh visibility state"
severity: MEDIUM
domain: navigation-patterns
lens: information-architecture
labels:
  - "audit:information-architecture/navigation-patterns"
---

## Summary
Module menu items can define an `isVisible()` callback to conditionally show/hide items, but these callbacks are only evaluated when the menu is first built. Changes to module state (e.g., WiFi connecting/disconnecting, Bluetooth pairing) don't trigger menu refreshes, leaving stale menu items visible or hidden.

**Evidence locations:**
- `components/cdc_core/include/cdc_core/IModule.h:32` - `bool (*isVisible)();` callback
- `components/cdc_core/src/ModuleRegistry.cpp:225` - `isVisible()` checked only during `getMenuItems()`
- `components/cdc_os_ui/src/AppUi.cpp:330` - Menu rebuilds only on explicit call

## Impact
**Stale UI:** Menu items may show/hide incorrectly after state changes.
**User confusion:** Items that should be available might be hidden (or vice versa).
**Feature discoverability:** Users may not see newly available options.

## Evidence
```cpp
// In ModuleRegistry.cpp - isVisible checked only once
uint8_t ModuleRegistry::getMenuItems(MenuLocation location, ModuleMenuItem* items, uint8_t maxItems) {
    for (uint8_t i = 0; i < count_; i++) {
        if (modules_[i]->getState() != ServiceState::STARTED) continue;

        ModuleMenuItem moduleItems[8] = {};
        uint8_t count = modules_[i]->getMenuItems(moduleItems, 8);

        for (uint8_t j = 0; j < count && totalCount < maxItems; j++) {
            if (moduleItems[j].location == location) {
                // Check visibility - but this is called only when menu is rebuilt!
                if (moduleItems[j].isVisible && !moduleItems[j].isVisible()) {
                    continue;
                }
                items[totalCount++] = moduleItems[j];
            }
        }
    }
    return totalCount;
}

// In AppUi.cpp - menu rebuilds only on explicit call
static void onMainMenuSelect(uint16_t index, void* userData) {
    // No check if menu needs refresh before showing
    ViewStack::instance().push(s_mainMenu);
}
```

## Recommended Fix
Implement automatic menu refresh on state changes:
1. **Call `isVisible()` at render time** (not just build time)
2. **Add state-change events** that trigger menu rebuilds
3. **Cache visibility state** and compare before rendering

Example:
```cpp
// In ListView render()
void ListView::render(bool partial) {
    // Check visibility for each item dynamically
    for (uint16_t i = 0; i < itemCount_; i++) {
        if (items_[i].isVisible && !items_[i].isVisible()) {
            // Skip hidden items
        }
    }
}

// Or: Trigger rebuild on state change
EventBus::instance().subscribe([](const Event& e) {
    if (e.type == EventType::MODULE_STATE_CHANGED) {
        ui_rebuild_menus();
    }
}, static_cast<uint32_t>(EventType::MODULE_STATE_CHANGED));
```

## References
- `components/cdc_core/include/cdc_core/IModule.h` - ModuleMenuItem with isVisible()
- `components/cdc_core/src/ModuleRegistry.cpp` - getMenuItems() implementation
