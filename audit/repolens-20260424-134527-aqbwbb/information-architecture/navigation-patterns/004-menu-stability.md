---
title: "[LOW] Menu items can disappear after navigation due to dynamic rebuild"
severity: LOW
domain: information-architecture/navigation-patterns
lens: navigation-patterns
labels:
  - "audit:information-architecture/navigation-patterns"
---

## Summary
Menu items are dynamically rebuilt from module registration each time a menu is displayed, but there's no guarantee that menu items remain stable during navigation. If a module is enabled/disabled or becomes ready while navigating, the menu structure can change, potentially causing navigation targets to shift or disappear.

**Files affected:**
- `components/cdc_os_ui/src/AppUi.cpp` - Menu rebuild functions (lines 310-360)
- `components/cdc_core/include/cdc_core/ModuleRegistry.h` - Menu item collection (lines 98-100)
- `components/mod_totp/src/TotpModule.cpp` - Module menu registration (lines 986-1000)

## Impact
Users navigating menus may experience:
1. Menu items shifting positions when returning to a menu
2. Selected items pointing to different features after menu rebuild
3. Lost navigation state if an item disappears

This is especially relevant for:
- Modules that register menu items conditionally
- Modules that become ready after initial menu display
- Expert menu where modules can be toggled on/off

## Evidence
Menu items are rebuilt dynamically each time:

```cpp
// components/cdc_os_ui/src/AppUi.cpp (lines 310-330)
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
    // ...
    if (s_mainMenu) {
        s_mainMenu->init(tr(StringId::MAIN_MENU), s_mainMenuItems, getMainMenuCount());
    }
}
```

Menu rebuild is triggered by module events:

```cpp
// components/cdc_os_ui/src/AppUi.cpp (lines 720-730)
void ui_on_modules_ready() {
    rebuildToolsMenu();
    rebuildMainMenu();
}

void ui_rebuild_menus() {
    rebuildToolsMenu();
    rebuildMainMenu();
}
```

Modules register menu items dynamically:

```cpp
// components/mod_totp/src/TotpModule.cpp (lines 986-1000)
uint8_t TotpModule::getMenuItems(core::ModuleMenuItem* items, uint8_t maxItems) {
    if (!items || maxItems == 0) return 0;
    
    items[0] = {mstr(STR_TOTP), 50, []() -> ui::IView* {
        if (!s_viewsInitialized) {
            s_listView.setOnSelect(onListSelect);
            s_viewsInitialized = true;
        }
        rebuildList();
        return &s_listView;
    }, nullptr, getName(), core::MenuLocation::MAIN_MENU, nullptr};
    
    return 1;
}
```

The `isVisible()` callback can conditionally hide items (defined in IModule.h line 34), but no mechanism preserves navigation state across rebuilds.

## Recommended Fix
Add menu stability guarantees:

1. **Track menu item identity** - Add unique ID to ModuleMenuItem:
   ```cpp
   struct ModuleMenuItem {
       const char* label;
       uint8_t priority;
       uint8_t id;  // Unique ID for tracking
       // ...
   };
   ```

2. **Preserve selection across rebuilds** - Store last selected index:
   ```cpp
   void rebuildMainMenu() {
       uint8_t lastSelection = s_mainMenu->getSelection();
       // ... rebuild ...
       s_mainMenu->setSelection(lastSelection);
   }
   ```

3. **Defer menu rebuilds** - Only rebuild when menu is not active:
   ```cpp
   void ui_rebuild_menus() {
       if (ViewStack::instance().current() != s_mainMenu) {
           rebuildMainMenu();
       }
       if (ViewStack::instance().current() != s_toolsMenu) {
           rebuildToolsMenu();
       }
   }
   ```

Scope: ~1 hour to add selection preservation.

## References
- Menu rebuild: `components/cdc_os_ui/src/AppUi.cpp` lines 310-360
- Module menu registration: `components/cdc_core/include/cdc_core/IModule.h` lines 24-37
