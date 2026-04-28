---
title: "[MEDIUM] Timezone String Formatting - Hardcoded UTC±N Format"
severity: MEDIUM
domain: i18n
lens: locale-formatting
labels:
  - audit:i18n/i18n-formatting
---

## Summary
Timezone offset is formatted using hardcoded `UTC±N` format in `Rtc.cpp`. The format uses a sign character directly after `UTC` without standard formatting conventions.

**Files:**
- `components/cdc_hal/src/Rtc.cpp:275-277`

**Evidence:**
```cpp
// Rtc.cpp:275-277
if (tzOffset_ == 0) {
    snprintf(tz, sizeof(tz), "UTC0");
} else {
    snprintf(tz, sizeof(tz), "UTC%+d", -tzOffset_);
}
```

## Impact
- Non-standard timezone format: `UTC+5` should typically be `UTC+05` (two digits)
- Some locales may expect different timezone notation (e.g., `GMT+5`, `CET`, etc.)
- The `%+d` format may produce inconsistent alignment for single vs double digit offsets

## Recommended Fix
1. Use consistent two-digit formatting for timezone offsets
2. Consider locale-appropriate timezone prefixes:

```cpp
// Better timezone formatting
void Esp32Rtc::applyTimezone() {
    char tz[16];
    int8_t absOffset = tzOffset_ < 0 ? -tzOffset_ : tzOffset_;
    char sign = tzOffset_ >= 0 ? '+' : '-';
    
    // Standard UTC±HH format with two digits
    snprintf(tz, sizeof(tz), "UTC%c%02d", sign, absOffset);
    
    setenv("TZ", tz, 1);
    tzset();
}
```

3. For display purposes, consider using IANA timezone names where possible

## References
- RFC 8536 (Time zone database)
- Unicode CLDR timezone formats
