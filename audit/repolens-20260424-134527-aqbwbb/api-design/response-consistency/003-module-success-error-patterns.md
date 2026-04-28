---
title: "[HIGH] GPG, Password, and TOTP modules use bare `OK`/`ERROR` without context"
severity: HIGH
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

Three modules (GPG, Password, TOTP) use bare `OK\r\n` and `ERROR\r\n` for success/error responses without any contextual information, while the main serial commands and vCard module use descriptive messages with action details.

**Locations:**
- `components/mod_gpg/src/GpgModule.cpp` - Lines 178, 190, 203
- `components/mod_password/src/PasswordModule.cpp` - Lines 299, 320
- `components/mod_totp/src/TotpModule.cpp` - Lines 262, 281

**Comparison table:**

| Module | Command | Success Response | Error Response |
|--------|---------|------------------|----------------|
| GPG | GPG_GENERATE | `OK\r\n` | `ERROR\r\n` |
| GPG | GPG_EXPORT | PEM data | `ERROR\r\n` |
| GPG | GPG_RESET | `OK\r\n` | `ERROR\r\n` |
| Password | PASSWORD_ADD | `OK\r\n` | `ERROR\r\n` |
| Password | PASSWORD_DEL | `OK\r\n` | `ERROR\r\n` |
| TOTP | TOTP_ADD | `OK\r\n` | `ERROR\r\n` |
| TOTP | TOTP_DEL | `OK\r\n` | `ERROR\r\n` |
| vCard | VCARD_UPDATE | `OK: vCard updated\r\n` | `ERROR: %s\r\n` |
| vCard | VCARD_DEL | `OK: vCard deleted\r\n` | `ERROR: Failed to delete vCard\r\n` |
| Serial | NVS_DEL | `OK: Key '%s.%s' deleted\r\n` | `ERROR: %s\r\n` |

## Impact

**Reduced user feedback:** Users cannot tell what operation succeeded without parsing context:
- `OK` after GPG_GENERATE doesn't confirm the curve or user ID
- `OK` after PASSWORD_ADD doesn't confirm the entry title
- `OK` after TOTP_ADD doesn't confirm the account name

**Inconsistent parser expectations:** External tools must handle:
- Detailed success messages from main commands (`OK: <action> <details>`)
- Bare `OK` from GPG/Password/TOTP modules
- Descriptive messages from vCard module

**Debugging difficulty:** When operations fail, users don't know why:
- GPG_GENERATE `ERROR` - was it curve selection, user ID, or key generation?
- PASSWORD_ADD `ERROR` - was it slot full, invalid data, or memory?
- TOTP_DEL `ERROR` - was it slot lookup or deletion?

**Cross-module inconsistency:** Similar operations have different response formats:
- `GPG_RESET` → `OK\r\n`
- `VCARD_DEL` → `OK: vCard deleted\r\n`
- `NVS_DEL` → `OK: Key '%s.%s' deleted\r\n`

## Evidence

**GPG module (lines 178, 190, 203):**
```cpp
// Line 178 - GPG_GENERATE
cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");

// Line 190 - GPG_EXPORT
cdc::serial::Console::printf("ERROR\r\n");

// Line 203 - GPG_RESET
cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
```

**Password module (lines 299, 320):**
```cpp
// Line 299 - PASSWORD_ADD
cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");

// Line 320 - PASSWORD_DEL
cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
```

**TOTP module (lines 262, 281):**
```cpp
// Line 262 - TOTP_ADD
cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");

// Line 281 - TOTP_DEL
cdc::serial::Console::printf(ok ? "ERROR\r\n" : "OK\r\n");  // Note: inverted!
```

**vCard module (lines 329, 331, 425, 427) - More consistent:**
```cpp
// Line 329 - VCARD_UPDATE success
Console::printf("OK: vCard updated\r\n");

// Line 331 - VCARD_UPDATE error
Console::printf("ERROR: %s\r\n", err[0] ? err : "Invalid vCard");

// Line 425 - VCARD_DEL success
serial::Console::printf("OK: vCard deleted\r\n");

// Line 427 - VCARD_DEL error
serial::Console::printf("ERROR: Failed to delete vCard\r\n");
```

**Main serial commands (components/serial_cmd/src/SerialCmd.cpp):**
```cpp
// Line 637 - Descriptive success
Console::printf("OK: Namespace '%s' erased\r\n", ns);

// Line 645 - Descriptive success
Console::printf("OK: Key '%s.%s' deleted\r\n", ns, key);

// Line 716 - Descriptive success with values
Console::printf("OK: Time set to %02d:%02d:%02d\r\n", h, m, s);
```

## Recommended Fix

### Standardize on descriptive response format

**Format convention:**
```
SUCCESS: <action> [<details>]
ERROR: <action> [<details>]
```

### GPG module updates (lines 178, 190, 203):

```cpp
// Line 178 - GPG_GENERATE
if (ok) {
    cdc::serial::Console::printf("OK: GPG key generated (%s)\r\n", 
                                  curve == CDC_CURVE_P256 ? "P-256" : "Ed25519");
} else {
    cdc::serial::Console::printf("ERROR: GPG key generation failed\r\n");
}

// Line 190 - GPG_EXPORT
if (!gpg_export_pubkey_pem(pem_buf, sizeof(pem_buf), &out_len)) {
    cdc::serial::Console::printf("ERROR: Public key export failed\r\n");
    return;
}

// Line 203 - GPG_RESET
if (ok) {
    cdc::serial::Console::printf("OK: GPG keys reset\r\n");
} else {
    cdc::serial::Console::printf("ERROR: GPG reset failed\r\n");
}
```

### Password module updates (lines 299, 320):

```cpp
// Line 299 - PASSWORD_ADD
if (ok) {
    cdc::serial::Console::printf("OK: Password entry '%s' added\r\n", entry.title);
} else {
    cdc::serial::Console::printf("ERROR: Password add failed\r\n");
}

// Line 320 - PASSWORD_DEL
if (ok) {
    cdc::serial::Console::printf("OK: Password entry %u deleted\r\n", index);
} else {
    cdc::serial::Console::printf("ERROR: Password delete failed\r\n");
}
```

### TOTP module updates (lines 262, 281):

```cpp
// Line 262 - TOTP_ADD
if (ok) {
    cdc::serial::Console::printf("OK: TOTP '%s' added\r\n", name);
} else {
    cdc::serial::Console::printf("ERROR: TOTP add failed\r\n");
}

// Line 281 - TOTP_DEL (also fix inverted logic)
if (ok) {
    cdc::serial::Console::printf("OK: TOTP '%s' deleted\r\n", name);  // Need name lookup
} else {
    cdc::serial::Console::printf("ERROR: TOTP delete failed\r\n");
}
```

### Optional: Create helper macros for consistency

```cpp
// In a shared header (e.g., mod_totp/TotpModule.h or a module_utils.h)
#define MODULE_SUCCESS(fmt, ...) cdc::serial::Console::printf("OK: " fmt "\r\n", ##__VA_ARGS__)
#define MODULE_ERROR(fmt, ...) cdc::serial::Console::printf("ERROR: " fmt "\r\n", ##__VA_ARGS__)

// Usage:
MODULE_SUCCESS("TOTP '%s' added", name);
MODULE_ERROR("TOTP add failed");
```

## References

- Existing findings: `001-inconsistent-success-prefixes.md`, `002-inconsistent-error-format.md`, `006-totp-minimal-responses.md`
- vCard module shows better pattern: `components/mod_vcard/src/VcardModule.cpp`
- Main serial commands reference: `components/serial_cmd/src/SerialCmd.cpp`
