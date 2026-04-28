---
title: "[LOW] Missing German translations for module strings"
severity: LOW
domain: design-system/ui-copy-consistency
lens: ui-copy-consistency
labels:
  - "microcopy"
  - "i18n"
  - "translations"
---

## Summary
Several module-specific i18n strings have incomplete German translations, with English text used as the German fallback:

1. **mod_password** - "TOTP Slot (optional)" not translated (line 100):
   ```cpp
   i18n.registerTranslation(s_strIdBase + STR_TOTP_SLOT, ui::Language::DE, "TOTP Slot (optional)");
   ```

2. **mod_totp** - Multiple strings not translated (lines 81-82):
   ```cpp
   i18n.registerTranslation(s_strIdBase + STR_ACCOUNT_NAME, ui::Language::DE, "Account Name");
   i18n.registerTranslation(s_strIdBase + STR_SECRET, ui::Language::DE, "Secret (Base32)");
   i18n.registerTranslation(s_strIdBase + STR_ISSUER, ui::Language::DE, "Issuer (optional)");
   i18n.registerTranslation(s_strIdBase + STR_DIGITS, ui::Language::DE, "Digits");
   ```

3. **mod_hid** - "ASCII only" not translated (line 70):
   ```cpp
   i18n.registerTranslation(s_strIdBase + STR_ASCII_ONLY, ui::Language::DE, "Nur ASCII");
   // This one is translated, but check others
   ```

## Impact
- **Incomplete localization**: German users see English text for some strings
- **Inconsistent experience**: Some UI elements in German, others in English
- **Reduced usability**: Technical terms in English may confuse non-technical German users
- **Professionalism**: Incomplete translations make the application feel unfinished

## Evidence
**File: `components/mod_password/src/PasswordModule.cpp` line 100**
```cpp
i18n.registerTranslation(s_strIdBase + STR_TOTP_SLOT, ui::Language::DE, "TOTP Slot (optional)");
```
Should be: "TOTP Slot (optional)" → "TOTP Slot (optional)" or "TOTP-Slot (optional)"

**File: `components/mod_totp/src/TotpModule.cpp` lines 78-85**
```cpp
i18n.registerTranslation(s_strIdBase + STR_ACCOUNT_NAME, ui::Language::DE, "Account Name");
i18n.registerTranslation(s_strIdBase + STR_SECRET, ui::Language::DE, "Secret (Base32)");
i18n.registerTranslation(s_strIdBase + STR_ISSUER, ui::Language::DE, "Issuer (optional)");
i18n.registerTranslation(s_strIdBase + STR_DIGITS, ui::Language::DE, "Digits");
i18n.registerTranslation(s_strIdBase + STR_ALGORITHM, ui::Language::DE, "Algorithmus");  // Translated
i18n.registerTranslation(s_strIdBase + STR_PERIOD, ui::Language::DE, "Periode");        // Translated
```

**File: `components/mod_password/src/PasswordModule.cpp` lines 96-105**
```cpp
i18n.registerTranslation(s_strIdBase + STR_TITLE, ui::Language::DE, "Titel");
i18n.registerTranslation(s_strIdBase + STR_USERNAME, ui::Language::DE, "Benutzername");
i18n.registerTranslation(s_strIdBase + STR_PASSWORD, ui::Language::DE, "Passwort");
i18n.registerTranslation(s_strIdBase + STR_URL, ui::Language::DE, "URL");
i18n.registerTranslation(s_strIdBase + STR_TOTP_SLOT, ui::Language::DE, "TOTP Slot (optional)");  // Not translated
i18n.registerTranslation(s_strIdBase + STR_NOTES, ui::Language::DE, "Notizen");
i18n.registerTranslation(s_strIdBase + STR_VIEW, ui::Language::DE, "Ansehen");
i18n.registerTranslation(s_strIdBase + STR_EDIT, ui::Language::DE, "Bearbeiten");
i18n.registerTranslation(s_strIdBase + STR_DELETE, ui::Language::DE, "Loeschen");
i18n.registerTranslation(s_strIdBase + STR_ACTIONS, ui::Language::DE, "Aktionen");
```

## Recommended Fix
1. **Add German translations for mod_password:**
   ```cpp
   // In mod_password/src/PasswordModule.cpp
   i18n.registerTranslation(s_strIdBase + STR_TOTP_SLOT, ui::Language::DE, "TOTP-Slot (optional)");
   ```

2. **Add German translations for mod_totp:**
   ```cpp
   // In mod_totp/src/TotpModule.cpp
   i18n.registerTranslation(s_strIdBase + STR_ACCOUNT_NAME, ui::Language::DE, "Kontoname");
   i18n.registerTranslation(s_strIdBase + STR_SECRET, ui::Language::DE, "Geheimschlüssel (Base32)");
   i18n.registerTranslation(s_strIdBase + STR_ISSUER, ui::Language::DE, "Aussteller (optional)");
   i18n.registerTranslation(s_strIdBase + STR_DIGITS, ui::Language::DE, "Ziffern");
   ```

3. **Audit all modules for similar issues:**
   - Check mod_ble_serial strings
   - Check mod_vcard strings
   - Check mod_nvsedit strings (if any)

4. **Document translation expectations:**
   - All module strings should have both English and German translations
   - Technical terms can remain in English if no common German equivalent exists
   - Document which terms are acceptable to keep in English

## References
- Internationalization best practices
- UI Copy Consistency: Complete translations
- German language localization guidelines
