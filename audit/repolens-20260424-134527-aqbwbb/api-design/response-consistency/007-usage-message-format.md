---
title: "[LOW] Usage messages use inconsistent argument naming conventions"
severity: LOW
domain: api-design
lens: response-consistency
labels:
  - "audit:api-design/response-consistency"
  - "serial-commands"
  - "mod_gpg"
  - "mod_password"
  - "mod_totp"
---

## Summary

Usage messages across modules use inconsistent argument naming conventions - some use uppercase with angle brackets, others use lowercase without angle brackets, making the CLI harder to learn.

**Locations:**
- `components/mod_gpg/src/GpgModule.cpp` - Lines 157, 170
- `components/mod_password/src/PasswordModule.cpp` - Lines 222, 264, 271, 310
- `components/mod_totp/src/TotpModule.cpp` - Lines 237, 242, 271, 290

**Current usage message formats:**

| Module | Command | Format | Example |
|--------|---------|--------|---------|
| GPG | GPG_GENERATE | Uppercase + angle brackets | `GPG_GENERATE <curve> <user_id>` |
| Password | PASSWORD_GET | Uppercase + angle brackets | `PASSWORD_GET <index>` |
| Password | PASSWORD_ADD | Uppercase + angle brackets | `PASSWORD_ADD <title> <username|- > ...` |
| Password | PASSWORD_DEL | Uppercase + angle brackets | `PASSWORD_DEL <index>` |
| TOTP | TOTP_ADD | Lowercase, no brackets | `TOTP_ADD name secret [issuer] ...` |
| TOTP | TOTP_DEL | Uppercase + angle brackets | `TOTP_DEL <index>` |
| TOTP | TOTP_GET | Uppercase + angle brackets | `TOTP_GET <index>` |

## Impact

**Learning curve:** Users must remember different formatting styles for different modules:
- GPG/Password: `<argument_name>` (uppercase, angle brackets)
- TOTP_ADD: `argument_name` (lowercase, no brackets)

**Visual consistency:** The CLI feels less polished with mixed formatting.

**Discoverability:** Angle brackets clearly indicate required arguments, but TOTP_ADD omits them inconsistently.

## Evidence

**GPG module (lines 157, 170):**
```cpp
// Uppercase with angle brackets
cdc::serial::Console::printf("Usage: GPG_GENERATE <curve> <user_id>\r\n");
```

**Password module (lines 222, 264, 271, 310):**
```cpp
// Line 222 - Uppercase with angle brackets
cdc::serial::Console::printf("Usage: PASSWORD_GET <index>\r\n");

// Line 264 - Uppercase with mixed notation
cdc::serial::Console::printf("Usage: PASSWORD_ADD <title> <username|- > <password> <url|- > [totpSlot] [notes]\r\n");

// Note: Uses `<username|- >` for required with default, `[totpSlot]` for optional
```

**TOTP module (lines 237, 242, 271, 290):**
```cpp
// Lines 237, 242 - Lowercase, no brackets (INCONSISTENT!)
cdc::serial::Console::printf("Usage: TOTP_ADD name secret [issuer] [digits] [period] [algo]\r\n");

// Lines 271, 290 - Uppercase with angle brackets (consistent with other modules)
cdc::serial::Console::printf("Usage: TOTP_DEL <index>\r\n");
cdc::serial::Console::printf("Usage: TOTP_GET <index>\r\n");
```

**Comparison:**

```
GPG:      Usage: GPG_GENERATE <curve> <user_id>
Password: Usage: PASSWORD_GET <index>
TOTP:     Usage: TOTP_ADD name secret [issuer] [digits] [period] [algo]  ← Different!
          Usage: TOTP_DEL <index>                                         ← Same as others
```

**Notation inconsistencies:**

1. **Angle brackets:**
   - Most commands: `<arg>` for required
   - TOTP_ADD: `name` (no brackets)

2. **Case:**
   - Most commands: `<curve>`, `<index>` (lowercase in brackets)
   - TOTP_ADD: `name`, `secret` (lowercase without brackets)

3. **Optional notation:**
   - Password: `[totpSlot]` (brackets)
   - TOTP: `[issuer]` (brackets) - consistent here

4. **Special notation:**
   - Password: `<username|- >` (required with default value notation)
   - TOTP: no such notation

## Recommended Fix

### Standardize on uppercase with angle brackets

**Recommended format:**
```
Usage: <COMMAND> <required_arg1> [optional_arg2]
```

**Update TOTP_ADD (lines 237, 242):**
```cpp
// Before
cdc::serial::Console::printf("Usage: TOTP_ADD name secret [issuer] [digits] [period] [algo]\r\n");

// After
cdc::serial::Console::printf("Usage: TOTP_ADD <name> <secret> [issuer] [digits] [period] [algo]\r\n");
```

### Update TOTP module:

```cpp
// Line 237
cdc::serial::Console::printf("Usage: TOTP_ADD <name> <secret> [issuer] [digits] [period] [algo]\r\n");

// Line 242
cdc::serial::Console::printf("Usage: TOTP_ADD <name> <secret> [issuer] [digits] [period] [algo]\r\n");
```

### Optional: Create usage helper function

```cpp
// In a shared header or module-specific header
/**
 * \brief Prints usage message with standard format.
 * \param cmd Command name.
 * \param args Argument format string.
 */
static void printUsage(const char* cmd, const char* args) {
    cdc::serial::Console::printf("Usage: %s %s\r\n", cmd, args);
}

// Usage:
printUsage("TOTP_ADD", "<name> <secret> [issuer] [digits] [period] [algo]");
```

### Document the convention

Add to a shared documentation file or header:

```cpp
/**
 * \brief Serial command usage message format convention:
 * 
 * Usage: <COMMAND> <required_arg1> [optional_arg2]
 * 
 * Rules:
 * - Command name in uppercase
 * - Required arguments in angle brackets: <arg>
 * - Optional arguments in square brackets: [arg]
 * - Argument names in lowercase
 * - Multiple arguments separated by spaces
 * 
 * Examples:
 * - Usage: GPG_GENERATE <curve> <user_id>
 * - Usage: PASSWORD_ADD <title> <username> <password> [url] [totpSlot] [notes]
 * - Usage: TOTP_ADD <name> <secret> [issuer] [digits] [period] [algo]
 */
```

## References

- Existing findings: `005-inconsistent-usage-messages.md` (main serial commands)
- POSIX command-line conventions
- GNU help formatting conventions
