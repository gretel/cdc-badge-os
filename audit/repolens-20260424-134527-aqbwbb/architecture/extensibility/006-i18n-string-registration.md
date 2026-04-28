---
title: "[MEDIUM] I18n String Registration Requires Manual Base ID Tracking"
severity: MEDIUM
domain: extensibility
lens: internationalization
labels:
  - "audit:architecture/extensibility"
---

## Summary
Module i18n strings are registered using a manual base ID system where each module must track `s_strIdBase` and calculate offsets. Adding new strings requires updating count constants and potentially renumbering offsets.

**Evidence:**
- `components/mod_totp/src/TotpModule.cpp:27-40`:
  ```cpp
  static uint16_t s_strIdBase = 0;
  static constexpr uint16_t STR_TOTP = 0;
  static constexpr uint16_t STR_ADD_ACCOUNT = 1;
  static constexpr uint16_t STR_ACCOUNT_NAME = 2;
  static constexpr uint16_t STR_SECRET = 3;
  // ... 14 strings
  static constexpr uint16_t STR_COUNT = 14;
  ```

- `components/mod_totp/src/TotpModule.cpp:53-92`:
  ```cpp
  static void registerStrings() {
      auto& i18n = ui::I18n::instance();
      s_strIdBase = i18n.registerModule("mod_totp", STR_COUNT);
      if (s_strIdBase == 0) {
          LOG_E(TAG, "Failed to register i18n strings");
          return;
      }
      
      i18n.registerTranslation(s_strIdBase + STR_TOTP, ui::Language::EN, "TOTP");
      i18n.registerTranslation(s_strIdBase + STR_ADD_ACCOUNT, ui::Language::EN, "Add Account");
      // ... 28 registrations (14 per language)
  }
  ```

- `components/grove_led/src/GroveLedModule.cpp:24-91`: Similar pattern repeated

## Impact
**Fragile String Management:**
1. Adding a new string in the middle requires updating all subsequent offsets
2. `STR_COUNT` must be manually kept in sync
3. Each module duplicates this pattern (boilerplate)
4. No compile-time validation of string counts

## Evidence
Files affected:
- `components/mod_totp/src/TotpModule.cpp:27-92` (TOTP strings)
- `components/grove_led/src/GroveLedModule.cpp:24-91` (Grove LED strings)
- `components/mod_gpg/src/GpgModule.cpp` (GPG strings)
- `components/cdc_ui/src/I18n.cpp:74-90` (I18n switch for language names)

I18n registration in `components/cdc_ui/src/I18n.cpp`:
```cpp
void I18n::registerTranslation(uint16_t id, Language lang, const char* text) {
    for (int i = 0; i < MAX_STRINGS; i++) {
        if (strings_[i].id == id && strings_[i].lang == lang) {
            strncpy(strings_[i].text, text, MAX_TEXT_LEN);
            return;
        }
    }
}
```

## Recommended Fix
Implement data-driven string registration:

1. **Use string table arrays:**
   ```cpp
   static constexpr struct {
       const char* en;
       const char* de;
   } s_strings[] = {
       {"TOTP", "TOTP"},
       {"Add Account", "Account hinzufuegen"},
       {"Account Name", "Account Name"},
       // ...
   };
   
   static void registerStrings() {
       auto& i18n = ui::I18n::instance();
       s_strIdBase = i18n.registerModule("mod_totp", COUNT_OF(s_strings));
       for (int i = 0; i < COUNT_OF(s_strings); i++) {
           i18n.registerTranslation(s_strIdBase + i, Language::EN, s_strings[i].en);
           i18n.registerTranslation(s_strIdBase + i, Language::DE, s_strings[i].de);
       }
   }
   ```

2. **Or use macro for auto-counting:**
   ```cpp
   #define STR(x) x
   #define STRS(...) {__VA_ARGS__}
   #define COUNT(x) (sizeof(x)/sizeof(x[0]))
   
   static constexpr char* s_en[] = STRS(
       STR("TOTP"),
       STR("Add Account"),
       STR("Account Name")
   );
   ```

3. **Add compile-time validation:**
   ```cpp
   static_assert(COUNT_OF(s_strings) == STR_COUNT, "String count mismatch");
   ```

## References
- String table localization
- X-Macro patterns
- Compile-time assertions
