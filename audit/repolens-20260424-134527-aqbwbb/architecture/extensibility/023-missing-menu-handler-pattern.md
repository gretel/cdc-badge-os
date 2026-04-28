---
title: "[MEDIUM] Missing Menu Handler Strategy Pattern"
severity: MEDIUM
domain: architecture/extensibility
lens: menu-handlers
labels:
  - "audit:architecture/extensibility"
---

## Summary
Menu item selection is handled through large switch statements in `AppUi.cpp` that route to specific views or functions. Adding new menu items requires modifying these switch statements, violating the Open/Closed Principle. There is no handler registry or strategy pattern to allow dynamic menu action registration.

**Files affected:**
- `components/cdc_os_ui/src/AppUi.cpp` (lines 416-472) - switch-based menu routing
- `components/cdc_os_ui/src/AppUi.cpp:419-424` - Tools menu switch
- `components/cdc_os_ui/src/AppUi.cpp:444-472` - Settings menu switch
- `components/cdc_os_ui/src/ExpertMenuUi.cpp:261` - Expert menu switch
- `components/cdc_os_ui/src/BluetoothMenuUi.cpp:172` - Bluetooth menu switch
- `components/cdc_os_ui/src/WifiMenuUi.cpp:257,488` - WiFi menu switches

## Impact
- **Menu extensibility**: Adding new settings or tools requires modifying `AppUi.cpp`
- **Module coupling**: Modules cannot register their own menu handlers without core changes
- **Code duplication**: Similar switch patterns repeated in multiple menu UI files
- **Testing difficulty**: Hard to inject mock handlers for unit testing menu flow
- **Maintenance burden**: Large switch statements grow with every new feature

## Evidence

### Hardcoded Switch in Settings Menu
`components/cdc_os_ui/src/AppUi.cpp:441-472`:
```cpp
static void onSettingsSelect(uint16_t index, void* userData) {
    (void)userData;

    switch (index) {
        case SETTINGS_IDX_BRIGHTNESS:
            ViewStack::instance().push(s_brightnessSlider);
            break;
        case SETTINGS_IDX_LANGUAGE:
            ViewStack::instance().push(s_languageMenu);
            break;
        case SETTINGS_IDX_TIMEZONE:
            ViewStack::instance().push(s_timezoneSlider);
            break;
        case SETTINGS_IDX_AUTO_SLEEP:
            ViewStack::instance().push(s_sleepSlider);
            break;
        case SETTINGS_IDX_BADGE_TEXT:
            settings::startBadgeTextEdit();
            break;
        case SETTINGS_IDX_SET_DATE:
            ViewStack::instance().push(s_dateInput);
            break;
        case SETTINGS_IDX_SET_TIME:
            ViewStack::instance().push(s_timeInput);
            break;
        case SETTINGS_IDX_CHANGE_PIN:
            if (s_pinChangeView) {
                s_pinChangeView->init(core::PinManager::BADGE_PIN_MIN, core::PinManager::BADGE_PIN_MAX);
                ViewStack::instance().push(s_pinChangeView);
            }
            break;
    }
}
```

### Hardcoded Switch in Tools Menu
`components/cdc_os_ui/src/AppUi.cpp:416-434`:
```cpp
static void onToolsSelect(uint16_t index, void* userData) {
    (void)userData;

    switch (index) {
        case 0: showModulesView(); return;
        case 1: showWifiMainMenu(); return;
        case 2: showBluetoothMenu(); return;
        case 3: showExpertMenu(); return;
    }

    uint8_t moduleIdx = index - TOOLS_FIXED_COUNT;
    if (moduleIdx < s_toolsModuleCount) {
        auto& item = s_toolsModuleItems[moduleIdx];
        if (item.getView) {
            IView* view = item.getView();
            if (view) ViewStack::instance().push(view);
        }
    }
}
```

### Repeated Pattern in Other Menus
`components/cdc_os_ui/src/ExpertMenuUi.cpp:261`:
```cpp
switch (index) {
    case 0: showModulesView(); break;
    case 1: showLogErrorView(); break;
    case 2: showTr01CacheRebuild(); break;
    case 3: showTr01CacheCleanup(); break;
    case 4: showGpgResetConfirm(); break;
    case 5: showNvsEdit(); break;
}
```

`components/cdc_os_ui/src/WifiMenuUi.cpp:257`:
```cpp
switch (index) {
    case 0: showWifiScanView(); break;
    case 1: showWifiManualAdd(); break;
    case 2: showWifiConfig(); break;
    case 3: showWifiStatus(); break;
    case 4: showWifiDisconnect(); break;
}
```

### No Handler Registry
There is no interface like:
```cpp
// Missing - should exist:
class IMenuHandler {
public:
    virtual ~IMenuHandler() = default;
    virtual void handle(uint16_t index, void* userData) = 0;
};

class MenuRegistry {
public:
    static MenuRegistry& instance();
    void registerHandler(const char* menuName, IMenuHandler* handler);
    void handleMenu(const char* menuName, uint16_t index, void* userData);
};
```

## Recommended Fix

