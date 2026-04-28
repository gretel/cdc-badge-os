---
title: "[MEDIUM] Date Input View - Hardcoded DD / MM / YYYY Separator"
severity: MEDIUM
domain: i18n
lens: locale-formatting
labels:
  - audit:i18n/i18n-formatting
---

## Summary
DateInputView uses hardcoded `DD / MM / YYYY` format with spaces around slashes for display. This format may not match user expectations for different locales.

**Files:**
- `components/cdc_views/src/DateInputView.cpp:232`

**Evidence:**
```cpp
// DateInputView.cpp:232
snprintf(dateStr, sizeof(dateStr), "%02d / %02d / %04d", day_, month_, year_);
```

## Impact
- Date format with spaces around separators is non-standard
- Order of day/month/year assumes European convention (DD.MM.YYYY)
- Users from different locales may input dates in wrong order expecting their local format

## Recommended Fix
1. Change separator to match common conventions (no spaces, use locale-appropriate separator)
2. Add date order preference based on language setting:

```cpp
// In DateInputView.cpp
void DateInputView::render(bool partial) {
    // ...
    char dateStr[20];
    
    // Get current language for default format
    bool isGerman = (ui::I18n::instance().getLanguage() == ui::Language::DE);
    
    if (isGerman) {
        // DD.MM.YYYY for German
        snprintf(dateStr, sizeof(dateStr), "%02d.%02d.%04d", day_, month_, year_);
    } else {
        // MM/DD/YYYY for English (or keep current format)
        snprintf(dateStr, sizeof(dateStr), "%02d/%02d/%04d", month_, day_, year_);
    }
    // ...
}
```

3. Consider adding a date format setting in preferences

## References
- Date format by country (Wikipedia)
- Unicode LDML date formats
