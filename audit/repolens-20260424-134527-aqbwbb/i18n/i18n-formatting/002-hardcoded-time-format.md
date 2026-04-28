---
title: "[MEDIUM] Hardcoded 24-hour time format (HH:MM) used throughout the codebase"
severity: MEDIUM
domain: i18n
lens: locale-aware-formatting
labels:
  - "audit:i18n/i18n-formatting"
---

## Summary

The codebase uses a hardcoded 24-hour time format `HH:MM` (or `HH:MM:SS`) in multiple locations. While 24-hour format is common in many locales, US and some other regions prefer 12-hour format with AM/PM.

**Files and line numbers:**
- `components/cdc_os_ui/src/AppUi.cpp:711` - Lock screen time display
- `components/cdc_os_ui/src/AppUi.cpp:259` - Lock screen clock update
- `components/cdc_os_ui/src/AppUi.cpp:561` - Module-ready clock update
- `components/cdc_os_ui/src/SettingsHandlers.cpp:131` - Settings time display
- `components/cdc_os_ui/src/SleepManager.cpp:162` - Sleep/wakeup time display
- `components/cdc_hal/src/Rtc.cpp:123` - `strftime` with `%H:%M` format
- `components/serial_cmd/src/SerialCmd.cpp:669` - Serial time output

## Impact

Users in 12-hour format locales (US, Canada, Australia, etc.) may find the 24-hour format less intuitive. The time separator (`:`) is also locale-specific (some locales use `.` or space).

## Evidence

```cpp
// components/cdc_os_ui/src/AppUi.cpp:711
snprintf(buf, sizeof(buf), "%02d:%02d", tm->tm_hour, tm->tm_min);

// components/cdc_hal/src/Rtc.cpp:123
strftime(buf, bufLen, "%H:%M", &timeinfo);
```

## Recommended Fix

1. Add a time format configuration option to settings (e.g., `time_format` with values: `HH:MM` 24-hour, `h:MM AM/PM` 12-hour)
2. Create a helper function `formatTime(struct tm*, char* buf, size_t bufLen, bool use24Hour)` that formats time according to the configured format
3. Update all time formatting calls to use this helper function
4. Store the user's preferred time format in NVS
5. Default to 24-hour format but allow user preference override

## References

- [Unicode LDML Time Formats](https://www.unicode.org/reports/tr35/tr35-dates.html#Time_Patterns)
- [12-hour vs 24-hour clock conventions](https://en.wikipedia.org/wiki/12-hour_clock)
