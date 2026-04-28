---
title: "[MEDIUM] AppUi.cpp is a monolithic component with 780+ lines of mixed responsibilities"
severity: MEDIUM
domain: frontend
lens: component-architecture
labels:
  - "component-architecture"
  - "maintainability"
  - "single-responsibility"
---

## Summary

The `AppUi.cpp` component (782 lines) handles **too many responsibilities** in a single file:
- Lock screen flow
- PIN entry management
- Main menu, Tools menu, Settings menu logic
- Language selection
- Status icon updates (WiFi, BLE, battery)
- Inactivity timeout handling
- Sleep management integration
- Serial command callbacks

**File:** `components/cdc_os_ui/src/AppUi.cpp`
**Lines:** 782 total

## Impact

**Maintainability:**
- Large file is harder to navigate and modify
- Changes to one feature (e.g., settings) risk breaking unrelated features (e.g., lock screen)
- Difficult for new developers to understand the full scope

**Testing:**
- Hard to unit test specific behaviors without pulling in entire file
- No clear boundaries for mock dependencies

**Component Reusability:**
- Lock screen logic is tightly coupled to AppUi
- Menu logic cannot be reused in other contexts
- Status icon updates are scattered throughout the file

**Violation of Single Responsibility Principle:**
The component should either be:
1. A high-level orchestrator that delegates to smaller components
2. Split into multiple focused components

## Evidence

**Lines 48-150:** Static state and helper functions
- 12 view pointers (LockScreen, PinEntry, menus, sliders, etc.)
- Hardware dependency tracking
- State for lock screen throttling

**Lines 151-270:** Status icon management
- `updatePowerStatusIcons()` - WiFi, BLE, battery updates
- `updateLockScreenClock()` - Clock refresh logic
- `clearKeypadBuffer()` - Input handling

**Lines 271-310:** Unlock flow
- `onUnlockRequested()` - PIN entry flow
- `onPinVerify()` - PIN validation
- `onPinSuccess()` - Transition to main menu
- `onInactivityTimeout()` - Lock screen transition

**Lines 311-490:** Menu rebuild and selection handlers
- `rebuildMainMenu()`, `rebuildToolsMenu()`, `rebuildMenuLabels()`
- `onMainMenuSelect()`, `onToolsSelect()`, `onSettingsSelect()`, `onLanguageSelect()`

**Lines 491-710:** `ui_init()` - Initialization function (220 lines!)
- Creates all views
- Sets up callbacks
- Loads NVS data
- Configures sleep manager
- Subscribes to events
- Sets up serial callbacks

**Lines 711-782:** Tick and refresh functions
- `ui_on_modules_ready()`
- `ui_rebuild_menus()`
- `ui_process()` - Main UI loop

## Recommended Fix

**Split into focused components (~1 hour each):**

**1. LockScreenManager (extract lines 271-310, 500-580)**
```cpp
// components/cdc_os_ui/src/LockScreenManager.cpp
class LockScreenManager {
public:
    void init(LockScreenView* view, PinEntryView* pinEntry);
    void onUnlockRequested();
    bool onPinVerify(const char* pin);
    void onPinSuccess();
private:
    LockScreenView* lockScreen_;
    PinEntryView* pinEntry_;
};
```

**2. MenuManager (extract lines 311-490)**
```cpp
// components/cdc_os_ui/src/MenuManager.cpp
class MenuManager {
public:
    void init(ListView* mainMenu, ListView* toolsMenu, ListView* settingsMenu);
    void rebuildAll();
    void onMainMenuSelect(uint16_t index, void* userData);
    void onToolsSelect(uint16_t index, void* userData);
    void onSettingsSelect(uint16_t index, void* userData);
private:
    ListView* mainMenu_;
    ListView* toolsMenu_;
    ListView* settingsMenu_;
};
```

**3. StatusBarManager (extract lines 151-270)**
```cpp
// components/cdc_os_ui/src/StatusBarManager.cpp
class StatusBarManager {
public:
    void init(LockScreenView* view);
    void updatePowerStatusIcons();
    void updateLockScreenClock();
private:
    LockScreenView* lockScreen_;
    int8_t lastMinute_;
    // ... status tracking
};
```

**4. SettingsInitializer (extract lines 491-710, settings-related code)**
```cpp
// components/cdc_os_ui/src/SettingsInitializer.cpp
void initViews(UiDeps& deps);
void setupCallbacks();
void loadNvsSettings();
```

**Resulting AppUi.cpp:**
```cpp
// High-level orchestrator only (~100 lines)
#include "LockScreenManager.h"
#include "MenuManager.h"
#include "StatusBarManager.h"

void ui_init(const UiDeps& deps) {
    static LockScreenManager lockManager;
    static MenuManager menuManager;
    static StatusBarManager statusManager;
    
    // Delegate to managers
    lockManager.init(s_lockScreen, s_pinEntry);
    menuManager.init(s_mainMenu, s_toolsMenu, s_settingsMenu);
    statusManager.init(s_lockScreen);
}

void ui_process(uint32_t nowMs) {
    statusManager.updatePowerStatusIcons();
    statusManager.updateLockScreenClock();
    // ...
}
```

## References

- [Single Responsibility Principle](https://en.wikipedia.org/wiki/Single_responsibility_principle)
- [Component-Based Development](https://en.wikipedia.org/wiki/Component-based_software_engineering)
- [Large Class Code Smell](https://refactoring.com/catalog/largeClass.html)

</content>