### Create Menu Handler Interface
```cpp
// components/cdc_ui/include/cdc_ui/MenuHandler.h
#pragma once
#include "cdc_ui/IView.h"
#include <cstdint>

namespace cdc::ui {

/**
 * Menu handler interface for extensible menu actions
 */
class IMenuHandler {
public:
    virtual ~IMenuHandler() = default;
    
    /**
     * Handle menu selection
     * @param index Selected item index
     * @param userData Optional user data from menu item
     */
    virtual void handle(uint16_t index, void* userData) = 0;
    
    /**
     * Get handler name for debugging
     */
    virtual const char* getName() const { return "IMenuHandler"; }
};

/**
 * Handler registry for dynamic menu routing
 */
class MenuRegistry {
public:
    static MenuRegistry& instance();
    
    /**
     * Register a handler for a menu
     * @param menuName Menu name (e.g., "settings", "tools")
     * @param handler Handler instance (must remain valid)
     */
    void registerHandler(const char* menuName, IMenuHandler* handler);
    
    /**
     * Handle a menu selection
     * @param menuName Menu name
     * @param index Selected item index
     * @param userData Optional user data
     */
    void handle(const char* menuName, uint16_t index, void* userData);
    
    /**
     * Get all registered menu names
     */
    void getMenuNames(char** names, uint8_t* count, uint8_t maxCount);
};

// Convenience handler base class
class MenuHandlerBase : public IMenuHandler {
public:
    using HandleFunc = void(*)(uint16_t index, void* userData);
    
    MenuHandlerBase(const char* name, HandleFunc func) 
        : name_(name), func_(func) {}
    
    void handle(uint16_t index, void* userData) override {
        if (func_) func_(index, userData);
    }
    
    const char* getName() const override { return name_; }
    
private:
    const char* name_;
    HandleFunc func_;
};

} // namespace cdc::ui
```

### Implement Built-in Handlers
```cpp
// components/cdc_os_ui/src/SettingsHandler.cpp
#include "cdc_ui/MenuHandler.h"

namespace cdc::ui {

class SettingsHandler : public IMenuHandler {
public:
    void handle(uint16_t index, void* userData) override {
        switch (index) {
            case SETTINGS_IDX_BRIGHTNESS:
                ViewStack::instance().push(s_brightnessSlider);
                break;
            case SETTINGS_IDX_LANGUAGE:
                ViewStack::instance().push(s_languageMenu);
                break;
            case SETTINGS_IDX_TIMEZONE:
                ViewStack::instance().push(s_timezoneSlider);
                break;
            case SETTINGS_IDX_AUTO_SLEEP:
                ViewStack::instance().push(s_sleepSlider);
                break;
            case SETTINGS_IDX_BADGE_TEXT:
                settings::startBadgeTextEdit();
                break;
            case SETTINGS_IDX_SET_DATE:
                ViewStack::instance().push(s_dateInput);
                break;
            case SETTINGS_IDX_SET_TIME:
                ViewStack::instance().push(s_timeInput);
                break;
            case SETTINGS_IDX_CHANGE_PIN:
                if (s_pinChangeView) {
                    s_pinChangeView->init(core::PinManager::BADGE_PIN_MIN, 
                                          core::PinManager::BADGE_PIN_MAX);
                    ViewStack::instance().push(s_pinChangeView);
                }
                break;
        }
    }
    
    const char* getName() const override { return "Settings"; }
};

static SettingsHandler s_settingsHandler;

void registerSettingsHandler() {
    MenuRegistry::instance().registerHandler("settings", &s_settingsHandler);
}

} // namespace cdc::ui
```

### Create Module-Specific Handlers
```cpp
// components/mod_totp/src/TotpMenuHandler.cpp
#include "cdc_ui/MenuHandler.h"

namespace cdc::mod_totp {

class TotpHandler : public IMenuHandler {
public:
    void handle(uint16_t index, void* userData) override {
        switch (index) {
            case 0: showTotpList(); break;
            case 1: showTotpAdd(); break;
            case 2: showTotpSettings(); break;
        }
    }
    
    const char* getName() const override { return "TOTP"; }
};

static TotpHandler s_totpHandler;

void mod_totp_register() {
    // Register with main menu registry
    auto& registry = cdc::ui::MenuRegistry::instance();
    registry.registerHandler("totp", &s_totpHandler);
}

} // namespace cdc::mod_totp
```

### Refactor AppUi to Use Registry
```cpp
// components/cdc_os_ui/src/AppUi.cpp
#include "cdc_ui/MenuHandler.h"

static void onSettingsSelect(uint16_t index, void* userData) {
    auto& registry = ui::MenuRegistry::instance();
    registry.handle("settings", index, userData);
}

static void onToolsSelect(uint16_t index, void* userData) {
    auto& registry = ui::MenuRegistry::instance();
    registry.handle("tools", index, userData);
}
```

### Initialize Handlers at Startup
```cpp
// components/cdc_os_ui/src/AppUi.cpp (init function)
void AppUi::init() {
    // ... existing init code ...
    
    // Register menu handlers
    ui::registerSettingsHandler();
    ui::registerToolsHandler();
    ui::registerExpertHandler();
    
    // Modules register their handlers
    mod_totp_register();
    mod_password_register();
    mod_gpg_register();
}
```

### Enable Dynamic Handler Registration
```cpp
// components/mod_custom/src/CustomMenuHandler.cpp
#include "cdc_ui/MenuHandler.h"

namespace cdc::mod_custom {

class CustomHandler : public ui::IMenuHandler {
public:
    void handle(uint16_t index, void* userData) override {
        // Custom menu logic
    }
};

void mod_custom_register() {
    static CustomHandler handler;
    ui::MenuRegistry::instance().registerHandler("custom", &handler);
}

} // namespace cdc::mod_custom
```

## References
- Strategy Pattern: https://refactoring.guru/design-patterns/strategy
- Command Pattern for menu actions: https://refactoring.guru/design-patterns/command
- Menu Model Pattern: https://en.wikipedia.org/wiki/Menu_(computing)#Menu_model
- Dependency Injection for handlers: https://martinfowler.com/articles/injection.html

</content>