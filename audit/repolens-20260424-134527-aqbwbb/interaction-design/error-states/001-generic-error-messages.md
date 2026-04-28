---
title: "[MEDIUM] Generic 'FAILED' error messages lack context for user recovery"
severity: MEDIUM
domain: interaction-design/error-states
lens: error-state-messaging
labels:
  - "audit:interaction-design/error-states"
---

## Summary
Multiple modules use the generic `ui::tr(ui::StringId::FAILED)` message for diverse failure scenarios, making it impossible for users to understand what went wrong or how to recover.

**Affected locations:**
- `components/mod_totp/src/TotpModule.cpp:770` - Edit wizard read failure
- `components/mod_totp/src/TotpModule.cpp:875` - Wizard validation failure  
- `components/mod_totp/src/TotpModule.cpp:908` - Account save failure
- `components/mod_gpg/src/GpgModule.cpp:492` - Key generation failure
- `components/mod_gpg/src/GpgModule.cpp:507` - Public key export failure
- `components/mod_gpg/src/GpgModule.cpp:518` - Reset confirmation failure
- `components/mod_password/src/PasswordModule.cpp:576` - Entry read failure
- `components/mod_password/src/PasswordModule.cpp:536` - Entry save failure
- `components/mod_password/src/PasswordModule.cpp:693` - Entry deletion failure
- `components/cdc_os_ui/src/BluetoothMenuUi.cpp` - Various BLE operations
- `components/cdc_os_ui/src/ExpertMenuUi.cpp` - Multiple operations

## Impact
**User Experience:** Users see "FAILED" but have no idea:
- What operation failed (generate key? save password? connect WiFi?)
- Why it failed (timeout? invalid data? hardware issue?)
- How to fix it (retry? check input? try again later?)

**Debugging:** When users report issues, developers can't distinguish between different failure modes since they all display the same message.

## Evidence
```cpp
// mod_totp/src/TotpModule.cpp:908
if (ok) {
    ui::showToastSuccess(ui::tr(ui::StringId::OK));
    // ...
} else {
    ui::showToastError(ui::tr(ui::StringId::FAILED));  // What failed? Save? Validate?
}

// mod_gpg/src/GpgModule.cpp:492
if (gpg_generate_key(s_wizard.curve)) {
    ui::showToastSuccess(ui::tr(ui::StringId::OK));
} else {
    ui::showToastError(ui::tr(ui::StringId::FAILED));  // Generation failed? Which curve?
}

// mod_password/src/PasswordModule.cpp:536
if (ok) {
    ui::showToastSuccess(mstr(STR_SAVED));
    // ...
} else {
    ui::showToastError(ui::tr(ui::StringId::FAILED));  // Add? Update? Validation?
}
```

## Recommended Fix
**1. Create specific error messages for each failure scenario:**

Add new i18n strings to each module:
```cpp
// mod_totp strings
STR_SAVE_FAILED = "Save failed"
STR_VALIDATE_FAILED = "Invalid data"
STR_READ_FAILED = "Load failed"

// mod_gpg strings  
STR_GENERATE_FAILED = "Key generation failed"
STR_EXPORT_FAILED = "Export failed"
STR_RESET_FAILED = "Reset failed"

// mod_password strings
STR_ENTRY_SAVE_FAILED = "Save entry failed"
STR_ENTRY_LOAD_FAILED = "Load entry failed"
STR_DELETE_FAILED = "Delete failed"
```

**2. Update error calls with context:**
```cpp
// Before
ui::showToastError(ui::tr(ui::StringId::FAILED));

// After
ui::showToastError(mstr(STR_SAVE_FAILED));  // or
ui::showToastError(mstr(STR_GENERATE_FAILED));
```

**3. For WiFi/BLE operations, include the specific error reason:**
```cpp
// Already implemented in WifiHandlers - propagate to UI
ui::showToastError(wifiHandlers.getLastError().c_str());
```

**Estimated effort:** ~1 hour per module (TOTP, GPG, Password, WiFi, Bluetooth)

## References
- [Nielsen Norman Group: Error Messages](https://www.nngroup.com/articles/error-message-101/)
- [Material Design: Error states](https://material.io/design/fundamentals/errors.html)
