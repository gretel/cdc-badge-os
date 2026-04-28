---
title: "[MEDIUM] Hardcoded toast notification strings in multiple modules"
severity: MEDIUM
domain: i18n/strings
lens: i18n-strings
labels:
  - "audit:i18n/i18n-strings"
---

## Summary
Multiple modules use hardcoded English strings directly in `showToastSuccess()` and `showToastError()` calls instead of using the i18n system (`mstr()` or `ui::tr()`). Affected files:

1. **components/mod_totp/src/TotpModule.cpp:467**
   ```cpp
   ui::showToastSuccess("Typed");
   ```

2. **components/mod_hid/src/HidModule.cpp:247-249**
   ```cpp
   ui::showToastSuccess("Advertising started");
   ui::showToastError("Failed to start");
   ```

3. **components/mod_nvsedit/src/NvsEditModule.cpp:67,320,354**
   ```cpp
   showToastError("Delete disabled");
   showToastError("Delete failed");
   ```

## Impact
- German-speaking users see English-only toast notifications
- Inconsistent user experience across the application
- Easy to fix but strings are completely untranslatable in current state

## Evidence
```
components/mod_totp/src/TotpModule.cpp:467:    ui::showToastSuccess("Typed");
components/mod_hid/src/HidModule.cpp:247:    ui::showToastSuccess("Advertising started");
components/mod_hid/src/HidModule.cpp:249:    ui::showToastError("Failed to start");
components/mod_nvsedit/src/NvsEditModule.cpp:67:    showToastError("Delete disabled");
components/mod_nvsedit/src/NvsEditModule.cpp:320:    showToastError("Delete failed");
components/mod_nvsedit/src/NvsEditModule.cpp:354:    showToastError("Delete failed");
```

## Recommended Fix
1. Add string IDs to each module's string registration:
   - `mod_totp`: Add `STR_TYPED` (e.g., "Typed" / "Eingegeben")
   - `mod_hid`: Add `STR_ADVERTISING_STARTED` and `STR_FAILED_TO_START`
   - `mod_nvsedit`: Add `STR_DELETE_DISABLED` and `STR_DELETE_FAILED`

2. Replace hardcoded strings with `mstr()` calls:
   ```cpp
   // Before:
   ui::showToastSuccess("Typed");
   
   // After:
   ui::showToastSuccess(mstr(STR_TYPED));
   ```

3. Register German translations alongside English in each module's `registerStrings()` function.

## References
- `components/cdc_ui/include/cdc_ui/I18n.h` - I18n API documentation
- `components/mod_totp/src/TotpModule.cpp:64-90` - Example of proper string registration
- `components/cdc_views/include/cdc_views/ToastView.h` - Toast API
