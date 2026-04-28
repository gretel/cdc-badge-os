---
title: "[HIGH] Generic error messages without specific context"
severity: HIGH
domain: Error Traceability
lens: error-traceability
labels:
  - "audit:maintainability/error-traceability"
---

## Summary
Multiple modules use generic error messages (e.g., "FAILED", "Error") without specific context about what operation failed or why. This makes debugging and error triage difficult.

## Impact
- **Debugging difficulty**: When users or developers see "FAILED", there's no way to know what specific operation failed
- **Poor user experience**: End users cannot understand what went wrong
- **Support burden**: Troubleshooting requires adding more logging or reproducing the issue
- **Error aggregation**: Errors cannot be easily grouped or classified because they all have the same message

## Evidence

### GPG Module - Multiple locations
`components/mod_gpg/src/GpgModule.cpp`:
- Line ~140: `ui::showToastError(ui::tr(ui::StringId::FAILED));` - Key generation failure
- Line ~170: `ui::showToastError(ui::tr(ui::StringId::FAILED));` - Export failure  
- Line ~180: `ui::showToastError(ui::tr(ui::StringId::FAILED));` - Reset failure

### TOTP Module - Multiple locations
`components/mod_totp/src/TotpModule.cpp`:
- Line ~95: `ui::showToastError(ui::tr(ui::StringId::FAILED));` - Wizard save failure
- Line ~120: `ui::showToastError(ui::tr(ui::StringId::FAILED));` - Account edit failure

### Password Module
`components/mod_password/src/PasswordModule.cpp`:
- Multiple occurrences: `ui::showToastError(ui::tr(ui::StringId::FAILED));`

### NVS Edit Module
`components/mod_nvsedit/src/NvsEditModule.cpp`:
- Line ~67: `showToastError("Delete disabled");`
- Line ~320: `showToastError("Delete failed");`
- Line ~354: `showToastError("Delete failed");`

### VCard Module
`components/mod_vcard/src/VcardModule.cpp`:
- `ui::showToastError("Exchange failed");`

## Recommended Fix

1. **Create specific error messages** for each failure case:
   ```cpp
   // Instead of:
   ui::showToastError(ui::tr(ui::StringId::FAILED));
   
   // Use:
   ui::showToastError("Key generation failed: insufficient slots");
   // or
   ui::showToastError("Export failed: memory full");
   ```

2. **Include error codes or status** where applicable:
   ```cpp
   char error_msg[64];
   snprintf(error_msg, sizeof(error_msg), "Key generation failed (code: %d)", err_code);
   ui::showToastError(error_msg);
   ```

3. **Log the specific error** with context before showing toast:
   ```cpp
   LOG_E(TAG, "Key generation failed: slot_map_error, slot_count=%d", slot_count);
   ui::showToastError("Key generation failed: no available slots");
   ```

4. **Consider error categories** for UI:
   - User errors (invalid input)
   - System errors (memory, storage)
   - Hardware errors (secure element, I2C)

## References
- [FIDO2 CTAP2 Error Codes](components/mod_fido2/include/mod_fido2/ctap2.h) - Good example of specific error classification
- [CTAPHID Error Codes](components/mod_fido2/include/mod_fido2/ctaphid.h) - Protocol-level error taxonomy
