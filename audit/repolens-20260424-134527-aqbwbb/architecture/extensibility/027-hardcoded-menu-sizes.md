---
title: "[MEDIUM] Hardcoded Menu Size Constants Limit Module Growth"
severity: MEDIUM
domain: architecture/extensibility
lens: extensibility-plugin
labels:
  - "hardcoded-behavior"
  - "ui-menus"
  - "open-closed-principle"
---

## Summary

The menu system uses hardcoded size constants for each menu location. When a menu exceeds its maximum capacity, additional module items are silently dropped without error notification.

Each menu has fixed backing storage arrays that must be resized in core code to accommodate more items.

**Evidence:**

`components/cdc_os_ui/src/AppUi.cpp:52-55`:
```cpp
static constexpr uint8_t MAIN_MENU_MAX_ITEMS = 16;
static constexpr uint8_t MAIN_MENU_FIXED_COUNT = 2;  // Tools + Settings
static constexpr uint8_t TOOLS_FIXED_COUNT = 4;       // Modules, WiFi, Bluetooth, Expert
static constexpr uint8_t TOOLS_MAX_ITEMS = 16;

static ListItem s_mainMenuItems[MAIN_MENU_MAX_ITEMS];
static core::ModuleMenuItem s_mainMenuModuleItems[MAIN_MENU_MAX_ITEMS];
```

`components/cdc_os_ui/src/ExpertMenuUi.cpp:20-28`:
```cpp
static constexpr uint8_t EXPERT_FIXED_COUNT = 3;
static constexpr uint8_t EXPERT_MAX_ITEMS = 12;
static constexpr uint8_t MODULES_VIEW_MAX = 16;

static ListItem s_expertItems[EXPERT_MAX_ITEMS];
static core::ModuleMenuItem s_expertModuleItems[EXPERT_MAX_ITEMS - EXPERT_FIXED_COUNT];
```

**Current limits:**
- Main menu: 14 module items (16 total - 2 fixed)
- Tools menu: 12 module items (16 total - 4 fixed)
- Expert menu: 9 module items (12 total - 3 fixed)

When `rebuildMainMenu()` or `rebuildToolsMenu()` is called, items beyond the array size are silently truncated.

## Impact

**Silent Data Loss:** Adding more than 14 modules that register for the main menu causes some to disappear without warning.

**Hard to Debug:** Developers may not realize their module's menu item is being dropped until they exceed the limit.

**Core Modification Required:** Increasing menu capacity requires modifying `AppUi.cpp` or `ExpertMenuUi.cpp` core files.

**No Dynamic Growth:** Menus cannot expand based on actual module count - they're bounded by compile-time constants.

## Recommended Fix

**Option 1 - Dynamic allocation:**
```cpp
// In AppUi.cpp
static std::vector<ListItem> s_mainMenuItems;
static std::vector<core::ModuleMenuItem> s_mainMenuModuleItems;

void rebuildMainMenu() {
    auto& moduleReg = core::ModuleRegistry::instance();
    uint8_t moduleCount = moduleReg.getMenuItems(
        core::MenuLocation::MAIN_MENU,
        nullptr, 0  // Get count only
    );

    // Resize vectors dynamically
    s_mainMenuItems.resize(moduleCount + MAIN_MENU_FIXED_COUNT);
    s_mainMenuModuleItems.resize(moduleCount);

    // Fill items...
}
```

**Option 2 - Larger static arrays with warning:**
```cpp
static constexpr uint8_t MAIN_MENU_MAX_ITEMS = 32;  // Increased
static ListItem s_mainMenuItems[MAIN_MENU_MAX_ITEMS];

void rebuildMainMenu() {
    uint8_t count = moduleReg.getMenuItems(..., s_mainMenuModuleItems, ...);
    if (count + MAIN_MENU_FIXED_COUNT > MAIN_MENU_MAX_ITEMS) {
        LOG_W(TAG, "Main menu overflow! %d items, max %d", count, MAIN_MENU_MAX_ITEMS);
        // Show toast alert to user
        showToastAlertSticky("Menu overflow - some items hidden");
    }
}
```

**Option 3 - Submenu pagination:**
When menu exceeds limit, automatically create "Page 2", "Page 3" entries:
```cpp
void rebuildMainMenu() {
    uint8_t count = moduleReg.getMenuItems(...);
    uint8_t page1Count = min(count, 14);
    // Create page 1
    // Add "More..." item if count > 14 that opens page 2
}
```

**Recommended:** Option 2 - Larger static arrays with warning. It's the quickest fix (~1 hour), maintains compatibility, and alerts developers when the limit is approached. Option 1 (dynamic allocation) is more elegant but requires more refactoring.

## References

- Main menu: `components/cdc_os_ui/src/AppUi.cpp:52-55`
- Expert menu: `components/cdc_os_ui/src/ExpertMenuUi.cpp:20-28`
- Menu rebuild: `components/cdc_os_ui/src/AppUi.cpp:315-335`
