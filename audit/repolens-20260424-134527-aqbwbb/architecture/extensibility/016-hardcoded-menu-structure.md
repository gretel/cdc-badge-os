---
title: "[MEDIUM] Hardcoded Menu Structure in AppUi"
severity: MEDIUM
domain: architecture/extensibility
lens: menu-architecture
labels:
  - "audit:architecture/extensibility"
---

## Summary
The main menu structure in `components/cdc_os_ui/src/AppUi.cpp` has hardcoded fixed items (Tools, Settings) and fixed positions for module items. The menu layout logic is tightly coupled to specific item indices and counts, making it difficult to add new top-level menu categories.

**Files affected:**
- `components/cdc_os_ui/src/AppUi.cpp` (lines 56-130, 404-434)
- `components/cdc_os_ui/src/AppUiInternal.h`

## Impact
- **Menu structure rigidity**: Adding new top-level categories (e.g., "Games", "Utilities") requires modifying AppUi.cpp
- **Index-dependent logic**: Menu handlers use hardcoded indices (`getToolsIndex()`, `getSettingsIndex()`)
- **Limited extensibility**: Modules can only register items, not create their own top-level menu categories
- **Maintenance burden**: Changes to menu structure require updates to multiple handler functions

## Evidence

### Hardcoded Menu Constants
`components/cdc_os_ui/src/AppUi.cpp:56-60`:
```cpp
static constexpr uint8_t MAIN_MENU_MAX_ITEMS = 16;
static constexpr uint8_t MAIN_MENU_FIXED_COUNT = 2;  // Tools + Settings
static constexpr uint8_t TOOLS_FIXED_COUNT = 4;       // Modules, WiFi, Bluetooth, Expert
static constexpr uint8_t TOOLS_MAX_ITEMS = 16;
```

### Index-Dependent Selection Logic
`components/cdc_os_ui/src/AppUi.cpp:404-408`:
```cpp
static void onMainMenuSelect(uint16_t index, void* userData) {
    (void)userData;

    // Module items first
    if (index < s_mainMenuPluginCount) {
        auto& item = s_mainMenuModuleItems[index];
        // ...
        return;
    }

    if (index == getToolsIndex()) {
        ViewStack::instance().push(s_toolsMenu);
    } else if (index == getSettingsIndex()) {
        ViewStack::instance().push(s_settingsMenu);
    }
}
```

### Hardcoded Switch Statements
`components/cdc_os_ui/src/AppUi.cpp:419-433`:
```cpp
static void onToolsSelect(uint16_t index, void* userData) {
    (void)userData;

    switch (index) {
        case 0: showModulesView(); return;
        case 1: showWifiMainMenu(); return;
        case 2: showBluetoothMenu(); return;
        case 3: showExpertMenu(); return;
    }
    // ...
}
```

### Fixed Settings Menu
`components/cdc_os_ui/src/AppUi.cpp:64-76`:
```cpp
enum SettingsMenuIdx {
    SETTINGS_IDX_BRIGHTNESS = 0,
    SETTINGS_IDX_LANGUAGE,
    SETTINGS_IDX_TIMEZONE,
    SETTINGS_IDX_AUTO_SLEEP,
    SETTINGS_IDX_BADGE_TEXT,
    SETTINGS_IDX_SET_DATE,
    SETTINGS_IDX_SET_TIME,
    SETTINGS_IDX_CHANGE_PIN,
    SETTINGS_IDX_COUNT
};
```

## Recommended Fix

### Create Menu Registry Pattern
Allow dynamic menu structure with registration:

```cpp
// components/cdc_ui/include/cdc_ui/MenuRegistry.h
#pragma once
#include <cstdint>

namespace cdc::ui {

struct MenuItem {
    const char* label;
    void (*onSelect)();
    uint8_t priority;  // Sort order
    const char* icon;  // Optional icon
};

struct MenuCategory {
    const char* name;
    MenuItem* items;
    uint8_t itemCount;
    uint8_t priority;
};

class MenuRegistry {
public:
    static MenuRegistry& instance();
    
    // Register a top-level menu category
    void registerCategory(const char* name, uint8_t priority);
    
    // Register menu items for a category
    void registerItems(const char* category, MenuItem* items, uint8_t count);
    
    // Get all categories for a menu location
    void getCategories(const char* location, MenuCategory* out, uint8_t maxCount);
    
    // Get items for a specific category
    void getCategoryItems(const char* category, MenuItem* out, uint8_t maxCount);
};

// Convenience macros
#define MENU_CATEGORY(name, priority) \
    cdc::ui::MenuRegistry::instance().registerCategory(name, priority)

#define MENU_ITEMS(category, items) \
    cdc::ui::MenuRegistry::instance().registerItems(category, items, sizeof(items)/sizeof(items[0]))

} // namespace cdc::ui
```

### Refactor AppUi to Use Registry
```cpp
// components/cdc_os_ui/src/AppUi.cpp
static void rebuildMainMenu() {
    auto& registry = ui::MenuRegistry::instance();
    ui::MenuCategory categories[8];
    uint8_t count = registry.getCategories("main", categories, 8);
    
    for (uint8_t i = 0; i < count; i++) {
        s_mainMenuItems[i] = {
            categories[i].name,
            0, false,
            reinterpret_cast<void*>(static_cast<uintptr_t>(i))
        };
    }
    
    s_mainMenu->init(tr(StringId::MAIN_MENU), s_mainMenuItems, count);
}

static void onMainMenuSelect(uint16_t index, void* userData) {
    auto& registry = ui::MenuRegistry::instance();
    ui::MenuCategory categories[8];
    uint8_t count = registry.getCategories("main", categories, 8);
    
    if (index < count) {
        // Push the selected category's menu
        ui::ListItem items[16];
        registry.getCategoryItems(categories[index].name, items, 16);
        auto* submenu = showListView(categories[index].name, items, 
                                     registry.getCategoryItemCount(categories[index].name),
                                     onSubmenuSelect);
    }
}
```

### Module-Registered Menu Categories
Modules can now register their own menu categories:
```cpp
// components/mod_games/src/GamesModule.cpp
static void registerMenus() {
    static ui::MenuItem gameItems[] = {
        {"Snake", []() { ui::ViewStack::instance().push(&s_snakeGame); }, 0},
        {"Memory", []() { ui::ViewStack::instance().push(&s_memoryGame); }, 1},
    };
    
    ui::MenuRegistry::instance().registerCategory("Games", 100);
    ui::MenuRegistry::instance().registerItems("Games", gameItems, 2);
}
```

## References
- Menu Pattern: https://en.wikipedia.org/wiki/Menu_(computing)
- Registry Pattern: https://refactoring.guru/design-patterns/registry
- Composite Pattern for nested menus: https://refactoring.guru/design-patterns/composite

</content>