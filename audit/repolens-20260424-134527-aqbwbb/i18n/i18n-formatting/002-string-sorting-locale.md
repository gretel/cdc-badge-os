---
title: "[MEDIUM] String sorting uses ASCII comparison, not locale-aware collation"
severity: MEDIUM
domain: i18n
lens: i18n-formatting
labels:
  - audit:i18n/i18n-formatting
---

## Summary

The codebase sorts strings alphabetically using **ASCII-based comparison** (`strcasecmp`, `std::tolower`) instead of **locale-aware collation**. This causes incorrect sorting order for:
- **Accented characters** (e.g., `é`, `ñ`, `ü`)
- **German umlauts** (ä, ö, ü should sort as ae, oe, ue or after regular letters)
- **Case sensitivity** (ASCII comparison may not match user expectations)

### Files Affected

| File | Lines | Sorting Method |
|------|-------|----------------|
| `components/mod_fido2/src/Fido2Ui.cpp` | 112-117, 154-156 | `strcasecmp` via `strcasecmp_safe()` |
| `components/mod_password/src/PasswordStore.cpp` | 328-339, 376-378 | `std::tolower` in `compareTitles()` |
| `components/mod_vcard/src/vcard_store.cpp` | 691 | `strcasecmp` in bubble sort |

### Evidence

**Fido2Ui.cpp:154** - ASCII case-insensitive sort:
```cpp
std::sort(s_sortMap, s_sortMap + count, [](uint8_t a, uint8_t b) {
    return strcasecmp_safe(s_labels[a], s_labels[b]) < 0;
});
```

**PasswordStore.cpp:328-338** - Manual ASCII lowercase comparison:
```cpp
int PasswordStore::compareTitles(const char* a, const char* b) {
    if (!a) return b ? -1 : 0;
    if (!b) return 1;
    while (*a || *b) {
        int ca = *a ? std::tolower(static_cast<unsigned char>(*a)) : 0;
        int cb = *b ? std::tolower(static_cast<unsigned char>(*b)) : 0;
        if (ca != cb) return ca - cb;
        if (*a) ++a;
        if (*b) ++b;
    }
    return 0;
}
```

**vcard_store.cpp:691** - ASCII sort for last names:
```cpp
if (strcasecmp(g_cards[a].last_name, g_cards[b].last_name) > 0) {
```

## Impact

1. **Incorrect Sorting for German Users**: The app supports German language, but German umlauts (ä, ö, ü) will not sort correctly:
   - Expected: `Müller` sorts after `Mueller` or near `Mu...`
   - Actual: `Müller` sorts by ASCII value, potentially in wrong position

2. **Accent Sensitivity**: Names with accents (e.g., `José`, `François`, `Señor`) will not sort alphabetically as users expect

3. **Inconsistent User Experience**: Users from different locales expect different sorting rules:
   - **German**: Umlauts often treated as base + e (ä = ae)
   - **Swedish**: Å, Ä, Ö are separate letters at the end of alphabet
   - **Spanish**: Ñ is a separate letter after N
   - **French**: Accents affect sort order (e.g., `é` after `e`)

4. **Module-Specific Inconsistency**: Each module implements its own sorting, making it hard to provide a consistent locale-aware solution

## Recommended Fix

### Option 1: Add Locale-Aware String Comparison Helper

Create a centralized locale-aware comparison function that respects the current language setting:

```cpp
/**
 * \brief Locale-aware string comparison for sorting.
 * \param a First string to compare.
 * \param b Second string to compare.
 * \return Negative if a < b, 0 if equal, positive if a > b.
 */
static int localeAwareCompare(const char* a, const char* b) {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;

    // Get current language from I18n
    Language lang = getActiveLanguage();

    // German-specific handling for umlauts
    if (lang == Language::DE) {
        // Convert umlauts to their base+e equivalents for sorting
        // ä -> ae, ö -> oe, ü -> ue, Ä -> Ae, Ö -> Oe, Ü -> Ue
        char aNorm[256], bNorm[256];
        normalizeGermanUmlauts(a, aNorm, sizeof(aNorm));
        normalizeGermanUmlauts(b, bNorm, sizeof(bNorm));
        return strcasecmp(aNorm, bNorm);
    }

    // Default: case-insensitive ASCII comparison
    return strcasecmp(a, b);
}

/**
 * \brief Normalize German umlauts for sorting.
 * \param src Source string.
 * \param dst Destination buffer.
 * \param dstLen Size of destination buffer.
 * \return void
 */
static void normalizeGermanUmlauts(const char* src, char* dst, size_t dstLen) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dstLen - 2; i++) {
        switch (src[i]) {
            case 'ä': if (j < dstLen - 2) { dst[j++] = 'a'; dst[j++] = 'e'; } break;
            case 'ö': if (j < dstLen - 2) { dst[j++] = 'o'; dst[j++] = 'e'; } break;
            case 'ü': if (j < dstLen - 2) { dst[j++] = 'u'; dst[j++] = 'e'; } break;
            case 'Ä': if (j < dstLen - 2) { dst[j++] = 'A'; dst[j++] = 'e'; } break;
            case 'Ö': if (j < dstLen - 2) { dst[j++] = 'O'; dst[j++] = 'e'; } break;
            case 'Ü': if (j < dstLen - 2) { dst[j++] = 'U'; dst[j++] = 'e'; } break;
            default: dst[j++] = src[i]; break;
        }
    }
    dst[j] = '\0';
}
```

### Option 2: Use Standard `strcoll()` (If Available)

The C library provides `strcoll()` for locale-aware comparison:

```cpp
#include <locale.h>

// Set locale based on language
if (lang == Language::DE) {
    setlocale(LC_COLLATE, "de_DE.UTF-8");
} else {
    setlocale(LC_COLLATE, "en_US.UTF-8");
}

// Use strcoll for comparison
int result = strcoll(a, b);
```

**Note**: ESP32 may have limited locale support, so this depends on the C library implementation.

### Option 3: Simple Unicode-Friendly Comparison

For a lightweight solution that handles common cases:

```cpp
static int unicodeAwareCompare(const char* a, const char* b) {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;

    while (*a && *b) {
        // Basic ASCII: case-insensitive
        if (*a < 128 && *b < 128) {
            int ca = tolower(*a);
            int cb = tolower(*b);
            if (ca != cb) return ca - cb;
            a++; b++;
        }
        // Extended ASCII/UTF-8: compare bytes directly
        else {
            if (*a != *b) return *a - *b;
            a++; b++;
        }
    }
    return *a - *b;
}
```

### Update All Call Sites

Replace the current comparison functions with the new locale-aware version:

1. **Fido2Ui.cpp:154**: Change `strcasecmp_safe()` to `localeAwareCompare()`
2. **PasswordStore.cpp:328**: Change `compareTitles()` to use locale-aware logic
3. **vcard_store.cpp:691**: Change `strcasecmp()` to `localeAwareCompare()`

## References

- [Unicode Collation Algorithm](https://unicode.org/reports/tr10/)
- [German Umlaut Sorting Rules](https://en.wikipedia.org/wiki/A%C3%BC%C3%B6)
- [`strcoll()` man page](https://man7.org/linux/man-pages/man3/strcoll.3.html)
- [JavaScript Intl.Collator](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Intl/Collator) (for reference on locale-aware sorting)
