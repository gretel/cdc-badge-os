---
title: "[MEDIUM] Modules use static view instances creating hidden coupling"
severity: MEDIUM
domain: architecture/coupling
lens: static-view-instances
labels:
  - "audit:architecture/coupling"
---

## Summary
Most modules define static view instances (ListView, SliderView, etc.) that are initialized in the module's `registerInitializer()` callback. These views are stored in static pointers and accessed globally within the module. This creates hidden coupling: the views must be initialized in a specific order, and they can't be easily tested or reused.

**Evidence:**
- `components/grove_led/src/GroveLedModule.cpp` (lines 172-182): Static view pointers
- `components/mod_totp/src/TotpModule.cpp` (lines 544-548): Static ListView instances
- `components/mod_password/src/PasswordModule.cpp` (line 339): Static ListView
- `components/mod_gpg/src/GpgModule.cpp` (lines 206-210): Static view instances
- `components/cdc_os_ui/src/AppUi.cpp` (lines 82-91): Static view pointers for core UI

## Impact
**Implicit initialization order:** Views must be initialized before they're used. If `showMainMenu()` is called before the static views are initialized, the app crashes.

**No dependency injection:** Views are accessed globally via static pointers. Can't substitute mock views for testing.

**Memory management unclear:** Static views live forever. No clear ownership or lifecycle.

**Example fragility:**
```cpp
// GroveLedModule.cpp (lines 172-182)
static ui::ListView* s_mainMenu = nullptr;
static ui::SliderView* s_ledCountSlider = nullptr;
...

// Initialized in registerInitializer (line 636-650)
static void initViews() {
    s_mainMenu = new ui::ListView();
    s_ledCountSlider = new ui::SliderView();
    ...
}

// Called from anywhere (e.g., line 300)
static void showMainMenu() {
    ViewStack::instance().push(s_mainMenu);  // What if s_mainMenu is nullptr?
}
```

## Evidence
**File: `components/grove_led/src/GroveLedModule.cpp` (lines 172-182)**
```cpp
static ui::ListView* s_mainMenu = nullptr;
static ui::SliderView* s_ledCountSlider = nullptr;
static ui::SliderView* s_brightnessSlider = nullptr;
static ui::ListView* s_colorMenu = nullptr;
static ui::ListView* s_effectMenu = nullptr;
static ui::RgbInputView s_rgbInput;  // Static instance, not pointer
```

**File: `components/mod_totp/src/TotpModule.cpp` (lines 544-548)**
```cpp
static ui::ListView s_listView;
static ui::ListView s_digitsMenu;
static ui::ListView s_algoMenu;
static ui::ListView s_periodMenu;
```

**File: `components/cdc_os_ui/src/AppUi.cpp` (lines 82-91)**
```cpp
static LockScreenView* s_lockScreen = nullptr;
static PinEntryView* s_pinEntry = nullptr;
static ListView* s_mainMenu = nullptr;
static ListView* s_toolsMenu = nullptr;
static ListView* s_settingsMenu = nullptr;
static SliderView* s_brightnessSlider = nullptr;
static SliderView* s_sleepSlider = nullptr;
static SliderView* s_timezoneSlider = nullptr;
static ListView* s_languageMenu = nullptr;
static DateInputView* s_dateInput = nullptr;
```

**File: `components/grove_led/src/GroveLedModule.cpp` (lines 636-650)**
```cpp
static void initViews() {
    s_mainMenu = new ui::ListView();
    s_ledCountSlider = new ui::SliderView();
    s_brightnessSlider = new ui::SliderView();
    s_colorMenu = new ui::ListView();
    s_effectMenu = new ui::ListView();
    ...
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        initViews();  // ← Initialization happens here
    });
}
```

## Recommended Fix
**Option 1: Use ViewStack with lazy initialization**
```cpp
class GroveLedModule {
private:
    ui::ListView* getMainMenu() {
        if (!s_mainMenu) {
            s_mainMenu = new ui::ListView();
            s_mainMenu->init(...);
        }
        return s_mainMenu;
    }
};
```

**Option 2: Module owns views as member variables**
```cpp
class GroveLedModule : public core::IModule {
private:
    ui::ListView mainMenu;
    ui::SliderView ledCountSlider;
    ...
};
```

**Option 3: Factory pattern**
```cpp
class ViewFactory {
public:
    static ui::ListView* createMainMenu();
    static ui::SliderView* createBrightnessSlider();
};
```

## References
- `components/grove_led/src/GroveLedModule.cpp` - Example module
- `components/mod_totp/src/TotpModule.cpp` - Another example
- `components/cdc_os_ui/src/AppUi.cpp` - Core UI views
- `components/cdc_ui/include/cdc_ui/ViewStack.h` - View management
