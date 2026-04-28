---
title: "[HIGH] Hardcoded English error messages in modules lack internationalization"
severity: HIGH
domain: Error Handling / Error Messages
lens: Error Message Quality
labels:
  - "audit:error-handling/error-messages"
---

## Summary
Multiple modules and UI components use hardcoded English strings in toast notifications instead of using the i18n string registration system. This affects both error messages and info/success messages.

### Affected Files and Lines

**mod_nvsedit** (`components/mod_nvsedit/src/NvsEditModule.cpp`):
- Line 67: `showToastError("Delete disabled")`
- Line 310: `showToastInfo("Deleted")`
- Line 320: `showToastError("Delete failed")`
- Line 337: `showToastInfo("Deleted")`
- Line 354: `showToastError("Delete failed")`

**mod_ble_serial** (`components/mod_ble_serial/src/BleSerialModule.cpp`):
- Line 257: `showToastError("BLE n/a")`
- Line 262: `showToastError("BLE disabled")`
- Line 278: `showToastError("Init failed")`

**mod_hid** (`components/mod_hid/src/HidModule.cpp`):
- Line 249: `showToastError("Failed to start")`
- Line 272: `showToastInfo("Disconnected")`

**mod_vcard** (`components/mod_vcard/src/VcardModule.cpp`):
- Line 301: `showToastError("Exchange failed")` (note: `STR_EXCHANGE_FAIL` is registered but not used here)

**mod_totp** (`components/mod_totp/src/TotpModule.cpp`):
- Line 467: `showToastSuccess("Typed")`

**mod_password** (`components/mod_password/src/PasswordModule.cpp`):
- Line 450: `showToastSuccess("Typed")`

**cdc_os_ui** (`components/cdc_os_ui/src/`):
- `WifiMenuUi.cpp:576,591,606`: `showToastError("Invalid IP", ...)` (3 occurrences)
- `ExpertMenuUi.cpp:47`: `showToastSuccess("OK", ...)`
- `AppUi.cpp:552`: `showToastAlertSticky(msg ? msg : "Slot map invalid")`

## Impact
- **User Experience**: German-speaking users see inconsistent language (English errors in German UI)
- **Accessibility**: Non-English speakers cannot understand error messages
- **Code Consistency**: Violates the project's established i18n pattern where all modules register their strings dynamically

## Evidence
The modules already have i18n infrastructure in place:

**mod_nvsedit** has no string registration at all:
```cpp
// components/mod_nvsedit/src/NvsEditModule.cpp:67
static void showDeleteDisabled() {
    showToastError("Delete disabled");  // Hardcoded English
}
```

**mod_ble_serial** has strings registered but uses hardcoded errors:
```cpp
// components/mod_ble_serial/src/BleSerialModule.cpp:26
static constexpr uint16_t STR_BLE_SERIAL = 0;
static constexpr uint16_t STR_ENABLED = 1;
static constexpr uint16_t STR_DISABLED = 2;
static constexpr uint16_t STR_COUNT = 3;

// But lines 257, 262, 278 use hardcoded strings:
ui::showToastError("BLE n/a");
ui::showToastError("BLE disabled");
ui::showToastError("Init failed");
```

## Recommended Fix
Add missing i18n string definitions and registrations to each module:

### mod_nvsedit
1. Add string enum with entries for: "Delete disabled", "Delete failed", "Deleted"
2. Register strings in `registerStrings()` with English and German translations
3. Replace hardcoded strings with `mstr(STR_*)` calls

### mod_ble_serial
1. Add new string enum entries: `STR_BLE_NA`, `STR_INIT_FAILED`
2. Register translations: "BLE n/a" / "BLE n/v", "Init failed" / "Init fehlgeschlagen"
3. Replace lines 257, 262, 278 with `mstr()` calls

### mod_hid
1. Add string enum entries: `STR_FAILED_TO_START`, `STR_DISCONNECTED`
2. Register translations: "Failed to start" / "Start fehlgeschlagen", "Disconnected" / "Getrennt"
3. Replace lines 249, 272 with `mstr()` calls

### mod_vcard
1. Replace line 301 with `mstr(STR_EXCHANGE_FAIL)` (already registered at line 77)

### mod_totp
1. Add string entry: `STR_TYPED`
2. Register translation: "Typed" / "Getippt"
3. Replace line 467 with `mstr()` call

### mod_password
1. Add string entry: `STR_TYPED`
2. Register translation: "Typed" / "Getippt"
3. Replace line 450 with `mstr()` call

### cdc_os_ui (WifiMenuUi, ExpertMenuUi, AppUi)
1. Add string entries to core `StringId` enum: `INVALID_IP`, `SLOT_MAP_INVALID`
2. Register translations in `I18n.cpp`: "Invalid IP" / "Ungültige IP", "Slot map invalid" / "Slot-Map ungültig"
3. Replace hardcoded strings with `tr(StringId::*)` calls

Reference existing patterns in `components/mod_fido2/src/Fido2Ui.cpp` and `components/mod_totp/src/TotpModule.cpp` for how to properly use internationalized error strings.

## References
- `components/cdc_ui/include/cdc_ui/I18n.h` - Core i18n API
- `components/mod_ble_serial/src/BleSerialModule.cpp:34-50` - Example of string registration
- `components/mod_vcard/src/VcardModule.cpp:77` - STR_EXCHANGE_FAIL already registered but unused
- `components/cdc_ui/src/I18n.cpp:232` - Core string registration pattern
- CDC Badge OS documentation: "Modules register strings dynamically via `I18n::registerModule()`"
