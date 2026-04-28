---
title: "[LOW] Decimal separator hardcoded to period (.) instead of locale-aware format"
severity: LOW
domain: i18n
lens: i18n-formatting
labels:
  - audit:i18n/i18n-formatting
---

## Summary

The codebase uses a **period (`.`) as the decimal separator** for floating-point numbers, which is correct for English and many other locales, but incorrect for many European locales (including German) where a **comma (`,`) is the standard decimal separator**.

### Files Affected

| File | Lines | Number Format |
|------|-------|---------------|
| `components/cdc_os_ui/src/HardwareInfo.cpp` | 137 | `%.1f C` (temperature) |

### Evidence

**HardwareInfo.cpp:137** - Period as decimal separator:
```cpp
append("%s: %.1f C\n", tr(StringId::HW_TEMP), tempC);
```

This will display `Temp: 25.5 C` for English users, but German users expect `Temp: 25,5 C`.

## Impact

1. **German Users**: Since the app supports German language (`Language::DE`), German users will see the decimal separator in English format, which may feel inconsistent with the rest of the localized UI.

2. **Locale Inconsistency**: The app already provides German translations for UI text, but numeric formatting remains in English convention.

3. **Low Priority**: The temperature display is primarily for hardware debugging, and the numeric value is still understandable regardless of the separator.

## Recommended Fix

### Option 1: Add Locale-Aware Decimal Separator

Create a helper function to format floating-point numbers with the correct decimal separator:

```cpp
/**
 * \brief Format float with locale-aware decimal separator.
 * \param value Float value to format.
 * \param precision Decimal precision.
 * \param buf Output buffer.
 * \param bufLen Size of output buffer.
 * \return void
 */
static void formatFloatLocaleAware(float value, uint8_t precision, char* buf, size_t bufLen) {
    // Get current language
    Language lang = getActiveLanguage();
    
    // Format with period first
    snprintf(buf, bufLen, "%.*f", precision, value);
    
    // Replace period with comma for German
    if (lang == Language::DE) {
        for (char* p = buf; *p; p++) {
            if (*p == '.') *p = ',';
        }
    }
}
```

Then update the call site:
```cpp
// HardwareInfo.cpp:137
char tempStr[16];
formatFloatLocaleAware(tempC, 1, tempStr, sizeof(tempStr));
append("%s: %s C\n", tr(StringId::HW_TEMP), tempStr);
```

### Option 2: Use `localeconv()` (If Available)

The C library provides locale-specific decimal point character:

```cpp
#include <locale.h>

// Initialize locale once at startup
setlocale(LC_ALL, "");

// Use localeconv to get decimal point
struct lconv* lc = localeconv();
char decimalPoint = lc->decimal_point[0];  // ',' for German, '.' for English

// Format with correct separator
char buf[16];
snprintf(buf, sizeof(buf), "%.*f", 1, tempC);
// Replace decimal point
for (char* p = buf; *p; p++) {
    if (*p == '.') *p = decimalPoint;
}
```

**Note**: ESP32's C library may have limited locale support, so test this approach.

### Option 3: Simple String Replacement

For a lightweight solution that doesn't require locale support:

```cpp
static void formatFloatGerman(float value, char* buf, size_t bufLen) {
    snprintf(buf, bufLen, "%.1f", value);
    // Replace period with comma for German display
    for (char* p = buf; *p; p++) {
        if (*p == '.') *p = ',';
    }
}
```

## References

- [Decimal Separator by Country](https://en.wikipedia.org/wiki/Decimal_separator#Countries_where_the_comma_is_used_as_decimal_separator)
- [C `localeconv()` function](https://en.cppreference.com/w/c/numeric/locale/localeconv)
- Most of Europe uses comma as decimal separator; US, UK, and many Asian countries use period
