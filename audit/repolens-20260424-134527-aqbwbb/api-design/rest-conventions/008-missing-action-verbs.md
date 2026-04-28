---
title: "[LOW] Missing action verbs for CRUD operations"
severity: LOW
domain: api-design
lens: serial-command-api
labels:
  - "audit:api-design/rest-conventions"
---

## Summary
Some modules lack complete CRUD (Create, Read, Update, Delete) command sets:

**Complete CRUD:**
- Password: `PASSWORD_LIST`, `PASSWORD_GET`, `PASSWORD_ADD`, `PASSWORD_DEL` (no UPDATE)
- TOTP: `TOTP_LIST`, `TOTP_GET`, `TOTP_ADD`, `TOTP_DEL` (no UPDATE)
- vCard: `VCARD_GET`, `VCARD_SET`, `VCARD_DELETE` (no LIST - makes sense for single object)

**Partial CRUD:**
- GPG: `GPG_STATUS`, `GPG_GENERATE`, `GPG_EXPORT`, `GPG_RESET` (uses different verbs)
- NVS: `NVS_LIST`, `NVS_READ`, `NVS_DEL`, `NVS_CLEAR` (no UPDATE - use DELETE + READ)

**Files**:
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/mod_password/src/PasswordModule.cpp`
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/mod_totp/src/TotpModule.cpp`
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/mod_gpg/src/GpgModule.cpp`

## Impact
**Completeness**: Users might expect UPDATE commands for password and TOTP modules.

**Workarounds**: Current pattern requires DELETE + ADD for updates, which is verbose.

## Evidence
```cpp
// Password module - missing UPDATE
reg.registerCommand({"PASSWORD_LIST", "List password entries", cmd_password_list, CMD_MODULE, true});
reg.registerCommand({"PASSWORD_GET", "Get password entry", cmd_password_get, CMD_MODULE, true});
reg.registerCommand({"PASSWORD_ADD", "Add password entry", cmd_password_add, CMD_MODULE, true});
reg.registerCommand({"PASSWORD_DEL", "Delete password entry", cmd_password_del, CMD_MODULE, true});
// Missing: PASSWORD_UPDATE or PASSWORD_SET

// TOTP module - missing UPDATE
reg.registerCommand({"TOTP_LIST", "List all TOTP accounts", cmd_totp_list, CMD_MODULE, true});
reg.registerCommand({"TOTP_GET", "Generate TOTP code by index", cmd_totp_get, CMD_MODULE, true});
reg.registerCommand({"TOTP_ADD", "Add TOTP account", cmd_totp_add, CMD_MODULE, true});
reg.registerCommand({"TOTP_DEL", "Delete TOTP account by index", cmd_totp_del, CMD_MODULE, true});
// Missing: TOTP_UPDATE or TOTP_SET

// GPG module - different pattern
registry.registerCommand({"GPG_STATUS", "Show GPG status", cmd_gpg_status, CMD_MODULE, true});
registry.registerCommand({"GPG_GENERATE", "Generate GPG keys", cmd_gpg_generate, CMD_MODULE, true});
registry.registerCommand({"GPG_EXPORT", "Export public keys", cmd_gpg_export, CMD_MODULE, true});
registry.registerCommand({"GPG_RESET", "Reset GPG keys", cmd_gpg_reset, CMD_MODULE, true});
// Uses domain-specific verbs instead of CRUD
```

**Note**: The TOTP module actually has `updateAccount()` method in `TotpStore`, but no serial command exposes it.

## Recommended Fix
**Option 1: Add UPDATE commands**
```cpp
// Password module
reg.registerCommand({"PASSWORD_UPDATE", "Update password entry", cmd_password_update, CMD_MODULE, true});

// TOTP module
reg.registerCommand({"TOTP_UPDATE", "Update TOTP account", cmd_totp_update, CMD_MODULE, true});
```

**Option 2: Use SET instead of UPDATE (more consistent with existing commands)**
```cpp
// Password module
reg.registerCommand({"PASSWORD_SET", "Set/Update password entry", cmd_password_set, CMD_MODULE, true});

// TOTP module
reg.registerCommand({"TOTP_SET", "Set/Update TOTP account", cmd_totp_set, CMD_MODULE, true});
```

**Option 3: Document the DELETE+ADD pattern**
```
// To update an entry:
1. PASSWORD_GET <index>  // view current values
2. PASSWORD_DEL <index>  // delete old entry
3. PASSWORD_ADD ...      // add with new values
```

**Recommendation**: Option 2 (SET) is most consistent with existing `SET_TIME`, `SET_NAME`, `SET_INFO` commands.

## References
- REST: PUT/PATCH for updates
- CRUD: Create, Read, Update, Delete
- SQL: SELECT, INSERT, UPDATE, DELETE
