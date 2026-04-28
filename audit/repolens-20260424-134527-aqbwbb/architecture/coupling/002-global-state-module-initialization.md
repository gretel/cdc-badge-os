---
title: "[MEDIUM] Global mutable state in AppUi.cpp creates coupling between UI and modules"
severity: MEDIUM
domain: Architecture/Coupling
lens: architecture/coupling
labels:
  - "audit:architecture/coupling"
---

## Summary
The `AppUi.cpp` file contains approximately 15 global static variables that store UI state and are accessed/modifed by multiple functions. These globals create implicit coupling between menu functions, module callbacks, and event handlers.

**Evidence locations:**
- `components/cdc_os_ui/src/AppUi.cpp:89-108` - Global UI state declarations
- `components/cdc_os_ui/src/AppUi.cpp:309` - `rebuildMainMenu()` accesses `s_mainMenuPluginCount`
- `components/cdc_os_ui/src/AppUi.cpp:333` - `rebuildToolsMenu()` accesses `s_toolsModuleCount`

**Global state variables:**
```cpp
// Line 89-91: View pointers
static LockScreenView* s_lockScreen = nullptr;
static PinEntryView* s_pinEntry = nullptr;
static ListView* s_mainMenu = nullptr;
static ListView* s_toolsMenu = nullptr;

// Line 100-108: Menu backing storage
static ListItem s_mainMenuItems[MAIN_MENU_MAX_ITEMS];
static core::ModuleMenuItem s_mainMenuModuleItems[MAIN_MENU_MAX_ITEMS];
static uint8_t s_mainMenuPluginCount = 0;
static ListItem s_toolsItems[TOOLS_MAX_ITEMS];
static core::ModuleMenuItem s_toolsModuleItems[TOOLS_MAX_ITEMS];
static uint8_t s_toolsModuleCount = 0;

// Line 114-119: Status cache
static int8_t s_lastMinute = -1;
static bool s_lastUsbConnected = false;
static bool s_lastCharging = false;
static bool s_lastWifiConnected = false;
static bool s_lastBleEnabled = false;
static bool s_lastBatteryPresent = false;
```

## Impact
- **Hidden state dependencies**: Functions like `onMainMenuSelect()` depend on `s_mainMenuPluginCount` being set by `rebuildMainMenu()`
- **Temporal coupling**: Menu rebuild must happen before menu selection works
- **Difficult to test**: Tests must manipulate global state before running functions
- **Race conditions**: Status icons updated from multiple places (line 191-227)

## Evidence
**Code showing coupling:**
```cpp
// components/cdc_os_ui/src/AppUi.cpp:387-399
static void onMainMenuSelect(uint16_t index, void* userData) {
    (void)userData;

    // Depends on s_mainMenuPluginCount set by rebuildMainMenu()
    if (index < s_mainMenuPluginCount) {
        auto& item = s_mainMenuModuleItems[index];  // Uses global array
        if (item.getView) {
            IView* view = item.getView();
            if (view) ViewStack::instance().push(view);
        }
        return;
    }
    // ...
}
```

```cpp
// components/cdc_os_ui/src/AppUi.cpp:316-328
void rebuildMainMenu() {
    auto& moduleReg = core::ModuleRegistry::instance();

    s_mainMenuPluginCount = moduleReg.getMenuItems(  // Modifies global
        core::MenuLocation::MAIN_MENU,
        s_mainMenuModuleItems,  // Writes to global array
        MAIN_MENU_MAX_ITEMS - MAIN_MENU_FIXED_COUNT
    );
    // ...
}
```

**Status icon updates from multiple locations:**
```cpp
// Line 191-227: updatePowerStatusIcons()
if (usbConnected != s_lastUsbConnected) {
    if (usbConnected) s_lockScreen->addStatusIcon(StatusIcon::USB);
    else s_lockScreen->removeStatusIcon(StatusIcon::USB);
    s_lastUsbConnected = usbConnected;  // Modifies global
}
```

## Recommended Fix
Encapsulate UI state in a **`UiContext` class**:

1. **Create UiContext class:**
```cpp
// components/cdc_os_ui/include/cdc_os_ui/UiContext.h
class UiContext {
public:
    // Views
    LockScreenView* lockScreen() const { return lockScreen_; }
    PinEntryView* pinEntry() const { return pinEntry_; }
    ListView* mainMenu() const { return mainMenu_; }
    ListView* toolsMenu() const { return toolsMenu_; }

    // Menu data
    uint8_t getMainMenuPluginCount() const { return mainMenuPluginCount_; }
    const core::ModuleMenuItem* getMainMenuModuleItems() const { return mainMenuModuleItems_; }
    uint8_t getToolsModuleCount() const { return toolsModuleCount_; }
    const core::ModuleMenuItem* getToolsModuleItems() const { return toolsModuleItems_; }

    // Status cache
    struct {
        int8_t lastMinute = -1;
        bool lastUsbConnected = false;
        bool lastCharging = false;
        bool lastWifiConnected = false;
        bool lastBleEnabled = false;
        bool lastBatteryPresent = false;
    } statusCache() const { return statusCache_; }

private:
    LockScreenView* lockScreen_ = nullptr;
    // ... other members
    struct { /* ... */ } statusCache_;
};

// Global accessor
UiContext& getUiContext();
```

2. **Refactor functions to use UiContext:**
```cpp
// Before:
static void onMainMenuSelect(uint16_t index, void* userData) {
    if (index < s_mainMenuPluginCount) {
        auto& item = s_mainMenuModuleItems[index];
        // ...
    }
}

// After:
static void onMainMenuSelect(uint16_t index, void* userData) {
    auto& ctx = getUiContext();
    if (index < ctx.getMainMenuPluginCount()) {
        const auto* items = ctx.getMainMenuModuleItems();
        auto& item = items[index];
        // ...
    }
}
```

3. **Update rebuild functions:**
```cpp
void rebuildMainMenu() {
    auto& ctx = getUiContext();
    auto& moduleReg = core::ModuleRegistry::instance();

    // Update context instead of globals
    ctx.setMainMenuPluginCount(moduleReg.getMenuItems(...));
    // ...
}
```

## References
- [Encapsulation on Wikipedia](https://en.wikipedia.org/wiki/Encapsulation_(computer_programming))
- [State Pattern on Wikipedia](https://en.wikipedia.org/wiki/State_pattern)
