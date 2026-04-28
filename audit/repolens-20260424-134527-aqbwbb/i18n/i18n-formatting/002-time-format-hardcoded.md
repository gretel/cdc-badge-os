---
title: "[HIGH] Hardcoded Time Format - HH:MM with colon separator"
severity: HIGH
domain: i18n
lens: locale-formatting
labels:
  - audit:i18n/i18n-formatting
---

## Summary
Time formatting uses hardcoded `HH:MM` format with colon separator in multiple locations. Some European locales (e.g., Germany, Finland) conventionally use `HH.MM` or `HHhMM` as time separator.

**Files:**
- `components/cdc_os_ui/src/SettingsHandlers.cpp:131`
- `components/cdc_os_ui/src/SleepManager.cpp:162`
- `components/cdc_hal/src/Rtc.cpp:123` (strftime with `%H:%M`)

**Evidence:**
```cpp
// SettingsHandlers.cpp:131
snprintf(buf, sizeof(buf), "%02d:%02d", tm->tm_hour, tm->tm_min);

// SleepManager.cpp:162
snprintf(buf, sizeof(buf), "%02d:%02d", tm->tm_hour, tm->tm_min);

// Rtc.cpp:123
strftime(buf, bufLen, "%H:%M", &timeinfo);
```

## Impact
- Users from locales that conventionally use different time separators may find the format unfamiliar
- Time separators vary by locale: `:` (US/UK), `.` (Germany/Finland), `h` (France), etc.
- The `Rtc.cpp` implementation uses `strftime` which respects `LC_TIME` locale, but the C locale is likely not set

## Recommended Fix
1. Add time format preference to settings (colon vs dot vs other separators)
2. Create a locale-aware time formatting function:

```cpp
// In components/cdc_hal/src/Rtc.h
void formatTimeLocaleAware(struct tm* tm, char* buf, size_t bufLen);

// Implementation in Rtc.cpp
void formatTimeLocaleAware(struct tm* tm, char* buf, size_t bufLen) {
    // Read time format preference from NVS (default: colon for EN, dot for DE)
    // Format options: 0=HH:MM, 1=HH.MM, 2=HHhMM
    uint8_t format = 0; // Load from NVS based on language setting
    switch (format) {
        case 0: snprintf(buf, bufLen, "%02d:%02d", tm->tm_hour, tm->tm_min); break;
        case 1: snprintf(buf, bufLen, "%02d.%02d", tm->tm_hour, tm->tm_min); break;
        case 2: snprintf(buf, bufLen, "%02dh%02d", tm->tm_hour, tm->tm_min); break;
    }
}
```

3. Update all callers to use the new function instead of direct `snprintf` or `strftime`

## References
- Time notation by country: https://en.wikipedia.org/wiki/12-hour_clock#By_country
- Locale conventions for time separators
