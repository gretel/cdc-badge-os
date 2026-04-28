---
title: "[MEDIUM] NVS Edit module lacks i18n string registration"
severity: MEDIUM
domain: i18n/strings
lens: i18n-strings
labels:
  - "audit:i18n/i18n-strings"
---

## Summary
The `mod_nvsedit` module uses hardcoded English strings for toast notifications and likely for UI labels. The module appears to have no i18n string registration system in place.

**components/mod_nvsedit/src/NvsEditModule.cpp:67,320,354**
```cpp
showToastError("Delete disabled");
showToastError("Delete failed");
```

Additionally, the module likely displays namespace names, key types, and other UI elements that should be translatable.

## Impact
- Complete lack of German translation support in NVS Edit module
- All user-facing strings are English-only
- Module is not following the project's i18n pattern used by other modules

## Evidence
```
components/mod_nvsedit/src/NvsEditModule.cpp:67:    showToastError("Delete disabled");
components/mod_nvsedit/src/NvsEditModule.cpp:320:    showToastError("Delete failed");
components/mod_nvsedit/src/NvsEditModule.cpp:354:    showToastError("Delete failed");
```

Also check for:
- `nvsTypeToString()` function (line 75-88) returns English type names like "u8", "str", "blob"
- Namespace and key names displayed in list views

## Recommended Fix
1. **Add string ID constants** at the top of the module:
   ```cpp
   static uint16_t s_strIdBase = 0;
   static constexpr uint16_t STR_DELETE_DISABLED = 0;
   static constexpr uint16_t STR_DELETE_FAILED = 1;
   static constexpr uint16_t STR_COUNT = 2;
   ```

2. **Add `registerStrings()` function**:
   ```cpp
   static void registerStrings() {
       auto& i18n = ui::I18n::instance();
       s_strIdBase = i18n.registerModule("mod_nvsedit", STR_COUNT);
       if (s_strIdBase == 0) {
           LOG_E(TAG, "Failed to register i18n strings");
           return;
       }

       i18n.registerTranslation(s_strIdBase + STR_DELETE_DISABLED, ui::Language::EN, "Delete disabled");
       i18n.registerTranslation(s_strIdBase + STR_DELETE_FAILED, ui::Language::EN, "Delete failed");

       i18n.registerTranslation(s_strIdBase + STR_DELETE_DISABLED, ui::Language::DE, "Loeschen deaktiviert");
       i18n.registerTranslation(s_strIdBase + STR_DELETE_FAILED, ui::Language::DE, "Loeschen fehlgeschlagen");
   }
   ```

3. **Call `registerStrings()` in module initialization** (similar to other modules)

4. **Replace hardcoded strings**:
   ```cpp
   // Before:
   showToastError("Delete disabled");
   
   // After:
   showToastError(mstr(STR_DELETE_DISABLED));
   ```

5. **Consider translating NVS type names** in `nvsTypeToString()`:
   - Create a helper function that uses i18n
   - Or keep as-is since "u8", "str", "blob" are technical terms

## References
- `components/mod_totp/src/TotpModule.cpp:52-92` - Complete example of module string registration
- `components/mod_nvsedit/src/NvsEditModule.cpp:26-88` - Current module structure
- `components/cdc_ui/include/cdc_ui/I18n.h:116-124` - `registerModule()` API
