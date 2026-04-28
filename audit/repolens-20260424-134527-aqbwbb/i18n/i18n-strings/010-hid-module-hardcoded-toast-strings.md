---
title: "[MEDIUM] Hardcoded toast strings in HID module"
severity: MEDIUM
domain: i18n/strings
lens: i18n-strings
labels:
  - "audit:i18n/i18n-strings"
---

## Summary
The HID module uses hardcoded English strings for toast notifications. The module has i18n infrastructure but some toast strings are not externalized.

**components/mod_hid/src/HidModule.cpp:244,247,249,272**
```cpp
ui::showToastInfo("Advertising stopped");   // Line 244
ui::showToastSuccess("Advertising started"); // Line 247
ui::showToastError("Failed to start");      // Line 249
ui::showToastInfo("Disconnected");          // Line 272
```

## Impact
- German users see English toast notifications for HID status changes
- Inconsistent with other module strings that use `mstr()`
- Common status messages should be translatable

## Evidence
```
components/mod_hid/src/HidModule.cpp:244:    ui::showToastInfo("Advertising stopped");
components/mod_hid/src/HidModule.cpp:247:        ui::showToastSuccess("Advertising started");
components/mod_hid/src/HidModule.cpp:249:        ui::showToastError("Failed to start");
components/mod_hid/src/HidModule.cpp:272:    ui::showToastInfo("Disconnected");
```

## Recommended Fix
1. **Add missing string IDs** in HidModule.cpp:
   ```cpp
   static constexpr uint16_t STR_ADVERTISING_STOPPED = 8;  // Adjust based on current STR_COUNT
   static constexpr uint16_t STR_ADVERTISING_STARTED = 9;
   static constexpr uint16_t STR_FAILED_TO_START = 10;
   static constexpr uint16_t STR_DISCONNECTED = 11;
   static constexpr uint16_t STR_COUNT = 12;
   ```

2. **Register translations** in `registerStrings()`:
   ```cpp
   i18n.registerTranslation(s_strIdBase + STR_ADVERTISING_STOPPED, ui::Language::EN, "Advertising stopped");
   i18n.registerTranslation(s_strIdBase + STR_ADVERTISING_STOPPED, ui::Language::DE, "Werbung gestoppt");

   i18n.registerTranslation(s_strIdBase + STR_ADVERTISING_STARTED, ui::Language::EN, "Advertising started");
   i18n.registerTranslation(s_strIdBase + STR_ADVERTISING_STARTED, ui::Language::DE, "Werbung gestartet");

   i18n.registerTranslation(s_strIdBase + STR_FAILED_TO_START, ui::Language::EN, "Failed to start");
   i18n.registerTranslation(s_strIdBase + STR_FAILED_TO_START, ui::Language::DE, "Start fehlgeschlagen");

   i18n.registerTranslation(s_strIdBase + STR_DISCONNECTED, ui::Language::EN, "Disconnected");
   i18n.registerTranslation(s_strIdBase + STR_DISCONNECTED, ui::Language::DE, "Getrennt");
   ```

3. **Replace hardcoded strings**:
   ```cpp
   // Before:
   ui::showToastInfo("Advertising stopped");
   
   // After:
   ui::showToastInfo(mstr(STR_ADVERTISING_STOPPED));
   ```

## References
- `components/mod_hid/src/HidModule.cpp:27-43` - Current string ID definitions
- `components/mod_hid/src/HidModule.cpp:47-100` - String registration function
- `components/mod_hid/src/HidModule.cpp:244,247,249,272` - Locations of hardcoded strings
