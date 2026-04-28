---
title: "[MEDIUM] Inconsistent Parameter Ordering in Commands"
severity: MEDIUM
domain: api-design
lens: rest-conventions
labels:
  - "audit:api-design/rest-conventions"
---

## Summary
Serial commands use inconsistent parameter ordering conventions, making it harder for users to predict command syntax without consulting documentation.

**Locations:**
- `components/mod_totp/src/TotpModule.cpp:237-252` - TOTP_ADD command parsing
- `components/mod_password/src/PasswordModule.cpp:249-289` - PASSWORD_ADD command parsing
- `components/mod_gpg/src/GpgModule.cpp:147-177` - GPG_GENERATE command parsing
- `components/serial_cmd/src/SerialCmd.cpp:1465` - TR01 commands

## Impact
**Learning curve:** Users must memorize parameter order for each command rather than applying a consistent mental model.

**Error rate:** Increased likelihood of users providing parameters in wrong order, especially when commands have similar names.

**Documentation dependency:** Every command requires explicit syntax documentation.

## Evidence

### Parameter Order Inconsistencies:

1. **TOTP_ADD** (TotpModule.cpp:237):
   ```
   Usage: TOTP_ADD name secret [issuer] [digits] [period] [algo]
   ```
   - Required: `name`, `secret`
   - Optional: `issuer`, `digits`, `period`, `algo`

2. **PASSWORD_ADD** (PasswordModule.cpp:249):
   ```
   Usage: PASSWORD_ADD <title> <username|- > <password> <url|- > [totpSlot] [notes]
   ```
   - Required: `title`, `username` (or `-`), `password`, `url` (or `-`)
   - Optional: `totpSlot`, `notes`

3. **GPG_GENERATE** (GpgModule.cpp:147):
   ```
   Usage: GPG_GENERATE <curve> <user_id>
   ```
   - Required: `curve`, `user_id`

4. **TR01_RMEM_READ** (SerialCmd.cpp:1465):
   ```
   Usage: TR01_RMEM_READ <slot>
   ```
   - Required: `slot`

### Key Inconsistencies:

1. **Identifier placement varies:**
   - TOTP: `name` (first) identifies the account
   - Password: `title` (first) identifies the entry
   - GPG: `curve` (first) is a parameter, `user_id` (second) identifies
   - TROPIC01: `slot` (first) is the identifier

2. **Optional parameter handling:**
   - TOTP_ADD: Optional params use positional notation
   - PASSWORD_ADD: Uses `-` placeholder for optional required fields
   - No consistent pattern for "skip this field"

3. **Multiple optional parameters:**
   ```
   TOTP_ADD: name secret [issuer] [digits] [period] [algo]
             (6 params, 4 optional, all positional)

   PASSWORD_ADD: title username password url totpSlot notes
                 (6 params, 2 truly optional, but 2 have placeholders)
   ```

## Recommended Fix

### Option 1: Standardize on REST-like Parameter Order
```
<IDENTIFIER> <primary-data> <optional-params...>

Examples:
TOTP_ADD <name> <secret> [issuer] [digits] [period] [algo]
PASSWORD_ADD <title> <password> [username] [url] [totpSlot] [notes]
GPG_CREATE <user_id> [curve]
```

### Option 2: Use Named Parameters
```
TOTP_ADD --name=<name> --secret=<secret> [--issuer=<issuer>] [--digits=<digits>]
PASSWORD_ADD --title=<title> --password=<password> [--username=<username>]
GPG_CREATE --user_id=<id> [--curve=<curve>]
```

### Option 3: Consistent Positional with Documentation
If keeping positional parameters, ensure:
1. **Identifiers always first**
2. **Required params before optional**
3. **Same field order across similar commands**

```
// Standardize all to: <identifier> <primary-value> [options...]
TOTP_ADD <name> <secret> [issuer] [digits] [period] [algo]
PASSWORD_ADD <title> <password> [username] [url] [totp] [notes]
GPG_CREATE <user_id> [curve]
```

### Specific Changes Needed:

1. **PASSWORD_ADD** - Reorder parameters:
   ```
   Current:  title username password url [totp] [notes]
   Suggested: title password [username] [url] [totp] [notes]
   ```

2. **GPG_GENERATE** - Consider reordering:
   ```
   Current:  curve user_id
   Suggested: user_id [curve]  (identifier first, optional second)
   ```

3. **Add named parameter support** (optional but recommended):
   ```
   TOTP_ADD --name=Google --secret=JBSWY3DPEHPK3PXP --issuer=Google
   ```

### Update Documentation
Add usage examples to command registration:
```cpp
reg.registerCommand({
    "TOTP_ADD",
    "Add TOTP account (name secret [issuer] [digits] [period] [algo])",
    cmd_totp_add,
    CMD_MODULE,
    true
});
```

## References

- [RFC 3986 - Uniform Resource Identifiers](https://tools.ietf.org/html/rfc3986)
- [Google API Design Guide - Request Message Names](https://google.aip.dev/121)
- [Cloude API Style Guide - Parameters](https://cloud.google.com/apis/design/design_patterns)
