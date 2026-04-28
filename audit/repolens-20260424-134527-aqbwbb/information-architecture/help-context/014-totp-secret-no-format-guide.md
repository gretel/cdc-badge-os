---
title: "[MEDIUM] TOTP secret input lacks format guidance for Base32 encoding"
severity: MEDIUM
domain: information-architecture
lens: help-context
labels:
  - "audit:information-architecture/help-context"
---

## Summary
The TOTP module (`components/mod_totp/src/TotpModule.cpp`) accepts secret keys for TOTP generation but provides no guidance on the expected format (Base32 encoding), common mistakes, or where users can find their secret.

**Evidence** (`components/mod_totp/src/TotpModule.cpp:28-32`):
```cpp
static constexpr uint16_t STR_TOTP = 0;
static constexpr uint16_t STR_ADD_ACCOUNT = 1;
static constexpr uint16_t STR_ACCOUNT_NAME = 2;
static constexpr uint16_t STR_SECRET = 3;
static constexpr uint16_t STR_ISSUER = 4;
static constexpr uint16_t STR_DIGITS = 5;
```

String registration (lines 64-66):
```cpp
i18n.registerTranslation(s_strIdBase + STR_SECRET, ui::Language::EN, "Secret (Base32)");
i18n.registerTranslation(s_strIdBase + STR_ISSUER, ui::Language::DE, "Issuer (optional)");
```

The label says "Secret (Base32)" but provides no:
- Example of valid Base32 format
- Common mistakes to avoid (lowercase, special chars, spaces)
- Where to get the secret (QR code from authenticator app, manual entry)
- Typical length (16-32 chars)

## Impact
Users will likely:
1. Enter secrets in wrong format (hex, base64, lowercase)
2. Include spaces or special characters
3. Not know how to extract the secret from QR codes
4. Get "Invalid input" errors without understanding why

## Evidence
- File: `components/mod_totp/src/TotpModule.cpp`
- Lines: 28-90 (string definitions and registration)
- Line 72: `STR_SECRET` = "Secret (Base32)" - minimal hint
- No format examples or helper text shown during input

## Recommended Fix
Add comprehensive format guidance:

1. **Enhanced label with example**:
   ```
   Secret (Base32, uppercase, no spaces)
   Example: JBSWY3DPEHPK3PXP
   ```

2. **Info action** (key '3'):
   Shows:
   - What Base32 is
   - Valid characters (A-Z, 2-7)
   - How to get secret from QR code
   - Common services' secret formats

3. **Input validation with helpful error**:
   ```
   Invalid Base32: Use A-Z and 2-7 only
   Tip: Copy from authenticator app settings
   ```

## References
- Base32 encoding: https://tools.ietf.org/html/rfc4648
- TOTP setup guides: https://github.com/google/google-authenticator/wiki/Key-Uri-Format
