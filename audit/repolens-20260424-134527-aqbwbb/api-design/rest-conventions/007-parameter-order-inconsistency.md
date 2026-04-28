---
title: "[LOW] Inconsistent parameter ordering for similar operations"
severity: LOW
domain: api-design
lens: serial-command-api
labels:
  - "audit:api-design/rest-conventions"
---

## Summary
Similar commands use different parameter orders:

**NVS operations:**
- `NVS_READ <namespace> <key>` (ns first, then key)
- `NVS_DEL <namespace> [key]` (ns first, then optional key)

**Time operations:**
- `SET_TIME <HH:MM:SS>` (single formatted string)
- `SET_DATE <DD.MM.YYYY>` (single formatted string)

**Password operations:**
- `PASSWORD_ADD <title> <username> <password> <url> [totpSlot] [notes]`
- `PASSWORD_GET <index>` (single index)
- `PASSWORD_DEL <index>` (single index)

**TOTP operations:**
- `TOTP_ADD <name> <secret> [issuer] [digits] [period] [algo]`
- `TOTP_GET <index>` (single index)
- `TOTP_DEL <index>` (single index)

**Files**:
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/serial_cmd/src/SerialCmd.cpp:582,612`
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/mod_password/src/PasswordModule.cpp:250,308`
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/mod_totp/src/TotpModule.cpp:230,271`

## Impact
**Predictability**: Minor cognitive overhead when switching between command groups.

**Learning curve**: New users must memorize parameter order for each command group.

## Evidence
```cpp
// NVS commands use namespace-first ordering
// Usage: NVS_READ <namespace> <key>
// Usage: NVS_DEL <namespace> [key]

// Time commands use formatted string
// Usage: SET_TIME HH:MM:SS
// Usage: SET_DATE DD.MM.YYYY

// Password commands use index for get/del
// Usage: PASSWORD_GET <index>
// Usage: PASSWORD_DEL <index>
// Usage: PASSWORD_ADD <title> <username> <password> <url> [totpSlot] [notes]

// TOTP commands use index for get/del
// Usage: TOTP_GET <index>
// Usage: TOTP_DEL <index>
// Usage: TOTP_ADD <name> <secret> [issuer] [digits] [period] [algo]
```

**Inconsistencies:**
1. NVS: `NVS_READ` requires namespace, but what if you want to search by key?
2. Time: `SET_TIME` and `SET_DATE` use formatted strings, but `GET_TIME` and `GET_DATE` return them
3. Password/TOTP: `ADD` takes many parameters, but `GET`/`DEL` take index - this is actually good design

## Recommended Fix
**Current design is mostly reasonable. Minor improvements:**

**1. Document parameter order clearly:**
```cpp
// Usage: PASSWORD_ADD <title> <username> <password> <url> [totpSlot] [notes]
//        All fields except title are space-separated.
//        Use '-' for optional fields to skip them.
```

**2. Consider named parameters for complex commands:**
```cpp
// Instead of positional parameters:
PASSWORD_ADD github user secret https://github.com 32 Work

// Allow named parameters:
PASSWORD_ADD --title=github --user=user --pass=secret --url=https://github.com --totp=32 --notes=Work
```

**3. Consider JSON input for complex objects:**
```cpp
// Single JSON argument:
PASSWORD_ADD '{"title":"github","user":"user","pass":"secret","url":"https://github.com","totp":32}'
```

**4. Keep simple commands simple:**
- `GET_TIME`, `SET_TIME` are fine as-is
- `GET_DATE`, `SET_DATE` are fine as-is

## References
- REST: Query parameter ordering doesn't matter (key=value pairs)
- Unix: Positional arguments vs flags (`-o option`)
- GNU CLI: Long options (`--name=value`) vs short options (`-n value`)
