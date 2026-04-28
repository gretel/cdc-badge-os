---
title: "[MEDIUM] TOTP Code Display - Hardcoded 6/7/8 Digit Formatting"
severity: MEDIUM
domain: i18n
lens: locale-formatting
labels:
  - audit:i18n/i18n-formatting
---

## Summary
TOTP code display uses hardcoded digit formatting with `snprintf` and specific format strings for 6, 7, or 8 digits. No grouping/separation for readability (e.g., `123 456` vs `123456`).

**Files:**
- `components/mod_totp/src/TotpStore.cpp:496-502`

**Evidence:**
```cpp
// TotpStore.cpp:496-502
if (account.digits == 8) {
    snprintf(codeOut, 9, "%08lu", static_cast<unsigned long>(code));
} else if (account.digits == 7) {
    snprintf(codeOut, 8, "%07lu", static_cast<unsigned long>(code));
} else {
    snprintf(codeOut, 7, "%06lu", static_cast<unsigned long>(code));
}
```

## Impact
- Long TOTP codes (7-8 digits) are harder to read and type without grouping
- Some locales conventionally group numbers for readability (e.g., `123 456` vs `123.456`)
- No option for visual grouping that varies by locale preference

## Recommended Fix
1. Add optional digit grouping for TOTP codes
2. Use locale-appropriate grouping separator (space, dot, or none)

```cpp
// Enhanced TOTP code formatting with optional grouping
void formatTotpCode(uint32_t code, uint8_t digits, char* out, size_t outMax, bool group = true) {
    if (digits == 6 && group) {
        // Group as 3+3 with space (locale-neutral)
        snprintf(out, outMax, "%03lu %03lu", code / 1000, code % 1000);
    } else if (digits == 8 && group) {
        // Group as 4+4 with space
        snprintf(out, outMax, "%04lu %04lu", code / 10000, code % 10000);
    } else {
        // No grouping
        char fmt[8];
        snprintf(fmt, sizeof(fmt), "%%0%dlu", digits);
        snprintf(out, outMax, fmt, static_cast<unsigned long>(code));
    }
}
```

3. Make grouping configurable in TOTP settings

## References
- Human-readable number formatting conventions
- TOTP usability best practices
