---
title: "[MEDIUM] Hardcoded date format (DD.MM.YYYY) used throughout the codebase"
severity: MEDIUM
domain: i18n
lens: locale-aware-formatting
labels:
  - "audit:i18n/i18n-formatting"
---

## Summary

The codebase uses a hardcoded date format `DD.MM.YYYY` in multiple locations, which is locale-specific (common in German/European locales). Different locales use different date formats:
- US: MM/DD/YYYY
- ISO: YYYY-MM-DD
- UK: DD/MM/YYYY
- Japan: YYYY/MM/DD

**Files and line numbers:**
- `components/cdc_os_ui/src/AppUi.cpp:713` - Lock screen date display
- `components/cdc_os_ui/src/AppUi.cpp:263` - Lock screen clock update
- `components/cdc_os_ui/src/AppUi.cpp:563` - Module-ready clock update
- `components/cdc_os_ui/src/SettingsHandlers.cpp:133` - Settings date display
- `components/cdc_os_ui/src/SleepManager.cpp:164` - Sleep/wakeup date display
- `components/cdc_hal/src/Rtc.cpp:136` - `strftime` with `%Y-%m-%d` format
- `components/serial_cmd/src/SerialCmd.cpp:684` - Serial date output

## Impact

Users in different locales will see dates in a format that may be confusing or incorrect for their region. For example:
- US users may misinterpret `03.04.2025` as March 4th instead of April 3rd
- The format uses `.` as separator which is uncommon in US/UK locales

## Evidence

```cpp
// components/cdc_os_ui/src/AppUi.cpp:713
snprintf(buf, sizeof(buf), "%02d.%02d.%04d", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);

// components/cdc_hal/src/Rtc.cpp:136
strftime(buf, bufLen, "%Y-%m-%d", &timeinfo);
```

## Recommended Fix

1. Add a date format configuration option to settings (e.g., `date_format` with values: `DD.MM.YYYY`, `MM/DD/YYYY`, `YYYY-MM-DD`)
2. Create a helper function `formatDate(struct tm*, char* buf, size_t bufLen, const char* formatStr)` that formats dates according to the configured format
3. Update all date formatting calls to use this helper function
4. Store the user's preferred date format in NVS
5. Default to ISO format (YYYY-MM-DD) for international compatibility, or use the locale's default based on a language setting

## References

- [Unicode LDML Date Formats](https://www.unicode.org/reports/tr35/tr35-dates.html#Date_Field_Symbol_Table)
- [ISO 8601 Date Format](https://en.wikipedia.org/wiki/ISO_8601)
