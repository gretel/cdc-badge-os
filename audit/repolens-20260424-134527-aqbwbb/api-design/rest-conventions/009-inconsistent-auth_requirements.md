---
title: "[MEDIUM] Inconsistent authentication requirements for similar operations"
severity: MEDIUM
domain: api-design
lens: serial-command-api
labels:
  - "audit:api-design/rest-conventions"
---

## Summary
Similar operations have inconsistent authentication requirements (`true` = requires auth, `false` = no auth):

**Module commands (all require auth = `true`):**
- `PASSWORD_LIST`, `PASSWORD_GET`, `PASSWORD_ADD`, `PASSWORD_DEL` (all `true`)
- `TOTP_LIST`, `TOTP_ADD`, `TOTP_DEL`, `TOTP_GET` (all `true`)
- `GPG_STATUS`, `GPG_GENERATE`, `GPG_EXPORT`, `GPG_RESET` (all `true`)

**System commands (mixed):**
- `STATUS`, `MEM`, `PING`, `HELP` (no auth = `false`)
- `REBOOT` (requires auth = `true`)
- `NVS_LIST`, `NVS_READ` (no auth = `false`)
- `NVS_DEL`, `NVS_CLEAR` (requires auth = `true`)
- `TR01_STATUS`, `TR01_INFO`, `TR01_SLOTS` (no auth = `false`)
- `TR01_ECC_DEL`, `TR01_RMEM_DEL`, `TR01_CLEANUP`, `TR01_WIPE` (requires auth = `true`)
- `TR01_SESSION`, `TR01_RESYNC`, `TR01_CACHE_REBUILD` (no auth = `false`)

**Files**:
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/serial_cmd/src/SerialCmd.cpp:1437-1487`
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/mod_totp/src/TotpModule.cpp:325`
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/mod_gpg/src/GpgModule.cpp:114`

## Impact
**Security**: Reading sensitive data (`NVS_READ`, `TR01_STATUS`) doesn't require authentication by default.

**Predictability**: Users might expect all module commands to have the same auth pattern.

**Configuration**: `FEATURE_SECURE_SERIAL` controls auth globally, but per-command auth is only used when feature is disabled.

## Evidence
```cpp
// System commands - mixed auth requirements
reg.registerCommand({"STATUS", "Show system status", cmdStatus, "system", false});  // no auth
reg.registerCommand({"REBOOT", "Restart the device", cmdReboot, "system", true});   // auth
reg.registerCommand({"NVS_LIST", "List NVS entries [namespace]", cmdNvsList, "nvs", false});  // no auth
reg.registerCommand({"NVS_READ", "Read NVS key (ns key)", cmdNvsRead, "nvs", false});         // no auth
reg.registerCommand({"NVS_DEL", "Delete NVS key/namespace", cmdNvsDel, "nvs", true});         // auth
reg.registerCommand({"TR01_STATUS", "Show TR01 status", cmdTr01Status, "tr01", false});       // no auth
reg.registerCommand({"TR01_ECC_DEL", "Delete ECC key slot", cmdTr01EccDel, "tr01", true});    // auth
reg.registerCommand({"TR01_SESSION", "Start/restart TR01 session", cmdTr01Session, "tr01", false}); // no auth

// Module commands - all require auth
reg.registerCommand({"PASSWORD_LIST", "List password entries", cmd_password_list, CMD_MODULE, true});
reg.registerCommand({"TOTP_LIST", "List all TOTP accounts", cmd_totp_list, CMD_MODULE, true});
registry.registerCommand({"GPG_STATUS", "Show GPG status", cmd_gpg_status, CMD_MODULE, true});

// vCard - no auth (inconsistent with other modules)
reg.registerCommand({"VCARD_SET", "Set own vCard", cmdVcardSet, "vcard", false});
reg.registerCommand({"VCARD_GET", "Show own vCard", cmdVcardGet, "vcard", false});
reg.registerCommand({"VCARD_DELETE", "Delete own vCard", cmdVcardDelete, "vcard", false});
```

**Key inconsistencies:**
1. `NVS_READ` (no auth) vs `PASSWORD_GET` (auth) - both read data
2. `TR01_STATUS` (no auth) vs `GPG_STATUS` (auth) - both show status
3. `TR01_SESSION` (no auth) - can start session without auth
4. `VCARD_*` commands (all no auth) - inconsistent with other modules

## Recommended Fix
**Option 1: Require auth for all data access operations**
```cpp
// Change these to require auth:
NVS_READ -> true  // reads potentially sensitive data
TR01_STATUS -> true  // shows secure element state
TR01_SLOTS -> true  // shows slot usage
TR01_SESSION -> true  // affects secure element
VCARD_GET -> true  // reads user data
VCARD_SET -> true  // writes user data
VCARD_DELETE -> true  // deletes user data
```

**Option 2: Document the rationale**
- `STATUS` commands = system health (no auth needed)
- `*_DEL`, `*_ADD`, `*_SET` = write operations (auth required)
- `*_READ`, `*_GET` = data access (depends on sensitivity)

**Option 3: Use FEATURE_SECURE_SERIAL consistently**
When `FEATURE_SECURE_SERIAL` is enabled, ALL commands except `PING` and `AUTH` require authentication. Document this behavior clearly.

**Recommendation**: Option 1 for security-sensitive data (passwords, TOTP, vCard). Option 2 for system diagnostics.

## References
- REST: Authentication for protected resources
- RBAC: Role-based access control
- Least privilege: Minimal auth for read, strict auth for write
