---
title: "[LOW] Settings menu presents options in flat list without grouping"
severity: LOW
domain: UI/UX
lens: cognitive-overload
labels:
  - settings-preferences
  - flat-layout
---

## Summary

The settings menu (`AppUi.cpp:600-620`) presents 8 settings options in a flat list without visual grouping or categorization. While 8 items is manageable, the settings cover distinct categories (display, language, time, sleep, PIN) that could benefit from logical grouping.

**Files:**
- `components/cdc_os_ui/src/AppUi.cpp:600-620` (settings menu initialization)
- `components/cdc_os_ui/include/cdc_os_ui/SettingsHandlers.h:14-42` (settings handlers)

## Impact

**User Experience:** As more settings are added, the flat list will become harder to navigate. Users must scan the entire list to find specific settings rather than navigating by category.

**Evidence:**
From `AppUi.cpp:600-612`, settings are added in a flat structure:
```cpp
s_settingsItems[SETTINGS_IDX_BRIGHTNESS] = {tr(StringId::BRIGHTNESS), 0, false, nullptr};
s_settingsItems[SETTINGS_IDX_LANGUAGE] = {tr(StringId::LANGUAGE), 0, false, nullptr};
s_settingsItems[SETTINGS_IDX_TIMEZONE] = {tr(StringId::TIMEZONE), 0, false, nullptr};
s_settingsItems[SETTINGS_IDX_AUTO_SLEEP] = {tr(StringId::AUTO_SLEEP), 0, false, nullptr};
s_settingsItems[SETTINGS_IDX_BADGE_TEXT] = {tr(StringId::BADGE_TEXT), 0, false, nullptr};
s_settingsItems[SETTINGS_IDX_SET_DATE] = {tr(StringId::SET_DATE), 0, false, nullptr};
s_settingsItems[SETTINGS_IDX_SET_TIME] = {tr(StringId::SET_TIME), 0, false, nullptr};
s_settingsItems[SETTINGS_IDX_CHANGE_PIN] = {tr(StringId::CHANGE_PIN), 0, false, nullptr};
```

These naturally group into categories:
- **Display:** Brightness, Badge Text
- **Time:** Timezone, Date, Time
- **System:** Language, Auto Sleep, PIN

## Recommended Fix

Implement one of these approaches:

1. **Add section headers** between logical groups in the list (e.g., "--- Display ---", "--- Time ---")
2. **Create submenu structure** with top-level categories that expand to show settings
3. **Add icons** to visually distinguish categories

Quick fix: Insert section divider items in the settings array with a distinct marker (e.g., "===" prefix).

## References

- Nielsen Norman Group: "Settings Design: Categorization and Hierarchy"
- Material Design: "Settings" page patterns
