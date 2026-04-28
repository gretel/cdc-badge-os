---
title: "[LOW] Settings menu has 8 items without search or grouping"
severity: LOW
domain: UI/UX
lens: cognitive-overload
labels:
  - "choice-paralysis"
  - "settings"
---

## Summary
The settings menu in `components/cdc_os_ui/src/AppUi.cpp` displays 8 flat list items with no search functionality or visual grouping. As more settings are added, users will need to scroll and scan through increasingly long lists.

**File:** `components/cdc_os_ui/src/AppUi.cpp`
**Lines:** 58-70 (SettingsMenuIdx enum), 360-380 (rebuildMenuLabels)

## Impact
- **Scanning Cost:** Users must visually scan all 8 items to find the one they want
- **Scaling Problem:** Adding more settings will make the list longer without any organization
- **No Quick Access:** Common settings (e.g., Brightness, Date/Time) are buried in a flat list

## Evidence
The settings menu structure:

```cpp
// Line 58-70: Flat enum with no grouping
enum SettingsMenuIdx {
    SETTINGS_IDX_BRIGHTNESS = 0,
    SETTINGS_IDX_LANGUAGE,
    SETTINGS_IDX_TIMEZONE,
    SETTINGS_IDX_AUTO_SLEEP,
    SETTINGS_IDX_BADGE_TEXT,
    SETTINGS_IDX_SET_DATE,
    SETTINGS_IDX_SET_TIME,
    SETTINGS_IDX_CHANGE_PIN,
    SETTINGS_IDX_COUNT  // 8 total items
};

// Line 360-380: All items rendered in one flat list
static void rebuildMenuLabels() {
    s_settingsItems[SETTINGS_IDX_BRIGHTNESS] = {tr(StringId::BRIGHTNESS), 0, false, nullptr};
    s_settingsItems[SETTINGS_IDX_LANGUAGE] = {tr(StringId::LANGUAGE), 0, false, nullptr};
    s_settingsItems[SETTINGS_IDX_TIMEZONE] = {tr(StringId::TIMEZONE), 0, false, nullptr};
    s_settingsItems[SETTINGS_IDX_AUTO_SLEEP] = {tr(StringId::AUTO_SLEEP), 0, false, nullptr};
    s_settingsItems[SETTINGS_IDX_BADGE_TEXT] = {tr(StringId::BADGE_TEXT), 0, false, nullptr};
    s_settingsItems[SETTINGS_IDX_SET_DATE] = {tr(StringId::SET_DATE), 0, false, nullptr};
    s_settingsItems[SETTINGS_IDX_SET_TIME] = {tr(StringId::SET_TIME), 0, false, nullptr};
    s_settingsItems[SETTINGS_IDX_CHANGE_PIN] = {tr(StringId::CHANGE_PIN), 0, false, nullptr};

    if (s_settingsMenu) {
        s_settingsMenu->init(tr(StringId::SETTINGS), s_settingsItems, SETTINGS_IDX_COUNT);
    }
}
```

Items can be grouped logically:
- **Display:** Brightness, Language, Date, Time
- **Power:** Auto Sleep
- **Badge:** Badge Text, Change PIN
- **Timezone:** Timezone

## Recommended Fix
Add visual grouping with section headers:

```cpp
// Option 1: Add section separator items
s_settingsItems[0] = {tr(StringId::BRIGHTNESS), 0, false, nullptr};
s_settingsItems[1] = {"--- Display ---", 0, true, nullptr};  // Section header
s_settingsItems[2] = {tr(StringId::LANGUAGE), 0, false, nullptr};
s_settingsItems[3] = {tr(StringId::BADGE_TEXT), 0, false, nullptr};
s_settingsItems[4] = {"--- Time ---", 0, true, nullptr};
s_settingsItems[5] = {tr(StringId::TIMEZONE), 0, false, nullptr};
s_settingsItems[6] = {tr(StringId::SET_DATE), 0, false, nullptr};
s_settingsItems[7] = {tr(StringId::SET_TIME), 0, false, nullptr};
s_settingsItems[8] = {"--- Security ---", 0, true, nullptr};
s_settingsItems[9] = {tr(StringId::CHANGE_PIN), 0, false, nullptr};
s_settingsItems[10] = {tr(StringId::AUTO_SLEEP), 0, false, nullptr};
```

**Option 2 (Advanced):** Create a submenu structure where "Display", "Time", etc. are expandable sections.

## References
- Information Architecture: Group related items to reduce cognitive load
- Miller's Law: 7 ± 2 items per group for optimal working memory
- Settings Design: Use sections for 6+ settings
