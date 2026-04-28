---
title: "[LOW] GPG_EXPORT command documentation missing output format"
severity: LOW
domain: api-design/api-documentation
lens: serial-commands
labels:
  - "audit:api-design/api-documentation"
---

## Summary
The `GPG_EXPORT` command documentation does not describe the output format or structure of the exported public key.

**File:** `docs/SERIAL_COMMANDS.md` (line 109)

## Impact
Users do not know what format to expect when exporting GPG public keys, making it harder to understand how to use the output (e.g., for importing into GnuPG or displaying as QR code).

## Evidence
**Documentation (SERIAL_COMMANDS.md):**
```markdown
| `GPG_EXPORT` | Export public keys `[AUTH]` |
```

**Implementation (components/mod_gpg/src/GpgModule.cpp:187-196):**
```cpp
static void cmd_gpg_export(const char* args) {
    (void)args;
    char pem_buf[2048];
    size_t out_len = 0;
    if (!gpg_export_pubkey_pem(pem_buf, sizeof(pem_buf), &out_len)) {
        cdc::serial::Console::printf("ERROR\r\n");
        return;
    }
    cdc::serial::Console::printf("%s\r\n", pem_buf);
}
```

Output format:
- PEM-encoded public key block
- Standard OpenPGP ASCII armor format

Example output:
```
-----BEGIN PGP PUBLIC KEY BLOCK-----

lQdGBF... (base64 encoded key data)
...
=abcd
-----END PGP PUBLIC KEY BLOCK-----
```

## Recommended Fix
Update `docs/SERIAL_COMMANDS.md` to include output format in the GPG Module section:

```markdown
**GPG_EXPORT Output:**
- PEM-encoded public key in OpenPGP ASCII armor format
- Suitable for importing into GnuPG or displaying as QR code

Example:
```
-----BEGIN PGP PUBLIC KEY BLOCK-----
...
-----END PGP PUBLIC KEY BLOCK-----
```
```

Or add an example:
```bash
# Export GPG public key
$ GPG_EXPORT
-----BEGIN PGP PUBLIC KEY BLOCK-----
...
-----END PGP PUBLIC KEY BLOCK-----
```

## References
- Implementation: `components/mod_gpg/src/GpgModule.cpp:187-196`
- Related doc: `docs/GPG.md` (Using with GnuPG section)
