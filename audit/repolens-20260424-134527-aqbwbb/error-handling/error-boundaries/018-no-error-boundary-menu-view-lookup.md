---
title: "[LOW] No error boundary in menu view lookup callbacks"
severity: LOW
domain: error-handling
lens: error-boundaries
labels:
  - "error-handling"
  - "ui-framework"
---

## Summary
Multiple menu UI files call `item.getView()` without error boundaries. When a module's view factory function throws, the menu navigation can crash. This pattern is repeated in multiple files and wasn't fully covered by the general "menu callback" finding.

**Evidence:**
- `components/cdc_os_ui/src/AppUi.cpp:398`:
```cpp
static void onMainMenuSelect(uint16_t index, void* userData) {
    if (index < s_mainMenuPluginCount) {
        auto& item = s_mainMenuModuleItems[index];
        if (item.getView) {
            IView* view = item.getView();  // No error boundary!
            if (view) ViewStack::instance().push(view);
        }
        return;
    }
    // ...
}
```

- `components/cdc_os_ui/src/AppUi.cpp:430`:
```cpp
static void onToolsSelect(uint16_t index, void* userData) {
    uint8_t moduleIdx = index - TOOLS_FIXED_COUNT;
    if (moduleIdx < s_toolsModuleCount) {
        auto& item = s_toolsModuleItems[moduleIdx];
        if (item.getView) {
            IView* view = item.getView();  // No error boundary!
            if (view) ViewStack::instance().push(view);
        }
    }
}
```

- `components/cdc_os_ui/src/ExpertMenuUi.cpp:273`:
```cpp
static void onExpertMenuSelect(uint16_t index, void* userData) {
    uint8_t moduleIdx = index - EXPERT_FIXED_COUNT;
    if (moduleIdx < s_expertModuleCount) {
        const auto& item = s_expertModuleItems[moduleIdx];
        if (item.getView) {
            IView* view = item.getView();  // No error boundary!
            if (view) {
                ViewStack::instance().push(view);
            }
        }
    }
}
```

- `components/cdc_os_ui/src/BluetoothMenuUi.cpp:188`:
```cpp
static void onBluetoothMenuSelect(uint16_t index, void* userData) {
    uint8_t moduleIdx = index - BT_IDX_FIXED_COUNT;
    if (moduleIdx < s_bluetoothModuleCount) {
        auto& item = s_bluetoothModuleItems[moduleIdx];
        if (item.getView) {
            IView* view = item.getView();  // No error boundary!
            if (view) {
                ViewStack::instance().push(view);
            }
        } else if (item.onSelect) {
            item.onSelect();  // Also no error boundary!
        }
    }
}
```

- `components/cdc_os_ui/src/WifiMenuUi.cpp:282`:
```cpp
static void onWifiMainSelect(uint16_t index, void* userData) {
    uint8_t moduleIdx = index - WIFI_MENU_FIXED_COUNT;
    if (moduleIdx < s_wifiModuleCount) {
        auto& item = s_wifiModuleItems[moduleIdx];
        if (item.getView) {
            IView* view = item.getView();  // No error boundary!
            if (view) {
                ViewStack::instance().push(view);
            }
        }
    }
}
```

## Impact
- **Menu crash**: Single buggy view factory can freeze menu navigation
- **Unreachable features**: Menu items appear but fail silently
- **No error visibility**: View factory errors not logged
- **Debug difficulty**: Hard to identify which module's view failed

## Recommended Fix
Add error boundaries to all menu view lookup calls. Create a helper function:

```cpp
// In AppUi.cpp or a shared header
static bool pushViewWithErrorHandling(const char* moduleName, ui::IView* view) {
    if (!view) return false;
    try {
        ui::ViewStack::instance().push(view);
        return true;
    } catch (const std::exception& e) {
        LOG_E(TAG, "Module '%s' view push exception: %s", moduleName, e.what());
        ui::showError("View error");
        return false;
    } catch (...) {
        LOG_E(TAG, "Module '%s' view push exception (unknown)", moduleName);
        ui::showError("View error");
        return false;
    }
}

static void onMainMenuSelect(uint16_t index, void* userData) {
    (void)userData;

    if (index < s_mainMenuPluginCount) {
        auto& item = s_mainMenuModuleItems[index];
        if (item.getView) {
            try {
                IView* view = item.getView();
                if (view) pushViewWithErrorHandling(item.moduleName, view);
            } catch (const std::exception& e) {
                LOG_E(TAG, "Main menu getView exception: %s", e.what());
                ui::showError("Menu error");
            } catch (...) {
                LOG_E(TAG, "Main menu getView exception (unknown)");
                ui::showError("Menu error");
            }
        }
        return;
    }

    if (index == getToolsIndex()) {
        ViewStack::instance().push(s_toolsMenu);
    } else if (index == getSettingsIndex()) {
        ViewStack::instance().push(s_settingsMenu);
    }
}
```

Apply the same pattern to all other menu select handlers.

## References
- [UI Error Boundaries](https://opencode.ai/guides/ui/error-boundaries/)
- [Callback Error Handling](https://en.cppreference.com/w/cpp/utility/functional)
