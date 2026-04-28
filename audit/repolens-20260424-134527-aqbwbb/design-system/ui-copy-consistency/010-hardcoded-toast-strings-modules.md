---
title: "[MEDIUM] Hardcoded toast strings in modules (not using i18n)"
severity: MEDIUM
domain: design-system/ui-copy-consistency
lens: ui-copy-consistency
labels:
  - "microcopy"
  - "i18n"
  - "hardcoded-strings"
---

## Summary
Multiple modules use hardcoded English toast strings instead of leveraging the i18n system:

1. **mod_password and mod_totp** - "Typed" success message (lines 450, 467):
   ```cpp
   ui::showToastSuccess("Typed");
   ```

2. **mod_nvsedit** - Status messages (lines 67, 310, 320, 516):
   ```cpp
   showToastError("Delete disabled");
   showToastInfo("Deleted");
   showToastError("Delete failed");
   showToastInfo("Read-only mode");
   ```

3. **mod_vcard** - Multiple status messages (lines 141, 229, 242, 253, 256, 301):
   ```cpp
   ui::showToastInfo("Waiting for vCard...");
   ui::showToastInfo("No vCard configured");
   ui::showToastInfo("Scan stopped");
   ui::showToastInfo("Advertising stopped");
   ui::showToastInfo("Advertising started");
   ui::showToastError("Exchange failed");
   ```

4. **mod_hid** - Status messages (lines 244, 247, 249, 272):
   ```cpp
   ui::showToastInfo("Advertising stopped");
   ui::showToastSuccess("Advertising started");
   ui::showToastError("Failed to start");
   ui::showToastInfo("Disconnected");
   ```

5. **mod_ble_serial** - Status messages (lines 257, 262, 278):
   ```cpp
   ui::showToastError("BLE n/a");
   ui::showToastError("BLE disabled");
   ui::showToastError("Init failed");
   ```

6. **WifiMenuUi** - Validation messages (lines 576, 591, 606):
   ```cpp
   showToastError("Invalid IP", TOAST_DURATION_MEDIUM_MS);
   ```

7. **AppUi** - Error message (line 552):
   ```cpp
   showToastAlertSticky(msg ? msg : "Slot map invalid");
   ```

8. **ExpertMenuUi** - Success message (line 47):
   ```cpp
   showToastSuccess("OK", TOAST_DURATION_SHORT_MS);
   ```

## Impact
- **Language switching incomplete**: When users change language, these strings remain in English
- **Inconsistent experience**: Some UI elements switch language, others don't
- **Maintainability**: String updates require searching code instead of central i18n file
- **Translation workflow**: Translators can't easily find all strings needing translation
- **Module-specific strings like "Typed" have no German equivalent**

## Evidence
**File: `components/mod_password/src/PasswordModule.cpp` line 450**
```cpp
ui::showToastSuccess("Typed");
```

**File: `components/mod_totp/src/TotpModule.cpp` line 467**
```cpp
ui::showToastSuccess("Typed");
```

**File: `components/mod_nvsedit/src/NvsEditModule.cpp` lines 67, 310, 320, 516**
```cpp
showToastError("Delete disabled");
showToastInfo("Deleted");
showToastError("Delete failed");
showToastInfo("Read-only mode");
```

**File: `components/mod_vcard/src/VcardModule.cpp` lines 141, 229, 242, 253, 256, 301**
```cpp
ui::showToastInfo("Waiting for vCard...");
ui::showToastInfo("No vCard configured");
ui::showToastInfo("Scan stopped");
ui::showToastInfo("Advertising stopped");
ui::showToastInfo("Advertising started");
ui::showToastError("Exchange failed");
```

**File: `components/mod_hid/src/HidModule.cpp` lines 244, 247, 249, 272**
```cpp
ui::showToastInfo("Advertising stopped");
ui::showToastSuccess("Advertising started");
ui::showToastError("Failed to start");
ui::showToastInfo("Disconnected");
```

**File: `components/mod_ble_serial/src/BleSerialModule.cpp` lines 257, 262, 278**
```cpp
ui::showToastError("BLE n/a");
ui::showToastError("BLE disabled");
ui::showToastError("Init failed");
```

**File: `components/cdc_os_ui/src/WifiMenuUi.cpp` lines 576, 591, 606**
```cpp
showToastError("Invalid IP", TOAST_DURATION_MEDIUM_MS);
```

