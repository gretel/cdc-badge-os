---
title: "[LOW] No error boundary in menu callback dispatch"
severity: LOW
domain: error-handling
lens: error-boundaries
labels:
  - "error-handling"
  - "ui-framework"
---

## Summary
Menu callbacks in `ModuleRegistry::getMenuItems()` are called without error isolation. When a module's `getView()` factory function or `onSelect()` callback throws, the menu system can crash.

**Evidence:**
- `components/cdc_core/src/ModuleRegistry.cpp:214-246`:
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
                if (moduleItems[j].isVisible && !moduleItems[j].isVisible()) {
                    continue;
                }
                moduleItems[j].moduleName = modules_[i]->getName();
                items[totalCount++] = moduleItems[j];
            }
        }
    }

    // Sort by priority
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

- Module menu item pattern (from `components/mod_totp/src/TotpModule.cpp:989-999`):
```cpp
items[0] = {mstr(STR_TOTP), 50, []() -> ui::IView* {
    if (!s_viewsInitialized) {
        s_listView.setOnSelect(onListSelect);
        s_viewsInitialized = true;
    }
    rebuildList();
    return &s_listView;
}, nullptr, getName(), core::MenuLocation::MAIN_MENU, nullptr};
```

## Impact
- **Menu crash**: Single buggy callback can freeze menu navigation
- **Unreachable features**: Menu items work but actions fail silently

## Recommended Fix
Add error boundaries when invoking callbacks:

```cpp
// In the code that calls getView() and onSelect():
// Example: when user selects a menu item

static void onMenuSelect(const ModuleMenuItem& item) {
    if (item.getView) {
        try {
            ui::IView* view = item.getView();
            if (view) {
                ViewStack::instance().push(view);
            }
        } catch (const std::exception& e) {
            LOG_E(TAG, "Menu getView exception: %s", e.what());
            ui::showError("Menu error");
        } catch (...) {
            LOG_E(TAG, "Menu getView exception (unknown)");
            ui::showError("Menu error");
        }
    }

    if (item.onSelect) {
        try {
            item.onSelect();
        } catch (const std::exception& e) {
            LOG_E(TAG, "Menu onSelect exception: %s", e.what());
            ui::showError("Menu error");
        } catch (...) {
            LOG_E(TAG, "Menu onSelect exception (unknown)");
            ui::showError("Menu error");
        }
    }
}
```

## References
- [Menu Patterns](https://opencode.ai/guides/ui/menu-patterns/)
- [Callback Error Handling](https://en.cppreference.com/w/cpp/utility/functional)
