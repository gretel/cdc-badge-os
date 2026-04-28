---
title: "[MEDIUM] Menu lists rebuilt without caching module state"
severity: MEDIUM
domain: startup-performance
lens: startup-perf
labels:
  - "audit:performance/startup-perf"
---

## Summary
Menu lists are rebuilt after module initialization (`main.cpp:234`) via `ui_on_modules_ready()`. This triggers menu reconstruction and label translation for all menu items, even though the module list hasn't changed since the last boot.

**Location:** `main/main.cpp:234`, `components/cdc_os_ui/src/AppUi.cpp:716-723`

## Impact
- **Redundant work**: Menu items rebuilt every boot, even if no modules changed
- **String translation overhead**: All menu labels translated again
- **Display update**: Menu rebuilds may trigger display refresh

## Evidence
From `main/main.cpp:232-236`:
```cpp
// Run all registered module initializers
LOG_I(TAG, "Initializing modules...");
cdc::core::ModuleRegistry::instance().runAllInitializers();

// Rebuild UI menus with module items
cdc::ui::ui_on_modules_ready();  // Rebuilds all menus
```

From `AppUi.cpp:716-723`:
```cpp
void ui_on_modules_ready() {
    rebuildToolsMenu();
    rebuildMainMenu();
}

void rebuildMainMenu() {
    auto& moduleReg = core::ModuleRegistry::instance();
    
    s_mainMenuPluginCount = moduleReg.getMenuItems(
        core::MenuLocation::MAIN_MENU,
        s_mainMenuModuleItems,
        MAIN_MENU_MAX_ITEMS - MAIN_MENU_FIXED_COUNT
    );
    
    for (uint8_t i = 0; i < s_mainMenuPluginCount; i++) {
        s_mainMenuItems[i] = {s_mainMenuModuleItems[i].label, 0, false, nullptr};
    }
    
    if (s_mainMenu) {
        s_mainMenu->init(tr(StringId::MAIN_MENU), s_mainMenuItems, getMainMenuCount());
    }
}
```

## Recommended Fix
**Cache menu state and rebuild only when needed**:

1. Store module list hash from last boot
2. Rebuild menus only if module list changed
3. Use dirty flags for individual menus

```cpp
void ui_on_modules_ready() {
    // Check if modules changed
    uint32_t currentHash = computeModuleListHash();
    uint32_t savedHash = loadSavedModuleHash();
    
    if (currentHash != savedHash) {
        rebuildMainMenu();
        rebuildToolsMenu();
        saveModuleHash(currentHash);
    }
}

uint32_t computeModuleListHash() {
    // Hash of module names + versions
    auto& reg = core::ModuleRegistry::instance();
    uint32_t hash = 0;
    for (uint8_t i = 0; i < reg.count(); i++) {
        auto* m = reg.getModuleAt(i);
        hash ^= stringHash(m->getName()) + stringHash(m->getVersion());
    }
    return hash;
}
```

**Alternative**: Defer menu rebuild to first access
```cpp
static bool menusRebuilt_ = false;
void rebuildMainMenu() {
    if (menusRebuilt_) return;
    // ... rebuild logic ...
    menusRebuilt_ = true;
}
```

## References
- ESP32: [Menu rendering time](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/spi_master.html) - E-Paper refresh is slow