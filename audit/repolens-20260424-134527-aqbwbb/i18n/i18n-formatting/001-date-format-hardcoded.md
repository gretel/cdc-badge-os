---
title: "[HIGH] Hardcoded Date Format - DD.MM.YYYY in Settings and SleepManager"
severity: HIGH
domain: i18n
lens: locale-formatting
labels:
  - audit:i18n/i18n-formatting
---

## Summary
Date formatting uses hardcoded `DD.MM.YYYY` pattern in `SettingsHandlers.cpp` (line 133) and `SleepManager.cpp` (line 164) instead of locale-aware formatting. The format assumes European convention without providing alternatives for other locales.

**Files:**
- `components/cdc_os_ui/src/SettingsHandlers.cpp:133`
- `components/cdc_os_ui/src/SleepManager.cpp:164`

**Evidence:**
```cpp
// SettingsHandlers.cpp:133
snprintf(buf, sizeof(buf), "%02d.%02d.%04d", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);

// SleepManager.cpp:164
snprintf(buf, sizeof(buf), "%02d.%02d.%04d", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
```

## Impact
- Users from different locales (e.g., US with MM/DD/YYYY, ISO 8601 with YYYY-MM-DD) will see dates in a format they may not understand
- Potential for confusion when reading dates (e.g., 04/05/2025 could be April 5th or May 4th depending on locale)
- Inconsistent with internationalization best practices for embedded systems

## Recommended Fix
1. Add a date format preference to the settings (e.g., `DATE_FORMAT_DDMM`, `DATE_FORMAT_MMDD`, `DATE_FORMAT_YYYYMM`)
2. Create a locale-aware date formatting function that respects user preference:

```cpp
// In components/cdc_os_ui/src/SettingsHandlers.h
void formatDateLocaleAware(struct tm* tm, char* buf, size_t bufLen);

// Implementation
void formatDateLocaleAware(struct tm* tm, char* buf, size_t bufLen) {
    // Read date format preference from NVS (default: DD.MM.YYYY for DE locale)
    // Format options: 0=DD.MM.YYYY, 1=MM/DD/YYYY, 2=YYYY-MM-DD
    uint8_t format = 0; // Load from NVS
    switch (format) {
        case 0: snprintf(buf, bufLen, "%02d.%02d.%04d", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900); break;
        case 1: snprintf(buf, bufLen, "%02d/%02d/%04d", tm->tm_mon + 1, tm->tm_mday, tm->tm_year + 1900); break;
        case 2: snprintf(buf, bufLen, "%04d-%02d-%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday); break;
    }
}
```

3. Update both `SettingsHandlers.cpp` and `SleepManager.cpp` to use the new function

## References
- ISO 8601 date format: https://en.wikipedia.org/wiki/ISO_8601
- Date format by country: https://en.wikipedia.org/wiki/Date_format_by_country
