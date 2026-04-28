---
title: "[MEDIUM] Missing German translations for module-specific strings"
severity: MEDIUM
domain: i18n/strings
lens: i18n-strings
labels:
  - "audit:i18n/i18n-strings"
---

## Summary
Several modules register English strings but have incomplete or missing German translations. The following strings are registered in English but either missing German translations or use identical English text:

**components/mod_fido2/src/Fido2Ui.cpp:74**
```cpp
i18n.registerTranslation(s_strIdBase + STR_WEB_AUTHN, ui::Language::DE, "WebAuthn");
```
("WebAuthn" is kept in English - acceptable as technical term, but should be consistent)

**components/mod_gpg/src/GpgModule.cpp:100-104**
```cpp
i18n.registerTranslation(s_strIdBase + STR_CURVE_ED25519, ui::Language::DE, "Ed25519");
i18n.registerTranslation(s_strIdBase + STR_CURVE_P256, ui::Language::DE, "P-256");
i18n.registerTranslation(s_strIdBase + STR_NO_KEY, ui::Language::DE, "Kein Key konfiguriert");
i18n.registerTranslation(s_strIdBase + STR_CONFIRM_RESET, ui::Language::DE, "Alle GPG Keys loeschen?");
i18n.registerTranslation(s_strIdBase + STR_EXPORT_TITLE, ui::Language::DE, "GPG Public Key");
```
("Key" and "Public" remain in English - inconsistent German)

**components/mod_totp/src/TotpModule.cpp:77-90**
Several strings keep English technical terms:
- "Account Name" → "Account Name" (should be "Kontoname")
- "Secret (Base32)" → "Secret (Base32)" (should be "Geheimnis (Base32)")
- "Issuer (optional)" → "Issuer (optional)" (should be "Aussteller (optional)")
- "Digits" → "Digits" (should be "Ziffern")
- "TOTP Code" → "TOTP Code" (should be "TOTP-Code")

## Impact
- Inconsistent German UI with English technical terms mixed in
- Users expecting full German localization get partial translation
- Technical terms like "Key", "Public", "Account" could be translated for better UX

## Evidence
```
components/mod_fido2/src/Fido2Ui.cpp:74: STR_WEB_AUTHN DE = "WebAuthn"
components/mod_gpg/src/GpgModule.cpp:100-104: Mixed German/English terms
components/mod_totp/src/TotpModule.cpp:77-90: Multiple untransliterated strings
```

## Recommended Fix
Review each string and decide if translation is appropriate:

1. **Technical terms**: Some terms like "WebAuthn", "Base32", "TOTP" are standard and may stay in English
2. **UI labels**: Labels like "Account Name", "Digits", "Curve" should be fully translated

Example fixes for mod_totp:
```cpp
// Before:
i18n.registerTranslation(s_strIdBase + STR_ACCOUNT_NAME, ui::Language::DE, "Account Name");

// After:
i18n.registerTranslation(s_strIdBase + STR_ACCOUNT_NAME, ui::Language::DE, "Kontoname");

// Before:
i18n.registerTranslation(s_strIdBase + STR_DIGITS, ui::Language::DE, "Digits");

// After:
i18n.registerTranslation(s_strIdBase + STR_DIGITS, ui::Language::DE, "Ziffern");
```

For mod_gpg, consider:
- "Kein Key konfiguriert" → "Kein Schluessel konfiguriert"
- "GPG Public Key" → "GPG Public-Schluessel" or "GPG-Public-Key"

## References
- `components/cdc_ui/src/I18n.cpp:201-370` - Core strings use proper German translations
- Display font limitation: German umlauts use `ae`, `oe`, `ue` (see CLAUDE.md)
