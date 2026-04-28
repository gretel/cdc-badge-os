---
title: "[MEDIUM] AppUi views allocated with new but never freed for application lifetime"
severity: MEDIUM
domain: performance/memory
lens: memory-management
labels:
  - "memory-leak"
  - "embedded"
---

## Summary

In `components/cdc_os_ui/src/AppUi.cpp`, the `AppUi::init()` method allocates 8 UI view objects with `new` but there is no corresponding cleanup/destructor to free them. These views persist in memory for the entire application lifetime.

**Location**: `components/cdc_os_ui/src/AppUi.cpp:585-674` (allocations), no destructor or cleanup method

```cpp
// Line 585: PinEntryView
s_pinEntry = new PinEntryView();

// Line 591-596: Main and Tools menus
s_mainMenu = new ListView();
s_toolsMenu = new ListView();

// Line 611: Settings menu
s_settingsMenu = new ListView();

// Line 619-623: Brightness slider
s_brightnessSlider = new SliderView();

// Line 630: Sleep slider
s_sleepSlider = new SliderView();

// Line 638: Timezone slider
s_timezoneSlider = new SliderView();

// Line 650: Language menu
s_languageMenu = new ListView();

// Line 658-668: Date/Time input views
s_dateInput = new DateInputView();
s_timeInput = new TimeInputView();

// Line 674: PIN change view
s_pinChangeView = new PinChangeView();

// No destructor or cleanup method exists!
```

## Impact

- **Memory leak**: 8 UI view objects are allocated once during initialization and never freed for the lifetime of the application.
- **Accumulated memory**: While this is a one-time allocation (not repeated like TOTP/Password modules), the total memory footprint includes:
  - 4 ListView instances (~64-128 bytes each)
  - 3 SliderView instances (~48-96 bytes each)
  - 1 PinEntryView instance (~32-64 bytes)
  - 1 DateInputView instance (~32-64 bytes)
  - 1 TimeInputView instance (~32-64 bytes)
  - 1 PinChangeView instance (~32-64 bytes)
  - Total: ~400-800 bytes of unreclaimed heap memory
- **Consistency issue**: Other modules (NVS Edit, TOTP, Password) clean up their views in `stop()`, but AppUi doesn't.
- **Embedded context**: On ESP32-S3 with limited heap (typically 2-8MB PSRAM + 512KB SRAM), every byte counts for long-running firmware.

### Context

The `AppUi` class acts as the main UI controller and is initialized once during application startup. The views are stored in static member pointers and reused throughout the application. However, there's no mechanism to free them when the application shuts down or when `AppUi::init()` is called again.

## Evidence

**File**: `components/cdc_os_ui/src/AppUi.cpp`

**Line 585-589** (PinEntryView):
```cpp
s_pinEntry = new PinEntryView();
s_pinEntry->init(tr(StringId::ENTER_PIN), 8, 3);
s_pinEntry->setOnVerify(onPinVerify);
s_pinEntry->setOnSuccess(onPinSuccess);
```

**Line 591-596** (Main/Tools menus):
```cpp
s_mainMenu = new ListView();
s_mainMenu->setOnSelect(onMainMenuSelect);

s_toolsMenu = new ListView();
s_toolsMenu->setOnSelect(onToolsSelect);
```

**Line 611-613** (Settings menu):
```cpp
s_settingsMenu = new ListView();
s_settingsMenu->init(tr(StringId::SETTINGS), s_settingsItems, SETTINGS_IDX_COUNT);
s_settingsMenu->setOnSelect(onSettingsSelect);
```

**Line 619-623** (Brightness slider):
```cpp
s_brightnessSlider = new SliderView();
uint16_t currentBrightness = s_deps.display ? s_deps.display->getBacklight() / 10 : 50;
s_brightnessSlider->init(tr(StringId::BRIGHTNESS), 0, 100, currentBrightness, 1, "%");
```

**Line 658-668** (Date/Time input):
```cpp
s_dateInput = new DateInputView();
s_dateInput->init(tr(StringId::SET_DATE), ...);

s_timeInput = new TimeInputView();
s_timeInput->init(tr(StringId::SET_TIME), ...);
```

**No destructor found**: The `AppUi` class has no destructor or cleanup method to free these views.

## Recommended Fix

### Option 1: Add destructor to AppUi class (recommended)

Add a destructor to `AppUi` class that frees all allocated views:

```cpp
// In AppUi.h
class AppUi {
public:
    ~AppUi();  // Add destructor
    // ... rest of class
};

// In AppUi.cpp
AppUi::~AppUi() {
    delete s_pinEntry;
    s_pinEntry = nullptr;
    delete s_mainMenu;
    s_mainMenu = nullptr;
    delete s_toolsMenu;
    s_toolsMenu = nullptr;
    delete s_settingsMenu;
    s_settingsMenu = nullptr;
    delete s_brightnessSlider;
    s_brightnessSlider = nullptr;
    delete s_sleepSlider;
    s_sleepSlider = nullptr;
    delete s_timezoneSlider;
    s_timezoneSlider = nullptr;
    delete s_languageMenu;
    s_languageMenu = nullptr;
    delete s_dateInput;
    s_dateInput = nullptr;
    delete s_timeInput;
    s_timeInput = nullptr;
    delete s_pinChangeView;
    s_pinChangeView = nullptr;
    delete s_lockScreen;
    s_lockScreen = nullptr;
}
```

### Option 2: Add explicit cleanup method

If `AppUi` is a singleton and destructor won't be called:

```cpp
// In AppUi.h
class AppUi {
public:
    void cleanup();  // Add cleanup method
    // ... rest of class
};

// In AppUi.cpp
void AppUi::cleanup() {
    delete s_pinEntry;
    s_pinEntry = nullptr;
    // ... delete all other views
}

// Call cleanup during application shutdown
```

### Option 3: Use static instances (most efficient for embedded)

Since these views are created once and reused, use static instances:

```cpp
// Change declarations in AppUi.h
static PinEntryView s_pinEntry;
static ListView s_mainMenu;
static ListView s_toolsMenu;
static ListView s_settingsMenu;
static SliderView s_brightnessSlider;
static SliderView s_sleepSlider;
static SliderView s_timezoneSlider;
static ListView s_languageMenu;
static DateInputView s_dateInput;
static TimeInputView s_timeInput;
static PinChangeView s_pinChangeView;
static LockScreenView s_lockScreen;

// Change allocations in AppUi.cpp (no new)
s_pinEntry.init(tr(StringId::ENTER_PIN), 8, 3);  // No "new"
// ... etc
```

This approach:
- Eliminates all dynamic allocation
- Consistent with other modules (TOTP, Password, NVS Edit)
- No cleanup needed
- Most memory-efficient for embedded context

## References

- Related to: Issues #001 (TOTP), #004 (Password), #005 (NVS Edit) memory leak patterns
- ESP32-S3 heap: 512KB SRAM + typically 2-8MB PSRAM
- Consistency: Follow same pattern as other modules for cleanup

</content>