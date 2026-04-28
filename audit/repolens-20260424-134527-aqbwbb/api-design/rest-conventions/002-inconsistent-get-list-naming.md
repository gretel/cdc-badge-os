---
title: "[MEDIUM] Inconsistent retrieval command naming (GET vs LIST)"
severity: MEDIUM
domain: api-design
lens: serial-command-api
labels:
  - "audit:api-design/rest-conventions"
---

## Summary
The API uses inconsistent naming for retrieval operations:
- **List operations**: `PASSWORD_LIST`, `TOTP_LIST`, `NVS_LIST`
- **Get operations**: `PASSWORD_GET`, `TOTP_GET`, `GPG_EXPORT`

While `LIST` (plural) for collections and `GET` (singular) for individual items is conceptually sound, the pattern is not consistently applied:
- `GPG_EXPORT` instead of `GPG_GET` (different verb for similar purpose)
- `TR01_RMEM_READ` instead of `TR01_RMEM_GET` (yet another verb)

**Files**:
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/mod_password/src/PasswordModule.cpp:330`
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/mod_totp/src/TotpModule.cpp:325`
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/mod_gpg/src/GpgModule.cpp:114`
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/serial_cmd/src/SerialCmd.cpp:1471`

## Impact
**Predictability**: Users expect consistent verbs for similar operations:
- `PASSWORD_LIST` + `PASSWORD_GET` (good pattern)
- `TOTP_LIST` + `TOTP_GET` (good pattern)
- `GPG_STATUS` + `GPG_EXPORT` (inconsistent - why not `GPG_GET`?)
- `TR01_RMEM_READ` (uses READ instead of GET)

**Learning curve**: Each module introduces slight variations that users must memorize.

## Evidence
```cpp
// Good: Consistent LIST/GET pattern
reg.registerCommand({"PASSWORD_LIST", "List password entries", cmd_password_list, CMD_MODULE, true});
reg.registerCommand({"PASSWORD_GET", "Get password entry", cmd_password_get, CMD_MODULE, true});

reg.registerCommand({"TOTP_LIST", "List all TOTP accounts", cmd_totp_list, CMD_MODULE, true});
reg.registerCommand({"TOTP_GET", "Generate TOTP code by index", cmd_totp_get, CMD_MODULE, true});

// Inconsistent: GPG uses STATUS and EXPORT
registry.registerCommand({"GPG_STATUS", "Show GPG status", cmd_gpg_status, CMD_MODULE, true});
registry.registerCommand({"GPG_EXPORT", "Export public keys", cmd_gpg_export, CMD_MODULE, true});

// Inconsistent: TROPIC01 uses READ
reg.registerCommand({"TR01_RMEM_READ", "Read R-Memory slot", cmdTr01RmemRead, "tr01", false});
```

## Recommended Fix
Standardize retrieval verbs:

**Option 1: GET for single items, LIST for collections**
```cpp
// Change GPG to use GET
GPG_GET  // instead of GPG_EXPORT (or keep EXPORT for PEM format specifically)

// Change TR01 to use GET
TR01_RMEM_GET  // instead of TR01_RMEM_READ
```

**Option 2: READ for all retrieval operations (more explicit)**
```cpp
// Change all to use READ
PASSWORD_READ, TOTP_READ, NVS_READ (already used), TR01_RMEM_READ (already used)
```

**Option 3: Keep current verbs but document the rationale**
- `LIST` = enumerate collection
- `GET` = retrieve by index with detail
- `READ` = raw data access (TROPIC01 level)
- `EXPORT` = formatted output (GPG public key in PEM)

If Option 3, document this in a command naming guide.

## References
- REST API conventions: GET for retrieval (HTTP method)
- Command-line tools: `ls` (list) vs `cat` (read) vs `show` (display)
- SQL: SELECT for retrieval, with variations (SELECT * vs SELECT column)
