---
title: "[LOW] TOTP code formatting uses hardcoded decimal format"
severity: LOW
domain: i18n
lens: locale-aware-formatting
labels:
  - "audit:i18n/i18n-formatting"
---

## Summary

The TOTP (Time-based One-Time Password) codes are formatted using a hardcoded decimal format with zero-padding. While TOTP codes are typically displayed as-is, some locales might prefer different visual groupings (e.g., `123 456` vs `123456`).

**Files and line numbers:**
- `components/mod_totp/src/TotpStore.cpp:498-502` - TOTP code formatting

## Impact

Minor visual preference differences. TOTP codes are typically entered as a single block, but some users might prefer visual grouping for readability.

## Evidence

```cpp
// components/mod_totp/src/TotpStore.cpp:498-502
snprintf(codeOut, 9, "%08lu", static_cast<unsigned long>(code));  // 8 digits
snprintf(codeOut, 8, "%07lu", static_cast<unsigned long>(code));  // 7 digits
snprintf(codeOut, 7, "%06lu", static_cast<unsigned long>(code));  // 6 digits
```

## Recommended Fix

1. Consider adding an option to display TOTP codes with visual grouping (e.g., `123 456` for 6-digit codes)
2. This is a low-priority enhancement since TOTP codes are functional regardless of visual formatting

## References

- [RFC 6238 TOTP](https://datatracker.ietf.org/doc/html/rfc6238)
