---
title: "[MEDIUM] Time Separator Inconsistency - Colon vs Period in TOTP Display"
severity: MEDIUM
domain: i18n
lens: locale-formatting
labels:
  - audit:i18n/i18n-formatting
---

## Summary
The TOTP countdown display uses hardcoded `"%us"` format with colon-style formatting, while time input view uses spaces around colon separator. Inconsistent formatting approach across the codebase.

**Files:**
- `components/mod_totp/src/TotpModule.cpp:454` (display format)
- `components/cdc_views/src/TimeInputView.cpp:191` (time display)

**Evidence:**
```cpp
// TotpModule.cpp:454 - Countdown display
display->printf("%us", static_cast<unsigned>(remaining_));

// TimeInputView.cpp:191 - Time display with spaces
snprintf(timeStr, sizeof(timeStr), "%02d : %02d", hour_, minute_);
```

## Impact
- Inconsistent visual formatting within the same application
- The TOTP countdown uses `%us` which is locale-neutral but not internationally standard
- TimeInputView uses spaces around the colon which differs from standard time notation

## Recommended Fix
1. Standardize time display format across all views
2. Use the locale-aware time formatting function (see issue #002)
3. For countdown/remaining time, consider locale-aware duration formatting:

```cpp
// Standardize to consistent format without spaces
snprintf(timeStr, sizeof(timeStr), "%02d:%02d", hour_, minute_);

// Or use locale-aware formatting
formatTimeLocaleAware(&timeinfo, timeStr, sizeof(timeStr));

// For countdown, use localized "seconds remaining" label
const char* secLabel = tr(StringId::SECONDS);  // "s" in EN, "s" in DE
display->printf("%u%s", static_cast<unsigned>(remaining_), secLabel);
```

## References
- ISO 8601 duration format
- Unicode Locale Data (time separator conventions)
