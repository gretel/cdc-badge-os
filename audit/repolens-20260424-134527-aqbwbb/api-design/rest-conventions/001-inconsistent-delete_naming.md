---
title: "[MEDIUM] Inconsistent delete command naming across modules"
severity: MEDIUM
domain: api-design
lens: serial-command-api
labels:
  - "audit:api-design/rest-conventions"
---

## Summary
The serial command API uses inconsistent naming for delete operations across different modules:
- **Password module**: `PASSWORD_DEL` (short form)
- **TOTP module**: `TOTP_DEL` (short form)
- **GPG module**: `GPG_RESET` (different verb entirely)
- **vCard module**: `VCARD_DELETE` (full word form)
- **TROPIC01**: `TR01_ECC_DEL`, `TR01_RMEM_DEL` (short form)
- **NVS**: `NVS_DEL`, `NVS_CLEAR` (short form + clear)

**Files**:
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/mod_password/src/PasswordModule.cpp:330`
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/mod_totp/src/TotpModule.cpp:325`
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/mod_gpg/src/GpgModule.cpp:114`
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/mod_vcard/src/VcardModule.cpp`

## Impact
**Consistency/Maintainability**: Users must memorize different naming conventions for the same operation:
- `PASSWORD_DEL index` vs `VCARD_DELETE` (why "DEL" vs "DELETE"?)
- `GPG_RESET` vs `TR01_WIPE` (different verbs for similar operations)
- Cognitive overhead increases with each new module

**Predictability**: Following REST conventions, similar operations should use consistent verbs. The mix of `DEL` (abbreviation) and `DELETE` (full word) creates unnecessary confusion.

## Evidence
```cpp
// Password module - uses "DEL"
reg.registerCommand({"PASSWORD_DEL", "Delete password entry", cmd_password_del, CMD_MODULE, true});

// TOTP module - uses "DEL"
reg.registerCommand({"TOTP_DEL", "Delete TOTP account by index", cmd_totp_del, CMD_MODULE, true});

// GPG module - uses "RESET" (not even "DEL")
registry.registerCommand({"GPG_RESET", "Reset GPG keys", cmd_gpg_reset, CMD_MODULE, true});

// vCard module - uses "DELETE" (full word)
reg.registerCommand({"VCARD_DELETE", "Delete own vCard", cmdVcardDelete, "vcard", false});

// TROPIC01 - mixes "DEL" and "WIPE"
reg.registerCommand({"TR01_ECC_DEL", "Delete ECC key slot", cmdTr01EccDel, "tr01", true});
reg.registerCommand({"TR01_WIPE", "Factory reset (TR01_WIPE CONFIRM)", cmdTr01Wipe, "tr01", true});
```

## Recommended Fix
Standardize on one naming convention across all modules. Recommended approach:

**Option 1: Use full word "DELETE" (more readable)**
```cpp
// Change all modules to use DELETE
PASSWORD_DELETE, TOTP_DELETE, GPG_DELETE, VCARD_DELETE, TR01_ECC_DELETE, TR01_RMEM_DELETE
```

**Option 2: Use consistent "DEL" (shorter)**
```cpp
// Change vCard to match others
VCARD_DEL  // instead of VCARD_DELETE
```

**Option 3: Keep backward compatibility with aliases**
```cpp
// Register both old and new names, deprecate old ones
reg.registerCommand({"PASSWORD_DEL", "Delete password entry (deprecated: use PASSWORD_DELETE)", cmd_password_del, CMD_MODULE, true});
reg.registerCommand({"PASSWORD_DELETE", "Delete password entry", cmd_password_del, CMD_MODULE, true});
```

Pick one convention and apply it consistently. Document the convention in a style guide for future module development.

## References
- REST API Design: Consistency in HTTP methods (DELETE is standard)
- Command-Line Interface Best Practices: Consistent verb usage
- RFC 7231: DELETE method semantics (for conceptual parallel)
