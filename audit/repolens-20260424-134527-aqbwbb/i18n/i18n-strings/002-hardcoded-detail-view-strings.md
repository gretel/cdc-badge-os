---
title: "[MEDIUM] Hardcoded strings in FIDO2 and GPG detail views"
severity: MEDIUM
domain: i18n/strings
lens: i18n-strings
labels:
  - "audit:i18n/i18n-strings"
---

## Summary
Detail view labels in FIDO2 and GPG modules use hardcoded English strings in `snprintf()` calls. These strings are displayed to users but are not externalized for translation.

**components/mod_fido2/src/Fido2Ui.cpp:202-208**
```cpp
snprintf(detail_text, sizeof(detail_text),
         "Relying Party:\n%s\n\n"
         "Type: %s  Algo: %s\n"
         "User: %s\n"
         "Slot: %d\n"
         "Sign count: %lu\n"
         "Resident: %s\n\n"
         "Fingerprint:\n%s\n\n"
         "%s",
```

**components/mod_gpg/src/GpgModule.cpp:401**
```cpp
snprintf(detail, sizeof(detail),
         "User-ID: %s\nCurve: %s\nFingerprint: %s\nCreated: %lu\nSign Count: %lu",
```

**components/mod_gpg/src/GpgModule.cpp:139-143** (serial command output)
```cpp
cdc::serial::Console::printf("User-ID: %s\r\n", status.user_id);
cdc::serial::Console::printf("Curve: %s\r\n", ...);
cdc::serial::Console::printf("Created: %lu\r\n", ...);
cdc::serial::Console::printf("Sign Count: %lu\r\n", ...);
```

## Impact
- German users see mixed-language detail views (English labels with German menu items)
- Labels like "Relying Party", "Sign count", "User-ID" are not translated
- Serial command output also uses hardcoded English (less critical but inconsistent)

## Evidence
```
components/mod_fido2/src/Fido2Ui.cpp:202-208: Hardcoded detail view labels
components/mod_gpg/src/GpgModule.cpp:401: Hardcoded status detail labels
components/mod_gpg/src/GpgModule.cpp:139-143: Hardcoded serial output labels
```

## Recommended Fix
1. Create a helper function or struct for localized field labels in each module
2. Add string IDs for each label:
   - FIDO2: `STR_Relying_PARTY`, `STR_TYPE`, `STR_ALGO`, `STR_USER`, `STR_SLOT`, `STR_SIGN_COUNT`, `STR_RESIDENT`, `STR_FINGERPRINT`
   - GPG: `STR_USER_ID`, `STR_CREATED`, `STR_SIGN_COUNT`

3. Example refactoring for FIDO2:
   ```cpp
   // Add to registerStrings():
   i18n.registerTranslation(s_strIdBase + STR_Relying_PARTY, ui::Language::EN, "Relying Party");
   i18n.registerTranslation(s_strIdBase + STR_Relying_PARTY, ui::Language::DE, "Relying Party"); // Or German equivalent
   
   // In showDetail():
   snprintf(detail_text, sizeof(detail_text),
            "%s:\n%s\n\n"
            "%s: %s  %s: %s\n"
            ...
            mstr(STR_Relying_PARTY), info.rp_id,
            mstr(STR_TYPE), key_type, ...);
   ```

## References
- `components/mod_fido2/src/Fido2Ui.cpp:56-81` - Current FIDO2 string registration
- `components/mod_gpg/src/GpgModule.cpp:63-104` - Current GPG string registration
- `components/cdc_ui/src/I18n.cpp:201-370` - Core string registration pattern
