---
title: "[MEDIUM] Hardcoded wizard option strings in TOTP module"
severity: MEDIUM
domain: i18n/strings
lens: i18n-strings
labels:
  - "audit:i18n/i18n-strings"
---

## Summary
The TOTP module's wizard uses hardcoded English strings for algorithm and period options. These are displayed as list labels but are not externalized.

**components/mod_totp/src/TotpModule.cpp:826-830**
```cpp
static ui::ListItem algoItems[3] = {
    {"SHA1", 0, false, nullptr},
    {"SHA256", 0, false, nullptr},
    {"SHA512", 0, false, nullptr}
};
```

**components/mod_totp/src/TotpModule.cpp:847-850**
```cpp
static ui::ListItem periodItems[2] = {
    {"30s", 0, false, nullptr},
    {"60s", 0, false, nullptr}
};
```

## Impact
- Algorithm names "SHA1", "SHA256", "SHA512" are shown in English
- Period options "30s", "60s" are shown in English
- German users see English technical terms in wizard flow
- While these are standard abbreviations, consistency with other translated UI would be better

## Evidence
```
components/mod_totp/src/TotpModule.cpp:826-830: Hardcoded algorithm names
components/mod_totp/src/TotpModule.cpp:847-850: Hardcoded period options
```

## Recommended Fix
1. Add string IDs for algorithm and period labels:
   ```cpp
   static constexpr uint16_t STR_ALGO_SHA1 = 20;
   static constexpr uint16_t STR_ALGO_SHA256 = 21;
   static constexpr uint16_t STR_ALGO_SHA512 = 22;
   static constexpr uint16_t STR_PERIOD_30S = 23;
   static constexpr uint16_t STR_PERIOD_60S = 24;
   static constexpr uint16_t STR_COUNT = 25;
   ```

2. Register translations in `registerStrings()`:
   ```cpp
   i18n.registerTranslation(s_strIdBase + STR_ALGO_SHA1, ui::Language::EN, "SHA1");
   i18n.registerTranslation(s_strIdBase + STR_ALGO_SHA1, ui::Language::DE, "SHA1"); // Technical term
   
   i18n.registerTranslation(s_strIdBase + STR_PERIOD_30S, ui::Language::EN, "30s");
   i18n.registerTranslation(s_strIdBase + STR_PERIOD_30S, ui::Language::DE, "30s"); // Could use "30 Sek"
   ```

3. Update wizard functions to use `mstr()`:
   ```cpp
   static ui::ListItem algoItems[3] = {
       {mstr(STR_ALGO_SHA1), 0, false, nullptr},
       {mstr(STR_ALGO_SHA256), 0, false, nullptr},
       {mstr(STR_ALGO_SHA512), 0, false, nullptr}
   };
   ```

**Note**: Technical terms like SHA1/SHA256/SHA512 might intentionally stay in English as they are standard abbreviations. The fix ensures consistency and allows future localization if desired.

## References
- `components/mod_totp/src/TotpModule.cpp:24-40` - Current string offset definitions
- `components/mod_totp/src/TotpModule.cpp:63-92` - String registration pattern
