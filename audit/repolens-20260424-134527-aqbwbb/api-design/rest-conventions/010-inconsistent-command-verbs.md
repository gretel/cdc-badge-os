---
title: "[MEDIUM] Inconsistent Command Verb Usage Across Modules"
severity: MEDIUM
domain: api-design
lens: rest-conventions
labels:
  - "audit:api-design/rest-conventions"
---

## Summary
The serial command interface uses inconsistent verb naming conventions across different modules, making the API less predictable for developers and users.

**Locations:**
- `components/serial_cmd/src/SerialCmd.cpp:1445-1488` - Built-in commands
- `components/mod_totp/src/TotpModule.cpp:324-328` - TOTP commands
- `components/mod_password/src/PasswordModule.cpp:327-332` - Password commands
- `components/mod_gpg/src/GpgModule.cpp:114-119` - GPG commands

## Impact
**Maintainability burden:** New developers must memorize module-specific conventions rather than applying consistent patterns.

**User experience:** Users expecting consistent command structures may use incorrect verbs (e.g., `TOTP_CREATE` instead of `TOTP_ADD`, `PASSWORD_UPDATE` instead of editing via UI).

**Documentation overhead:** Each module requires separate documentation for its naming conventions.

## Evidence

### Verb Inconsistencies Found:

1. **Create/Add operations use different verbs:**
   ```
   TOTP_ADD          (mod_totp)
   PASSWORD_ADD      (mod_password)
   GPG_GENERATE      (mod_gpg)     <- Should be GPG_ADD for consistency?
   ```

2. **Delete operations use abbreviated verb:**
   ```
   TOTP_DEL
   PASSWORD_DEL
   TR01_ECC_DEL
   TR01_RMEM_DEL
   ```
   While consistent within themselves, `DEL` is abbreviated vs full `DELETE`.

3. **Mixed verb-object vs object-verb ordering:**
   ```
   SET_TIME          (Verb-Object)
   GET_TIME          (Verb-Object)
   TR01_STATUS       (Module-Noun, no verb)
   GPG_STATUS        (Module-Noun, no verb)
   ```

4. **Update operations missing:**
   - No `TOTP_UPDATE` command (UI uses wizard, serial only has ADD/DEL/GET)
   - No `PASSWORD_UPDATE` command
   - `SET_NAME`, `SET_INFO`, `SET_INFO2` exist but are generic display commands

### Current Command Inventory:

**System commands (SerialCmd.cpp:1445):**
```
HELP, PING, STATUS, MEM, ERROR_LOG, REBOOT
```

**NVS commands:**
```
NVS_LIST, NVS_READ, NVS_DEL, NVS_CLEAR
```

**Time commands:**
```
GET_TIME, GET_DATE, SET_TIME, SET_DATE
```

**Display commands:**
```
SET_NAME, SET_INFO, SET_INFO2
```

**TOTP commands (TotpModule.cpp:324):**
```
TOTP_LIST, TOTP_ADD, TOTP_DEL, TOTP_GET
```

**Password commands (PasswordModule.cpp:327):**
```
PASSWORD_LIST, PASSWORD_GET, PASSWORD_ADD, PASSWORD_DEL
```

**GPG commands (GpgModule.cpp:114):**
```
GPG_STATUS, GPG_GENERATE, GPG_EXPORT, GPG_RESET
```

**TROPIC01 commands (SerialCmd.cpp:1465):**
```
TR01_STATUS, TR01_INFO, TR01_SESSION, TR01_SLOTS,
TR01_RMEM_READ, TR01_ECC_DEL, TR01_RMEM_DEL,
TR01_RESYNC, TR01_CACHE_REBUILD, TR01_CLEANUP, TR01_WIPE
```

## Recommended Fix

Choose **one** of the following approaches and apply it consistently:

### Option A: Standardize on Full Verbs (Recommended)
```
TOTP_ADD      -> TOTP_CREATE (or keep ADD)
TOTP_DEL      -> TOTP_DELETE
PASSWORD_DEL  -> PASSWORD_DELETE
GPG_GENERATE  -> GPG_CREATE

Add missing UPDATE commands:
TOTP_UPDATE   (for updating existing accounts)
PASSWORD_UPDATE
```

### Option B: Keep Abbreviations but Be Consistent
```
Keep: TOTP_ADD, PASSWORD_ADD
Change: GPG_GENERATE -> GPG_ADD
Keep: TOTP_DEL, PASSWORD_DEL (DEL is acceptable abbreviation)
Add: TOTP_UPD, PASSWORD_UPD (for updates)
```

### Option C: Adopt REST-like HTTP Method Semantics
(If serial commands were to mirror REST)
```
Create:    TOTP_CREATE, PASSWORD_CREATE
Read:      TOTP_GET, PASSWORD_GET (or just TOTP, PASSWORD)
List:      TOTP_LIST, PASSWORD_LIST
Update:    TOTP_UPDATE, PASSWORD_UPDATE
Delete:    TOTP_DELETE, PASSWORD_DELETE
```

### Additional Recommendations:

1. **Add UPDATE commands** for modules that support editing:
   - `TOTP_UPDATE <index> <field> <value>` - Update specific field
   - `PASSWORD_UPDATE <index> <field> <value>` - Update specific field

2. **Standardize status/info commands:**
   - Either `GET_STATUS`, `GET_INFO` (verb-object)
   - Or `STATUS`, `INFO` (noun-only, with context)

3. **Document the convention** in `README.md` or `docs/serial_commands.md` for future reference.

## References

- [REST API Design Rulebook - Naming Resources](http://restapiguide.org/)
- [Google API Design Guide - Standard Methods](https://google.aip.dev/135)
- [Microsoft REST API Guidelines - Verbs](https://github.com/microsoft/api-guidelines/blob/vNext/azure/Guidelines.md#resource-verbs)
