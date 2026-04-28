---
title: "[MEDIUM] Hardcoded toast strings in vCard module"
severity: MEDIUM
domain: i18n/strings
lens: i18n-strings
labels:
  - "audit:i18n/i18n-strings"
---

## Summary
The vCard module uses hardcoded English strings for some toast notifications instead of using the i18n system. The module has proper i18n registration but misses some strings.

**components/mod_vcard/src/VcardModule.cpp:141,229,242,245,253**
```cpp
ui::showToastInfo("Waiting for vCard...");  // Line 141
ui::showToastInfo("No vCard configured");   // Line 229
ui::showToastInfo("Scan stopped");          // Line 242
ui::showToastInfo(mstr(STR_SCANNING));      // Line 245 (correct - using mstr)
ui::showToastInfo("Advertising stopped");   // Line 253
```

## Impact
- German users see English toast notifications for common actions
- Inconsistent with other module strings that use `mstr()`
- Easy to fix but affects user experience

## Evidence
```
components/mod_vcard/src/VcardModule.cpp:141:    ui::showToastInfo("Waiting for vCard...");
components/mod_vcard/src/VcardModule.cpp:229:    ui::showToastInfo("No vCard configured");
components/mod_vcard/src/VcardModule.cpp:242:    ui::showToastInfo("Scan stopped");
components/mod_vcard/src/VcardModule.cpp:253:    ui::showToastInfo("Advertising stopped");
```

## Recommended Fix
1. **Add missing string IDs** in VcardModule.cpp:
   ```cpp
   static constexpr uint16_t STR_WAITING_FOR_VCARD = 16;
   static constexpr uint16_t STR_NO_VCARD_CONFIGURED = 17;
   static constexpr uint16_t STR_SCAN_STOPPED = 18;
   static constexpr uint16_t STR_ADVERTISING_STOPPED = 19;
   static constexpr uint16_t STR_COUNT = 20;
   ```

2. **Register translations** in `registerStrings()`:
   ```cpp
   i18n.registerTranslation(s_strIdBase + STR_WAITING_FOR_VCARD, ui::Language::EN, "Waiting for vCard...");
   i18n.registerTranslation(s_strIdBase + STR_WAITING_FOR_VCARD, ui::Language::DE, "Warte auf vCard...");

   i18n.registerTranslation(s_strIdBase + STR_NO_VCARD_CONFIGURED, ui::Language::EN, "No vCard configured");
   i18n.registerTranslation(s_strIdBase + STR_NO_VCARD_CONFIGURED, ui::Language::DE, "Keine vCard konfiguriert");

   i18n.registerTranslation(s_strIdBase + STR_SCAN_STOPPED, ui::Language::EN, "Scan stopped");
   i18n.registerTranslation(s_strIdBase + STR_SCAN_STOPPED, ui::Language::DE, "Scan gestoppt");

   i18n.registerTranslation(s_strIdBase + STR_ADVERTISING_STOPPED, ui::Language::EN, "Advertising stopped");
   i18n.registerTranslation(s_strIdBase + STR_ADVERTISING_STOPPED, ui::Language::DE, "Werbung gestoppt");
   ```

3. **Replace hardcoded strings**:
   ```cpp
   // Before:
   ui::showToastInfo("Waiting for vCard...");
   
   // After:
   ui::showToastInfo(mstr(STR_WAITING_FOR_VCARD));
   ```

## References
- `components/mod_vcard/src/VcardModule.cpp:25-41` - Current string ID definitions
- `components/mod_vcard/src/VcardModule.cpp:57-115` - String registration function
- `components/mod_vcard/src/VcardModule.cpp:141,229,242,253` - Locations of hardcoded strings
