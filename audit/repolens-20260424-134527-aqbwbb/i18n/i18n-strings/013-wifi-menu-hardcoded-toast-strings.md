---
title: "[MEDIUM] Hardcoded toast strings in WiFi menu UI"
severity: MEDIUM
domain: i18n/strings
lens: i18n-strings
labels:
  - "audit:i18n/i18n-strings"
---

## Summary
The WiFi menu UI (core component, not a module) uses hardcoded English strings for toast notifications.

**components/cdc_os_ui/src/WifiMenuUi.cpp:576,591,606**
```cpp
showToastError("Invalid IP", TOAST_DURATION_MEDIUM_MS);  // Three occurrences
```

## Impact
- German users see English toast notification "Invalid IP"
- Core UI component should follow i18n conventions
- The string "Invalid IP" is displayed during WiFi setup, a common user flow

## Evidence
```
components/cdc_os_ui/src/WifiMenuUi.cpp:576:    showToastError("Invalid IP", TOAST_DURATION_MEDIUM_MS);
components/cdc_os_ui/src/WifiMenuUi.cpp:591:    showToastError("Invalid IP", TOAST_DURATION_MEDIUM_MS);
components/cdc_os_ui/src/WifiMenuUi.cpp:606:    showToastError("Invalid IP", TOAST_DURATION_MEDIUM_MS);
```

## Recommended Fix
1. **Add string ID to core StringId enum** in `components/cdc_ui/include/cdc_ui/I18n.h`:
   ```cpp
   // In the Settings or Hardware section:
   WIFI_INVALID_IP,
   ```

2. **Add German translation** in `components/cdc_ui/src/I18n.cpp`:
   ```cpp
   REG(WIFI_INVALID_IP, "Invalid IP", "Ungueltige IP");
   ```

3. **Replace hardcoded strings** in WifiMenuUi.cpp:
   ```cpp
   // Before:
   showToastError("Invalid IP", TOAST_DURATION_MEDIUM_MS);
   
   // After:
   showToastError(tr(StringId::WIFI_INVALID_IP), TOAST_DURATION_MEDIUM_MS);
   ```

**Alternative**: If this is considered a module-specific string, add it to a WiFi module's i18n registration instead of core strings.

## References
- `components/cdc_ui/include/cdc_ui/I18n.h:21-186` - Core StringId enum
- `components/cdc_ui/src/I18n.cpp:201-370` - Core string translations
- `components/cdc_os_ui/src/WifiMenuUi.cpp:576,591,606` - Locations of hardcoded strings
