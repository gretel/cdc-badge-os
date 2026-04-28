---
title: "[LOW] Settings menu items lack descriptions of what they do"
severity: LOW
domain: information-architecture
lens: help-context
labels:
  - "audit:information-architecture/help-context"
---

## Summary
The settings menu (`components/cdc_os_ui/src/AppUi.cpp`) shows items like "Brightness", "Language", "Timezone", "Auto Sleep", etc., but provides no descriptions of what each setting controls or the valid range of values.

**Evidence** (`components/cdc_os_ui/src/AppUi.cpp` lines 50-90):
```cpp
enum SettingsMenuIdx {
    SETTINGS_IDX_BRIGHTNESS = 0,
    SETTINGS_IDX_LANGUAGE,
    SETTINGS_IDX_TIMEZONE,
    SETTINGS_IDX_AUTO_SLEEP,
    SETTINGS_IDX_BADGE_TEXT,
    SETTINGS_IDX_SET_DATE,
    SETTINGS_IDX_SET_TIME,
    SETTINGS_IDX_CHANGE_PIN,
    SETTINGS_IDX_COUNT
};
```

When users select these settings:
- **Brightness**: Shows slider but no explanation of range (0-100%)
- **Auto Sleep**: Shows interval but no explanation of units or options
- **Timezone**: Shows offset but no explanation of UTC vs local time
- **Badge Text**: No explanation of what appears on the lock screen

## Impact
Users may:
1. Not understand the effect of changing settings
2. Choose inappropriate values (e.g., very short sleep interval)
3. Not know how to revert to defaults

## Evidence
- File: `components/cdc_os_ui/src/AppUi.cpp`
- Lines: 50-90 (settings menu structure)
- No descriptions in menu labels
- No "Info" or help icons next to settings

## Recommended Fix
Add brief descriptions to settings:

1. **Enhanced menu labels**:
   ```
   Brightness (0-100%, current: 50%)
   Language (English / Deutsch)
   Timezone (UTC offset: -12 to +14)
   Auto Sleep (0=never, 1-60 minutes)
   Badge Text (shown on lock screen)
   ```

2. **Info key** (key '3') on each setting:
   Shows detailed explanation and recommended values

3. **Reset to default** option:
   Allow users to restore original settings

## References
- UI/UX best practices for settings: https://developer.apple.com/design/human-interface-guidelines/settings/
