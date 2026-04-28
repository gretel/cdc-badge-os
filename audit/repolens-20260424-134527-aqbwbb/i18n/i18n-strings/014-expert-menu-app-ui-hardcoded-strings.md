---
title: "[LOW] Hardcoded toast strings in Expert Menu and App UI"
severity: LOW
domain: i18n/strings
lens: i18n-strings
labels:
  - "audit:i18n/i18n-strings"
---

## Summary
Core UI components (Expert Menu and App UI) use hardcoded English strings for toast notifications. These are less critical as they are less frequently used.

**components/cdc_os_ui/src/ExpertMenuUi.cpp:47**
```cpp
showToastSuccess("OK", TOAST_DURATION_SHORT_MS);
```

**components/cdc_os_ui/src/AppUi.cpp:552**
```cpp
showToastAlertSticky(msg ? msg : "Slot map invalid");
```

## Impact
- German users see English toast notifications
- Expert menu is used less frequently, lower priority
- "Slot map invalid" is a technical error message

## Evidence
```
components/cdc_os_ui/src/ExpertMenuUi.cpp:47:    showToastSuccess("OK", TOAST_DURATION_SHORT_MS);
components/cdc_os_ui/src/AppUi.cpp:552:        showToastAlertSticky(msg ? msg : "Slot map invalid");
```

## Recommended Fix
1. **For "OK" string**: Consider adding to core strings if used frequently:
   ```cpp
   // In I18n.h (if not already there):
   OK,  // Already exists as StringId::OK!
   ```
   
   Note: `StringId::OK` already exists! Should use `tr(StringId::OK)` instead of "OK".

2. **For "Slot map invalid"**: Add new core string:
   ```cpp
   // In I18n.h (Settings or Hardware section):
   SLOT_MAP_INVALID,
   
   // In I18n.cpp:
   REG(SLOT_MAP_INVALID, "Slot map invalid", "Slot-Map unguelig");
   
   // In AppUi.cpp:
   showToastAlertSticky(msg ? msg : tr(StringId::SLOT_MAP_INVALID));
   ```

**Priority**: LOW - These are infrequent error/status messages. The "OK" fix is trivial since `StringId::OK` already exists.

## References
- `components/cdc_ui/include/cdc_ui/I18n.h:34` - StringId::OK already exists
- `components/cdc_os_ui/src/ExpertMenuUi.cpp:47` - Location of "OK" string
- `components/cdc_os_ui/src/AppUi.cpp:552` - Location of "Slot map invalid" string