**File: `components/cdc_os_ui/src/AppUi.cpp` line 552**
```cpp
showToastAlertSticky(msg ? msg : "Slot map invalid");
```

**File: `components/cdc_os_ui/src/ExpertMenuUi.cpp` line 47**
```cpp
showToastSuccess("OK", TOAST_DURATION_SHORT_MS);
```

## Recommended Fix
1. **Add missing strings to core I18n** (for shared messages):
   ```cpp
   // In I18n.h - add to StringId enum
   TYPED,                  // "Typed" / "Eingegeben"
   DELETE_DISABLED,        // "Delete disabled" / "Loeschen deaktiviert"
   DELETE_FAILED,          // "Delete failed" / "Loeschen fehlgeschlagen"
   READ_ONLY_MODE,         // "Read-only mode" / "Nur-Lesen-Modus"
   WAITING_FOR_VCARD,      // "Waiting for vCard..." / "Warte auf vCard..."
   NO_VCARD_CONFIGURED,    // "No vCard configured" / "Keine vCard konfiguriert"
   SCAN_STOPPED,           // "Scan stopped" / "Scan gestoppt"
   ADVERTISING_STOPPED,    // "Advertising stopped" / "Werbung gestoppt"
   ADVERTISING_STARTED,    // "Advertising started" / "Werbung gestartet"
   EXCHANGE_FAILED,        // "Exchange failed" / "Austausch fehlgeschlagen"
   FAILED_TO_START,        // "Failed to start" / "Start fehlgeschlagen"
   DISCONNECTED,           // "Disconnected" / "Getrennt"
   BLE_NA,                 // "BLE n/a" / "BLE n/v"
   BLE_DISABLED,           // "BLE disabled" / "BLE deaktiviert"
   INIT_FAILED,            // "Init failed" / "Init fehlgeschlagen"
   INVALID_IP,             // "Invalid IP" / "Ungueltige IP"
   SLOT_MAP_INVALID,       // "Slot map invalid" / "Slot-Map ungueiltg"
   ```

2. **Add German translations to I18n.cpp**:
   ```cpp
   REG(TYPED,              "Typed",             "Eingegeben");
   REG(DELETE_DISABLED,    "Delete disabled",   "Loeschen deaktiviert");
   REG(DELETE_FAILED,      "Delete failed",     "Loeschen fehlgeschlagen");
   REG(READ_ONLY_MODE,     "Read-only mode",    "Nur-Lesen-Modus");
   REG(WAITING_FOR_VCARD,  "Waiting for vCard...", "Warte auf vCard...");
   REG(NO_VCARD_CONFIGURED,"No vCard configured","Keine vCard konfiguriert");
   REG(SCAN_STOPPED,       "Scan stopped",      "Scan gestoppt");
   REG(ADVERTISING_STOPPED,"Advertising stopped","Werbung gestoppt");
   REG(ADVERTISING_STARTED,"Advertising started","Werbung gestartet");
   REG(EXCHANGE_FAILED,    "Exchange failed",   "Austausch fehlgeschlagen");
   REG(FAILED_TO_START,    "Failed to start",   "Start fehlgeschlagen");
   REG(DISCONNECTED,       "Disconnected",      "Getrennt");
   REG(BLE_NA,             "BLE n/a",           "BLE n/v");
   REG(BLE_DISABLED,       "BLE disabled",      "BLE deaktiviert");
   REG(INIT_FAILED,        "Init failed",       "Init fehlgeschlagen");
   REG(INVALID_IP,         "Invalid IP",        "Ungueltige IP");
   REG(SLOT_MAP_INVALID,   "Slot map invalid",  "Slot-Map ungueiltg");
   ```

3. **Update each module to use i18n**:
   ```cpp
   // Example: mod_password/src/PasswordModule.cpp
   ui::showToastSuccess(ui::tr(ui::StringId::TYPED));
   
   // Example: components/cdc_os_ui/src/WifiMenuUi.cpp
   showToastError(ui::tr(ui::StringId::INVALID_IP), TOAST_DURATION_MEDIUM_MS);
   ```

4. **Consolidate duplicate strings**:
   - "Advertising started" / "Advertising stopped" used in both mod_vcard and mod_hid
   - "Disabled" / "Enabled" patterns should be consistent
   - "Failed" variations should use common pattern

## References
- Hardcoded Strings vs Centralized Copy guidelines
- Internationalization best practices
- UI Copy Consistency: Toast message patterns
