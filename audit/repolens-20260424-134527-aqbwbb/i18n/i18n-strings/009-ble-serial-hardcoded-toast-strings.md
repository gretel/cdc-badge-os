---
title: "[MEDIUM] Hardcoded toast strings in BLE Serial module"
severity: MEDIUM
domain: i18n/strings
lens: i18n-strings
labels:
  - "audit:i18n/i18n-strings"
---

## Summary
The BLE Serial module uses hardcoded English strings for some toast notifications. The module has i18n infrastructure but some strings are not externalized.

**components/mod_ble_serial/src/BleSerialModule.cpp:257,262,278**
```cpp
ui::showToastError("BLE n/a");      // Line 257
ui::showToastError("BLE disabled"); // Line 262
ui::showToastError("Init failed");  // Line 278
```

## Impact
- German users see English toast notifications for BLE status
- Inconsistent with other module strings that use `mstr()`
- Common status messages should be translatable

## Evidence
```
components/mod_ble_serial/src/BleSerialModule.cpp:257:    ui::showToastError("BLE n/a");
components/mod_ble_serial/src/BleSerialModule.cpp:262:    ui::showToastError("BLE disabled");
components/mod_ble_serial/src/BleSerialModule.cpp:278:        ui::showToastError("Init failed");
```

## Recommended Fix
1. **Add missing string IDs** in BleSerialModule.cpp:
   ```cpp
   static constexpr uint16_t STR_BLE_NA = 3;  // Adjust based on current STR_COUNT
   static constexpr uint16_t STR_BLE_DISABLED = 4;
   static constexpr uint16_t STR_INIT_FAILED = 5;
   static constexpr uint16_t STR_COUNT = 6;
   ```

2. **Register translations** in `registerStrings()`:
   ```cpp
   i18n.registerTranslation(s_strIdBase + STR_BLE_NA, ui::Language::EN, "BLE n/a");
   i18n.registerTranslation(s_strIdBase + STR_BLE_NA, ui::Language::DE, "BLE n/v");  // n/v = nicht verfügbar

   i18n.registerTranslation(s_strIdBase + STR_BLE_DISABLED, ui::Language::EN, "BLE disabled");
   i18n.registerTranslation(s_strIdBase + STR_BLE_DISABLED, ui::Language::DE, "BLE deaktiviert");

   i18n.registerTranslation(s_strIdBase + STR_INIT_FAILED, ui::Language::EN, "Init failed");
   i18n.registerTranslation(s_strIdBase + STR_INIT_FAILED, ui::Language::DE, "Init fehlgeschlagen");
   ```

3. **Replace hardcoded strings**:
   ```cpp
   // Before:
   ui::showToastError("BLE n/a");
   
   // After:
   ui::showToastError(mstr(STR_BLE_NA));
   ```

## References
- `components/mod_ble_serial/src/BleSerialModule.cpp:14-22` - Current string ID definitions
- `components/mod_ble_serial/src/BleSerialModule.cpp:35-55` - String registration function
- `components/mod_ble_serial/src/BleSerialModule.cpp:257,262,278` - Locations of hardcoded strings
