---
title: "[MEDIUM] Additional hardcoded toast strings in vCard module"
severity: MEDIUM
domain: i18n/strings
lens: i18n-strings
labels:
  - "audit:i18n/i18n-strings"
---

## Summary
Additional hardcoded English strings in the vCard module were found (following finding #8). The module has i18n infrastructure but some toast strings are not externalized.

**components/mod_vcard/src/VcardModule.cpp:256,301**
```cpp
ui::showToastInfo("Advertising started");  // Line 256
ui::showToastError("Exchange failed");     // Line 301
```

Combined with finding #8, the complete list of hardcoded strings in vCard module:
- "Waiting for vCard..." (line 141)
- "No vCard configured" (line 229)
- "Scan stopped" (line 242)
- "Advertising stopped" (line 253)
- "Advertising started" (line 256)
- "Exchange failed" (line 301)

## Impact
- German users see English toast notifications for common vCard operations
- Inconsistent with other module strings that use `mstr()`
- All user-facing status messages should be translatable

## Evidence
```
components/mod_vcard/src/VcardModule.cpp:256:    ui::showToastInfo("Advertising started");
components/mod_vcard/src/VcardModule.cpp:301:    ui::showToastError("Exchange failed");
```

## Recommended Fix
1. **Add missing string IDs** in VcardModule.cpp (combining with finding #8):
   ```cpp
   static constexpr uint16_t STR_WAITING_FOR_VCARD = 16;
   static constexpr uint16_t STR_NO_VCARD_CONFIGURED = 17;
   static constexpr uint16_t STR_SCAN_STOPPED = 18;
   static constexpr uint16_t STR_ADVERTISING_STOPPED = 19;
   static constexpr uint16_t STR_ADVERTISING_STARTED = 20;
   static constexpr uint16_t STR_EXCHANGE_FAILED = 21;
   static constexpr uint16_t STR_COUNT = 22;
   ```

2. **Register all translations** in `registerStrings()`:
   ```cpp
   // Add to existing registrations:
   i18n.registerTranslation(s_strIdBase + STR_ADVERTISING_STARTED, ui::Language::EN, "Advertising started");
   i18n.registerTranslation(s_strIdBase + STR_ADVERTISING_STARTED, ui::Language::DE, "Werbung gestartet");

   i18n.registerTranslation(s_strIdBase + STR_EXCHANGE_FAILED, ui::Language::EN, "Exchange failed");
   i18n.registerTranslation(s_strIdBase + STR_EXCHANGE_FAILED, ui::Language::DE, "Austausch fehlgeschlagen");
   ```

3. **Replace all hardcoded strings**:
   ```cpp
   // Before:
   ui::showToastInfo("Advertising started");
   
   // After:
   ui::showToastInfo(mstr(STR_ADVERTISING_STARTED));
   ```

## References
- `components/mod_vcard/src/VcardModule.cpp:25-41` - Current string ID definitions
- `components/mod_vcard/src/VcardModule.cpp:57-115` - String registration function
- `components/mod_vcard/src/VcardModule.cpp:141,229,242,253,256,301` - All hardcoded string locations
