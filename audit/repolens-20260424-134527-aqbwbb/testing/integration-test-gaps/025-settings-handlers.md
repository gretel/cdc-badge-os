---
title: "[LOW] SettingsHandlers and settings persistence lacks integration tests"
severity: LOW
domain: ui
lens: integration-test-gaps
labels:
  - "audit:testing/integration-test-gaps"
  - "component:cdc_os_ui"
  - "area:settings"
---

## Summary
The `SettingsHandlers` component (`components/cdc_os_ui/src/SettingsHandlers.cpp`) handles settings persistence (brightness, language, timezone, auto-sleep), but **no integration tests** verify that settings are saved to NVS and loaded correctly.

## Impact
- **Save**: Settings may not be saved correctly
- **Load**: Settings may not load correctly
- **Defaults**: Default values may be wrong
- **Validation**: Invalid values may not be validated

## Evidence

**SettingsHandlers API** (`components/cdc_os_ui/src/SettingsHandlers.cpp`):
```cpp
void settingsBrightnessChanged(uint8_t value);
void settingsLanguageChanged(Language lang);
void settingsSleepChanged(uint8_t minutes);
void settingsTextChanged(const char* text);

// Load functions
uint8_t settingsGetBrightness();
Language settingsGetLanguage();
uint8_t settingsGetSleepMinutes();
```

**NV storage** (`components/cdc_os_ui/src/SettingsHandlers.cpp:50-150`):
```cpp
void settingsBrightnessChanged(uint8_t value) {
    nvs_open("settings", NVS_READWRITE, &handle);
    nvs_set_u8(handle, "brightness", value);
    nvs_commit(handle);
}

uint8_t settingsGetBrightness() {
    nvs_open("settings", NVS_READWRITE, &handle);
    uint8_t brightness = nvs_get_u8(handle, "brightness", &def);
    return brightness;
}
```

**Settings in AppUi** (`components/cdc_os_ui/src/AppUi.cpp:200-300`):
```cpp
// Settings menu uses handlers
void showSettingsMenu() {
    static ListItem items[] = {
        {"Brightness", 0, false, (void*)0},
        {"Language", 0, false, (void*)1},
        {"Auto Sleep", 0, false, (void*)2},
        // ...
    };
}
```

**Current test coverage**: None

## Recommended Fix

Create integration test `test_settings_handlers/` that verifies:

1. **Save/Load**: Settings save and load correctly
2. **Defaults**: Default values used when no settings
3. **Persistence**: Settings survive reboot (NVS commit)
4. **Validation**: Invalid values clamped to range

**Test structure** (example):
```cpp
// test/test_settings_handlers/test_settings.cpp
#include "cdc_os_ui/SettingsHandlers.h"

void test_settings_save_load() {
    // Save settings
    settingsBrightnessChanged(128);
    settingsLanguageChanged(Language::DE);
    settingsSleepChanged(10);
    
    // Load settings
    ASSERT_EQ(settingsGetBrightness(), 128);
    ASSERT_EQ(settingsGetLanguage(), Language::DE);
    ASSERT_EQ(settingsGetSleepMinutes(), 10);
}

void test_settings_defaults() {
    // Clear NVS (simulating fresh install)
    
    // Load should return defaults
    ASSERT_EQ(settingsGetBrightness(), 192);  // Default
    ASSERT_EQ(settingsGetLanguage(), Language::EN);  // Default
}
```

## References
- [SettingsHandlers implementation](components/cdc_os_ui/src/SettingsHandlers.cpp)
- [AppUi settings menu](components/cdc_os_ui/src/AppUi.cpp:200)

</content>