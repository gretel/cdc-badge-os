---
title: "[MEDIUM] Generic \"Failed\" error messages lack context for users"
severity: MEDIUM
domain: Error Handling / Error Messages
lens: Error Message Quality
labels:
  - "audit:error-handling/error-messages"
---

## Summary
Multiple modules use the generic "Failed" string (`ui::tr(ui::StringId::FAILED)`) for error toasts without providing specific context about what operation failed. This makes it difficult for users to understand and troubleshoot issues.

### Affected Files and Lines

**mod_password** (`components/mod_password/src/PasswordModule.cpp`):
- Line 464: `showToastError(tr(FAILED))` - readEntry failed (what entry?)
- Line 536: `showToastError(tr(FAILED))` - password save failed (which slot?)
- Line 580: `showToastError(tr(FAILED))` - password update failed
- Line 692: `showToastError(tr(FAILED))` - password delete failed

**mod_gpg** (`components/mod_gpg/src/GpgModule.cpp`):
- Line 492: `showToastError(tr(FAILED))` - GPG operation failed
- Line 506: `showToastError(tr(FAILED))` - GPG operation failed
- Line 522: `showToastError(tr(FAILED))` - GPG operation failed

**mod_totp** (`components/mod_totp/src/TotpModule.cpp`):
- Line 762: `showToastError(tr(FAILED))` - TOTP operation failed
- Line 912: `showToastError(tr(FAILED))` - TOTP operation failed

**mod_fido2** (`components/mod_fido2/src/Fido2Ui.cpp`):
- Line 308: `showToastError(tr(FAILED), 2000)` - FIDO2 operation failed

## Impact
- **User Confusion**: "Failed" alone doesn't tell the user what went wrong
- **Troubleshooting Difficulty**: Users cannot understand which operation failed to retry or fix
- **Poor UX**: Generic messages feel unprofessional compared to specific messages like "PIN too short" or "Invalid input"

## Evidence
Example from PasswordModule:

```cpp
// components/mod_password/src/PasswordModule.cpp:461-466
static void showDetails(uint16_t slot) {
    PasswordEntry entry = {};
    if (!PasswordStore::instance().readEntry(slot, &entry)) {
        ui::showToastError(ui::tr(ui::StringId::FAILED));  // Generic!
        return;
    }
    // ...
}
```

Better alternative (already used elsewhere in same file):
```cpp
// components/mod_password/src/PasswordModule.cpp:410
ui::showToastError(mstr(STR_SLOT_ERROR));  // More specific
```

## Recommended Fix
Replace generic "Failed" messages with context-specific messages:

### mod_password
1. Add string entries: `STR_READ_FAILED`, `STR_SAVE_FAILED`, `STR_UPDATE_FAILED`, `STR_DELETE_FAILED`
2. Register translations (EN/DE)
3. Replace generic calls:
   - Line 464: `mstr(STR_READ_FAILED)` or "Load failed"
   - Line 536: `mstr(STR_SAVE_FAILED)` or "Save failed"
   - Line 580: `mstr(STR_UPDATE_FAILED)` or "Update failed"
   - Line 692: `mstr(STR_DELETE_FAILED)` or "Delete failed"

### mod_gpg
1. Add string entries: `STR_GPG_LOAD_FAILED`, `STR_GPG_SAVE_FAILED`, `STR_GPG_EXPORT_FAILED`
2. Replace generic calls with specific operation names

### mod_totp
1. Add string entries: `STR_TOTP_GENERATE_FAILED`, `STR_TOTP_SETUP_FAILED`
2. Replace generic calls with specific operation names

### mod_fido2
1. Add string entry: `STR_FIDO2_FAILED` or use existing specific error (e.g., `WRONG_PIN`, `TOO_MANY_ATTEMPTS` where appropriate)
2. Consider using FIDO2-specific error codes from `ctap2.h` for more granular feedback

### Alternative Approach
Consider creating a helper function that takes an operation name:
```cpp
void showOperationError(const char* operation) {
    static char buf[64];
    snprintf(buf, sizeof(buf), "%s failed", operation);
    showToastError(buf);
}
// Usage: showOperationError("Load"); showOperationError("Save");
```

## References
- `components/cdc_ui/src/I18n.cpp:232` - Definition of `FAILED` string
- `components/mod_password/src/PasswordModule.cpp:410` - Example of specific error message (`STR_SLOT_ERROR`)
- `components/mod_fido2/src/Fido2Ui.cpp:361-364` - Example of specific errors (`TOO_MANY_ATTEMPTS`, `WRONG_PIN`)
