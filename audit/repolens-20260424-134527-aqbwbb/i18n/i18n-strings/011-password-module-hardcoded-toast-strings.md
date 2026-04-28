---
title: "[MEDIUM] Hardcoded toast strings in Password module"
severity: MEDIUM
domain: i18n/strings
lens: i18n-strings
labels:
  - "audit:i18n/i18n-strings"
---

## Summary
The Password module uses hardcoded English strings for toast notifications. The module has i18n infrastructure but some strings are not externalized.

**components/mod_password/src/PasswordModule.cpp:450**
```cpp
ui::showToastSuccess("Typed");
```

Note: The TOTP module has the same "Typed" string at line 467. Both should use a shared string or each register their own.

## Impact
- German users see English toast notification "Typed"
- Inconsistent with other module strings that use `mstr()`
- The word "Typed" indicates successful entry confirmation

## Evidence
```
components/mod_password/src/PasswordModule.cpp:450:    ui::showToastSuccess("Typed");
components/mod_totp/src/TotpModule.cpp:467:    ui::showToastSuccess("Typed");  // Same string
```

## Recommended Fix
1. **Add missing string ID** in PasswordModule.cpp:
   ```cpp
   static constexpr uint16_t STR_TYPED = 21;  // Adjust based on current STR_COUNT
   static constexpr uint16_t STR_COUNT = 22;
   ```

2. **Register translations** in `registerStrings()`:
   ```cpp
   i18n.registerTranslation(s_strIdBase + STR_TYPED, ui::Language::EN, "Typed");
   i18n.registerTranslation(s_strIdBase + STR_TYPED, ui::Language::DE, "Eingegeben");
   ```

3. **Replace hardcoded string**:
   ```cpp
   // Before:
   ui::showToastSuccess("Typed");
   
   // After:
   ui::showToastSuccess(mstr(STR_TYPED));
   ```

**Note**: Consider if "Typed" should be a shared core string (`StringId::TYPED`) since both TOTP and Password modules use it. This would avoid duplication.

## References
- `components/mod_password/src/PasswordModule.cpp:27-50` - Current string ID definitions
- `components/mod_password/src/PasswordModule.cpp:64-117` - String registration function
- `components/mod_password/src/PasswordModule.cpp:450` - Location of hardcoded string
- `components/mod_totp/src/TotpModule.cpp:467` - Same string in TOTP module
