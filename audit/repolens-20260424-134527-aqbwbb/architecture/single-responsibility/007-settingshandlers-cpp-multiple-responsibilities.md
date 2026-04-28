---
title: "[LOW] SettingsHandlers.cpp combines brightness, sleep, timezone, date/time, PIN, and badge text"
severity: LOW
domain: cdc_os_ui
lens: single-responsibility
labels:
  - "audit:architecture/single-responsibility"
---

## Summary
`components/cdc_os_ui/src/SettingsHandlers.cpp` (282 lines) handles multiple distinct responsibilities:
1. **Brightness** - `onBrightnessSave()`, `onBrightnessChange()`, `brightnessStepCallback()`
2. **Sleep** - `onSleepIntervalSave()`
3. **Timezone** - `onTimezoneSave()`
4. **Date/Time** - `onDateConfirm()`, `onTimeConfirm()`
5. **PIN change** - `onPinChangeComplete()`
6. **Badge text** - `startBadgeTextEdit()`, `showBadgeTextStep()`, `onBadgeNameSave()`, `onBadgeInfoSave()`, `onBadgeInfo2Save()`
7. **NVS persistence** - `saveDisplayField()`

## Impact
- **Moderate coupling**: Changes to any setting type require modifying same file
- **Testing difficulty**: Cannot test brightness without initializing all dependencies
- **Manageable size**: 282 lines is acceptable but could be cleaner

## Evidence
File: `components/cdc_os_ui/src/SettingsHandlers.cpp`
- Lines 14-39: State and forward declarations
- Lines 41-53: Initialization
- Lines 55-62: Badge text processing
- Lines 64-82: Brightness callbacks
- Lines 84-102: Brightness step calculation
- Lines 104-110: Sleep interval save
- Lines 112-139: Timezone save with clock update
- Lines 141-159: Date confirm
- Lines 161-179: Time confirm
- Lines 181-188: PIN change complete
- Lines 190-232: Badge text wizard
- Lines 234-282: Badge text save callbacks and NVS

Key pattern showing mixed concerns:
```cpp
// Brightness
void onBrightnessSave(uint16_t value) {
    if (s_display) {
        s_display->setBacklight(value * 10);
        s_display->saveBacklight();
    }
}

// Sleep
void onSleepIntervalSave(uint16_t value) {
    if (s_sleep) {
        s_sleep->setLightSleepInterval(static_cast<uint32_t>(value) * 60);
    }
}

// Timezone
void onTimezoneSave(uint16_t value) {
    auto* rtc = hal::getRtcInstance();
    if (rtc) {
        rtc->setTimezoneOffset(tzOffset);
        // Update lock screen clock
        ...
    }
}

// Badge text
void onBadgeNameSave(const char* text) {
    if (s_lockScreen) s_lockScreen->setDisplayName(text);
    saveDisplayField("name", text);
    s_badgeTextPendingStep = BADGE_STEP_INFO;
}
```

## Recommended Fix
Consider splitting if the module grows larger:
1. **DisplaySettings** - Brightness and badge text in `components/cdc_os_ui/src/DisplaySettings.cpp`
2. **TimeSettings** - Timezone, date, time in `components/cdc_os_ui/src/TimeSettings.cpp`
3. **SleepSettings** - Sleep interval in `components/cdc_os_ui/src/SleepSettings.cpp`
4. **PinSettings** - PIN change in `components/cdc_os_ui/src/PinSettings.cpp`

For now (282 lines), the file is manageable. Consider refactoring when it exceeds 400 lines.

## References
- Single Responsibility Principle: https://en.wikipedia.org/wiki/Single-responsibility_principle
- Code Smells: https://martinfowler.com/bliki/LongMethod.html
