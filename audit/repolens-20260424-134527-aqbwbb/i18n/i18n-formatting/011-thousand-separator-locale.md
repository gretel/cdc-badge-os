---
title: "[LOW] Large numbers displayed without locale-aware thousand separators"
severity: LOW
domain: i18n
lens: i18n-formatting
labels:
  - audit:i18n/i18n-formatting
---

## Summary

Large numbers are displayed **without thousand separators**, making them harder to read. Different locales use different thousand separators:
- **English (US/UK)**: comma (`,`) - e.g., `1,234,567`
- **German/European**: period (`.`) - e.g., `1.234.567`
- **Some locales**: space (` `) - e.g., `1 234 567`

### Files Affected

| File | Lines | Number Display |
|------|-------|----------------|
| `components/cdc_os_ui/src/HardwareInfo.cpp` | 100, 108, 115, 143 | Memory sizes, uptime |

### Evidence

**HardwareInfo.cpp:100** - No thousand separator:
```cpp
append("%s: %lu/%lu KB\n", tr(StringId::HW_HEAP), freeHeap, totalHeap);
```

This will display `Heap: 123456/200000 KB` which is harder to read than `Heap: 123,456/200,000 KB` (English) or `Heap: 123.456/200.000 KB` (German).

**HardwareInfo.cpp:143** - Uptime without separator:
```cpp
append("%s: %llu s\n", tr(StringId::HW_UPTIME), (unsigned long long)uptimeS);
```

## Impact

1. **Readability**: Large numbers are harder to read quickly without thousand separators.

2. **Locale Consistency**: The app supports German language, but number formatting remains in plain English style.

3. **Low Priority**: The numbers are still understandable, just less readable.

## Recommended Fix

### Option 1: Add Locale-Aware Thousand Separator

Create a helper function to format integers with locale-aware thousand separators:

```cpp
/**
 * \brief Format integer with locale-aware thousand separators.
 * \param value Integer value to format.
 * \param buf Output buffer.
 * \param bufLen Size of output buffer.
 * \return void
 */
static void formatIntLocaleAware(unsigned long value, char* buf, size_t bufLen) {
    Language lang = getActiveLanguage();
    char thousandSep = (lang == Language::DE) ? '.' : ',';
    
    // Convert to string first
    char temp[32];
    snprintf(temp, sizeof(temp), "%lu", value);
    
    // Calculate output length with separators
    size_t len = strlen(temp);
    size_t sepCount = (len > 3) ? (len - 1) / 3 : 0;
    size_t totalLen = len + sepCount + 1;
    
    if (totalLen > bufLen) {
        snprintf(buf, bufLen, "%lu", value);  // Fallback
        return;
    }
    
    // Build result string with separators
    char* out = buf + totalLen - 1;
    *out = '\0';
    int pos = 0;
    
    for (int i = len - 1; i >= 0; i--) {
        if (pos > 0 && pos % 3 == 0) {
            out--;
            *out = thousandSep;
        }
        out--;
        *out = temp[i];
        pos++;
    }
    
    // Adjust buffer to point to start of string
    memmove(buf, out, totalLen);
}
```

### Option 2: Simple String Insertion

For a lightweight solution:

```cpp
static void formatWithThousandSep(unsigned long value, char* buf, size_t bufLen, char sep) {
    char temp[32];
    snprintf(temp, sizeof(temp), "%lu", value);
    
    size_t len = strlen(temp);
    size_t j = 0;
    int count = 0;
    
    for (int i = len - 1; i >= 0; i--) {
        if (count > 0 && count % 3 == 0) {
            temp[len + j + 1] = temp[len + j];
            temp[len + j] = sep;
            j++;
        }
        count++;
    }
    
    snprintf(buf, bufLen, "%s", temp);
}

// Usage:
char heapStr[32];
formatWithThousandSep(freeHeap, heapStr, sizeof(heapStr), ',');  // English
append("%s: %s KB\n", tr(StringId::HW_HEAP), heapStr);
```

### Option 3: Use `strtof()` with `localeconv()` (If Available)

```cpp
#include <locale.h>
#include <locale.h>

// Initialize at startup
setlocale(LC_ALL, "");

// Format with locale-aware separators
char buf[32];
snprintf(buf, sizeof(buf), "%'lu", value);  // Note the ' flag (GNU extension)
```

**Note**: This requires locale support in the C library and may not work on ESP32.

## References

- [Thousand Separator by Country](https://en.wikipedia.org/wiki/Decimal_separator#Thousands_separator)
- [GNU `printf()` thousand separator](https://www.gnu.org/software/libc/manual/html_node/Converting-to-Numbers.html)
- Most European countries use period as thousand separator; US/UK use comma
