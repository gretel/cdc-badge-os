---
title: "[LOW] Inconsistent Module Prefix Naming Convention"
severity: LOW
domain: api-design
lens: rest-conventions
labels:
  - "audit:api-design/rest-conventions"
---

## Summary
Module command prefixes use inconsistent naming patterns, which could confuse users when discovering available commands.

**Locations:**
- `components/serial_cmd/src/SerialCmd.cpp:1445-1488` - Built-in command registration
- `components/mod_totp/src/TotpModule.cpp:96-100` - TOTP module
- `components/mod_gpg/src/GpgModule.cpp:105-110` - GPG module
- `components/mod_password/src/PasswordModule.cpp:120-125` - Password module

## Impact
**Discoverability:** Users may not intuitively know the correct prefix for each module.

**Consistency:** New modules may continue the inconsistent pattern.

**Documentation:** Requires explicit mapping of modules to command prefixes.

## Evidence

### Current Module Prefixes:

| Module | Prefix | Example Commands |
|--------|--------|------------------|
| System | (none) | HELP, PING, STATUS |
| NVS | NVS | NVS_LIST, NVS_READ, NVS_DEL |
| Time | (none) | GET_TIME, SET_TIME, GET_DATE |
| Display | (none) | SET_NAME, SET_INFO |
| PIN | (none) | PIN_STATUS, PIN_RESET |
| TROPIC01 | TR01 | TR01_STATUS, TR01_INFO, TR01_WIPE |
| TOTP | TOTP | TOTP_LIST, TOTP_ADD, TOTP_DEL |
| GPG | GPG | GPG_STATUS, GPG_GENERATE, GPG_EXPORT |
| Password | PASSWORD | PASSWORD_LIST, PASSWORD_ADD, PASSWORD_DEL |

### Inconsistencies:

1. **Some modules use prefixes, others don't:**
   ```
   NVS_LIST          (has prefix)
   GET_TIME          (no module prefix, just verb-time)
   TOTP_LIST         (has prefix)
   GPG_STATUS        (has prefix)
   PIN_STATUS        (has prefix)
   ```

2. **Abbreviated vs full module names:**
   ```
   TR01_STATUS       (abbreviated: TROPIC01 -> TR01)
   GPG_STATUS        (abbreviation: already short)
   TOTP_LIST         (abbreviation: already short)
   PASSWORD_LIST     (full word)
   ```

3. **Mixed grouping:**
   ```
   // Time commands - no prefix, verb-first
   GET_TIME
   GET_DATE
   SET_TIME
   SET_DATE

   // Display commands - no prefix, verb-first
   SET_NAME
   SET_INFO
   SET_INFO2

   // NVS commands - prefix-first
   NVS_LIST
   NVS_READ
   NVS_DEL
   NVS_CLEAR
   ```

4. **Authentication commands location:**
   ```cpp
   // Auth commands are in "system" namespace, not "auth"
   AUTH           (no prefix)
   LOGOUT         (no prefix)
   ```

## Recommended Fix

### Option 1: Prefix All Module Commands (Recommended)

```
// Time module
TIME_GET, TIME_SET, TIME_GET_DATE, TIME_SET_DATE

// Display module  
DISP_SET_NAME, DISP_SET_INFO, DISP_SET_INFO2

// PIN module
PIN_GET_STATUS, PIN_RESET

// Auth module
AUTH_LOGIN, AUTH_LOGOUT
```

### Option 2: Remove All Prefixes (if namespace collision unlikely)

```
// Keep current pattern for short commands
LIST_TOTP, ADD_TOTP, DEL_TOTP, GET_TOTP

// Or verb-first pattern
GET_TOTP_LIST, ADD_TOTP_ACCOUNT, DEL_TOTP_ACCOUNT
```

### Option 3: Hybrid Approach (Current-ish)

Keep prefixes for complex modules, none for simple system commands:

```
// System commands - no prefix
HELP, PING, STATUS, REBOOT

// Simple modules - no prefix (if no collision)
GET_TIME, SET_TIME, GET_DATE, SET_DATE
SET_NAME, SET_INFO, SET_INFO2
AUTH, LOGOUT

// Complex modules - use prefix
TOTP_LIST, TOTP_ADD, TOTP_DEL
GPG_STATUS, GPG_GENERATE, GPG_EXPORT
PASSWORD_LIST, PASSWORD_ADD, PASSWORD_DEL
TR01_STATUS, TR01_WIPE, etc.
NVS_LIST, NVS_READ, NVS_DEL
```

### Recommendation:

**Stick with Option 3** (hybrid) but document the convention:

1. **System commands** (single-word, universally useful): No prefix
2. **Simple domain commands** (time, display): No prefix if no collision
3. **Complex modules** (multiple commands): Use prefix

### Minimum Changes Needed:

1. **Add TIME prefix for clarity:**
   ```
   TIME_GET, TIME_SET, TIME_DATE_GET, TIME_DATE_SET
   ```

2. **Add AUTH prefix:**
   ```
   AUTH_LOGIN, AUTH_LOGOUT
   ```

3. **Document the pattern** in serial command documentation.

## References

- [Google API Naming Conventions](https://cloud.google.com/apis/design/naming_conventions)
- [REST API Design - Resource Naming](https://restfulapi.net/resource-naming/)
