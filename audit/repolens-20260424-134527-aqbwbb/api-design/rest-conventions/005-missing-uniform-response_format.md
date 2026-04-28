---
title: "[MEDIUM] Inconsistent response formats across similar commands"
severity: MEDIUM
domain: api-design
lens: serial-command-api
labels:
  - "audit:api-design/rest-conventions"
---

## Summary
Similar commands return data in inconsistent formats:

**List commands return different formats:**
- `PASSWORD_LIST`: `index: title (slot N)` with slot number
- `TOTP_LIST`: `index: name (slot N)` with slot number
- `NVS_LIST`: `[namespace]` grouped format with key and type

**Get/Read commands return different formats:**
- `PASSWORD_GET`: Multi-line key-value format with all fields
- `TOTP_GET`: Single line `code (seconds) [issuer]`
- `TR01_RMEM_READ`: Hex dump format with offset
- `NVS_READ`: `key = value` single line

**Files**:
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/mod_password/src/PasswordModule.cpp:216`
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/mod_totp/src/TotpModule.cpp:193`
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/serial_cmd/src/SerialCmd.cpp:582,1002`

## Impact
**Parsing difficulty**: Serial clients cannot use a uniform parser for similar operations.

**User experience**: Users expect consistent output formatting for similar operations.

**Evidence**:
```cpp
// PASSWORD_LIST output:
// 0: github (slot 150)
// 1: google (slot 151)

// TOTP_LIST output:
// 0: google (slot 32)
// 1: github (slot 33)

// NVS_LIST output:
// [nvs_time]
//  time_zone (str)
// [mod_password]
//  entry_0 (blob)
```

```cpp
// PASSWORD_GET output:
// Title: github
// Username: user@example.com
// Password: secret123
// URL: https://github.com
// TOTP Slot: 32
// Notes: Work account

// TOTP_GET output:
// 123456 (28s) [Google]

// TR01_RMEM_READ output:
// R-Memory Slot 32 (120 bytes):
//   0000: 47 6F 6F 67 6C 65 ...
//   0010: 73 01 0A 1F ...
```

## Recommended Fix
**Standardize output formats:**

**1. List format (all list commands):**
```
<index>: <name> [metadata...]
```
Example: `0: github [slot 150]`

**2. Detail format (all get/read commands):**
```
<field>: <value>
<field>: <value>
...
```
Example:
```
Name: github
Username: user@example.com
Password: secret123
```

**3. Raw format (for hex/binary data):**
```
<name> Slot <N> (<size> bytes):
  <hex dump>
```

**Optionally add format modifiers:**
- `PASSWORD_GET <index> --raw` (structured key-value)
- `PASSWORD_GET <index> --json` (machine-parseable)
- `TR01_RMEM_READ <slot> --text` (try to decode as text)

## References
- REST: Consistent response format (JSON, XML)
- Content-Type headers for different formats
- Unix: `ls` (human) vs `ls -1` (machine) vs `ls -l` (detailed)
