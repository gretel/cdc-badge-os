---
title: "[LOW] Password TOTP Slot Display - Integer Formatting Without Grouping"
severity: LOW
domain: i18n
lens: locale-formatting
labels:
  - audit:i18n/i18n-formatting
---

## Summary
Password module displays TOTP slot numbers using simple integer formatting without any consideration for locale-specific number formatting conventions.

**Files:**
- `components/mod_password/src/PasswordModule.cpp:479`
- `components/mod_password/src/PasswordModule.cpp:628`

**Evidence:**
```cpp
// PasswordModule.cpp:479
snprintf(totpBuf, sizeof(totpBuf), "%u", entry.totpSlot);

// PasswordModule.cpp:628
snprintf(totpBuf, sizeof(totpBuf), "%u", s_wizard.entry.totpSlot);
```

## Impact
- For large slot numbers (>999), no thousand separators are used
- Different locales use different thousand separators (comma, period, space)
- Minor impact since TOTP slots are typically small numbers (0-99)

## Recommended Fix
1. Add a helper function for locale-aware integer formatting
2. Use it consistently across the codebase:

```cpp
// Helper function for locale-aware integer formatting
void formatIntegerLocale(uint32_t value, char* out, size_t outMax) {
    // For small numbers, no grouping needed
    if (value < 1000) {
        snprintf(out, outMax, "%lu", static_cast<unsigned long>(value));
        return;
    }
    
    // Get locale preference for thousand separator
    bool isGerman = (ui::I18n::instance().getLanguage() == ui::Language::DE);
    char sep = isGerman ? '.' : ',';
    
    // Format with grouping
    char temp[32];
    snprintf(temp, sizeof(temp), "%lu", static_cast<unsigned long>(value));
    // Add grouping logic here...
    snprintf(out, outMax, "%s", temp); // Simplified
}
```

3. For TOTP slots specifically, the impact is minimal since slot numbers are small

## References
- Number formatting conventions by locale
- Unicode CLDR number formats
