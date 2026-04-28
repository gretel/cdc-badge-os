---
title: "[LOW] Timezone handling assumes local time without user configuration"
severity: LOW
domain: i18n
lens: locale-aware-formatting
labels:
  - "audit:i18n/i18n-formatting"
---

## Summary

The codebase uses `localtime()` to convert UTC time to local time, but the timezone offset is applied without a clear user-configurable timezone setting. The timezone offset is stored but may not be intuitive for users to configure.

**Files and line numbers:**
- `components/cdc_os_ui/src/SettingsHandlers.cpp:127-134` - Timezone offset applied in settings
- `components/cdc_os_ui/src/SleepManager.cpp:158-165` - Timezone used for display
- `components/cdc_os_ui/src/AppUi.cpp:253-265` - Timezone used for lock screen
- `components/cdc_hal/src/Rtc.cpp` - Timezone offset storage and application

## Impact

Users may not understand how to configure the correct timezone. The current implementation uses a simple offset (e.g., +12, -5) rather than timezone names (e.g., "America/New_York", "Europe/Berlin"), which doesn't account for daylight saving time (DST) transitions.

## Evidence

```cpp
// components/cdc_os_ui/src/SettingsHandlers.cpp:122-124
// Value is 0-26 (slider range), convert to -12..+14 (actual timezone)
int8_t tzOffset = static_cast<int8_t>(static_cast<int16_t>(value) - 12);
rtc->setTimezoneOffset(tzOffset);
```

The offset-based approach doesn't handle:
- Daylight saving time transitions
- Historical timezone changes
- Cities in the same offset with different DST rules

## Recommended Fix

1. Add a timezone selection menu with named timezones (e.g., "UTC", "Europe/Berlin", "America/New_York")
2. Use a lightweight timezone database or lookup table for common timezones
3. Implement DST detection based on timezone name and date
4. Alternatively, implement automatic timezone detection via NTP server response
5. Store timezone as a string (e.g., "Europe/Berlin") rather than offset

## References

- [IANA Time Zone Database](https://en.wikipedia.org/wiki/Zone.tab)
- [POSIX TZ Environment Variable](https://pubs.opengroup.org/onlinepubs/9699919799/utilities/TZ.html)
