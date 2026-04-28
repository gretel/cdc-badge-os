---
title: "[MEDIUM] Hardcoded date formats (DD.MM.YYYY) without locale awareness"
severity: MEDIUM
domain: i18n
lens: i18n-formatting
labels:
  - audit:i18n/i18n-formatting
---

## Summary

The codebase uses **hardcoded date formats** (`DD.MM.YYYY`) throughout multiple files without considering the user's locale preferences. Different regions use different date formats:
- **Europe/Germany**: DD.MM.YYYY (e.g., 25.12.2024)
- **USA**: MM/DD/YYYY (e.g., 12/25/2024)
- **ISO 8601/International**: YYYY-MM-DD (e.g., 2024-12-25)

The application currently assumes a European date format for all users, regardless of their locale setting.

### Files Affected

| File | Lines | Usage |
|------|-------|-------|
| `components/cdc_os_ui/src/SettingsHandlers.cpp` | 133, 146-158 | Date display and setting |
| `components/cdc_os_ui/src/AppUi.cpp` | 261, 563, 713 | Lock screen date display |
| `components/cdc_os_ui/src/SleepManager.cpp` | 164 | Sleep timer display |
| `components/cdc_views/src/DateInputView.cpp` | 186, 232 | Date input UI (uses `/` separator) |
| `components/serial_cmd/src/SerialCmd.cpp` | 684, 730-739 | Serial command output |

### Evidence

**SettingsHandlers.cpp:133** - Hardcoded European format:
```cpp
snprintf(buf, sizeof(buf), "%02d.%02d.%04d", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
```

**DateInputView.cpp:232** - Inconsistent format (uses `/` separator):
```cpp
snprintf(dateStr, sizeof(dateStr), "%02d / %02d / %04d", day_, month_, year_);
```

**SerialCmd.cpp:739** - Documentation assumes DD.MM.YYYY:
```cpp
Console::printf("ERROR: Invalid format. Use DD.MM.YYYY\r\n");
```

**Inconsistency Found**: The DateInputView uses `/` as separator (`%02d / %02d / %04d`) while most other components use `.` (`%02d.%02d.%04d`).

## Impact

1. **User Experience**: Users from the US and other regions using MM/DD/YYYY format will find the date format unfamiliar or confusing
2. **Confusion**: The inconsistent separator (`. `vs `/`) between DateInputView and other components may cause user confusion
3. **Limited Localization**: Even though the app supports English and German languages, the date format remains hardcoded to European style

## Recommended Fix

### Option 1: Add Date Format Preference (Recommended)

1. **Add a date format setting** in the timezone/date settings:
   - Create new NVS key for date format preference (0=DD.MM.YYYY, 1=MM/DD/YYYY, 2=YYYY-MM-DD)
   - Add UI option in settings menu to select preferred format

2. **Create a date formatting helper function**:
```cpp
/**
 * \brief Format date according user preference.
 * \param tm Pointer to broken-down time structure.
 * \param buf Output buffer.
 * \param bufLen Size of output buffer.
 * \return void
 */
void formatDateForDisplay(const struct tm* tm, char* buf, size_t bufLen) {
    // Get user's preferred date format from NVS
    // Format: 0=DD.MM.YYYY, 1=MM/DD/YYYY, 2=YYYY-MM-DD
    uint8_t format = getDatePreference();
    
    switch (format) {
        case 0: snprintf(buf, bufLen, "%02d.%02d.%04d", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900); break;
        case 1: snprintf(buf, bufLen, "%02d/%02d/%04d", tm->tm_mon + 1, tm->tm_mday, tm->tm_year + 1900); break;
        case 2: snprintf(buf, bufLen, "%04d-%02d-%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday); break;
    }
}
```

3. **Update all call sites** to use the helper function

### Option 2: Default to ISO 8601 (Simpler)

Use ISO 8601 format (`YYYY-MM-DD`) as the default since it's:
- Locale-neutral and internationally recognized
- Already used by `Rtc.cpp` interface (`getDateStr()` returns "YYYY-MM-DD")
- Sorts correctly alphabetically

**Update all date formatting to use ISO format**:
- SettingsHandlers.cpp:133
- AppUi.cpp:261, 563, 713
- SleepManager.cpp:164
- SerialCmd.cpp:684

### Option 3: Match DateInputView separator

If keeping DD.MM.YYYY format, update DateInputView.cpp to use `.` instead of `/` for consistency:
```cpp
// Change from:
snprintf(dateStr, sizeof(dateStr), "%02d / %02d / %04d", day_, month_, year_);
// To:
snprintf(dateStr, sizeof(dateStr), "%02d.%02d.%04d", day_, month_, year_);
```

## References

- [ISO 8601 Date and Time Format](https://en.wikipedia.org/wiki/ISO_8601)
- [Date Format by Country](https://en.wikipedia.org/wiki/Date_format_by_country)
- The existing `Rtc.cpp` already uses ISO format (`YYYY-MM-DD`) in `getDateStr()` (line 136)
