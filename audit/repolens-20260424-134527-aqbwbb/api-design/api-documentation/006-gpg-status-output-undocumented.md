---
title: "[LOW] GPG_STATUS command documentation missing output format"
severity: LOW
domain: api-design/api-documentation
lens: serial-commands
labels:
  - "audit:api-design/api-documentation"
---

## Summary
The `GPG_STATUS` command documentation does not describe the output format or fields returned by the command.

**File:** `docs/SERIAL_COMMANDS.md` (line 108)

## Impact
Users do not know what information they will receive when using `GPG_STATUS`, making it harder to parse the output programmatically or understand the full capabilities of the command.

## Evidence
**Documentation (SERIAL_COMMANDS.md):**
```markdown
| `GPG_STATUS` | Show GPG key status `[AUTH]` |
```

**Implementation (components/mod_gpg/src/GpgModule.cpp:131-143):**
```cpp
static void cmd_gpg_status(const char* args) {
    (void)args;
    gpg_status_t status = {};
    if (!gpg_get_status(&status)) {
        cdc::serial::Console::printf("ERROR: No key configured\r\n");
        return;
    }
    cdc::serial::Console::printf("User-ID: %s\r\n", status.user_id);
    cdc::serial::Console::printf("Curve: %s\r\n",
                                 status.curve == CDC_CURVE_ED25519 ? "Ed25519" : "P-256");
    cdc::serial::Console::printf("Created: %lu\r\n", static_cast<unsigned long>(status.created_at));
    cdc::serial::Console::printf("Sign Count: %lu\r\n", static_cast<unsigned long>(status.sign_count));
}
```

Output fields:
- `User-ID` - The GPG user ID (name and optional email)
- `Curve` - Key curve (Ed25519 or P-256)
- `Created` - Unix timestamp of key creation
- `Sign Count` - Number of signatures created

## Recommended Fix
Update `docs/SERIAL_COMMANDS.md` to include output format in the GPG section:

```markdown
**GPG_STATUS Output:**
```
User-ID: <name> [<email>]
Curve: Ed25519 or P-256
Created: <unix_timestamp>
Sign Count: <count>
```
```

Or add an example:
```bash
# Show GPG key status
$ GPG_STATUS
User-ID: Max Mustermann <max@example.com>
Curve: Ed25519
Created: 1704067200
Sign Count: 42
```

## References
- Implementation: `components/mod_gpg/src/GpgModule.cpp:131-143`
- Related doc: `docs/GPG.md`
