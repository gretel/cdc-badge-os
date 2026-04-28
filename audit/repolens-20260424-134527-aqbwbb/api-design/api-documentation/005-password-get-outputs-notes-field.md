---
title: "[LOW] PASSWORD_GET documentation missing output fields"
severity: LOW
domain: api-design/api-documentation
lens: serial-commands
labels:
  - "audit:api-design/api-documentation"
---

## Summary
The `PASSWORD_GET` command documentation does not describe the output format or all fields returned by the command.

**File:** `docs/SERIAL_COMMANDS.md` (line 95)

## Impact
Users do not know what information they will receive when using `PASSWORD_GET`, making it harder to parse the output programmatically or understand the full capabilities of the command.

## Evidence
**Documentation (SERIAL_COMMANDS.md):**
```markdown
| `PASSWORD_GET <index>` | Get password entry details `[AUTH]` |
```

**Implementation (components/mod_password/src/PasswordModule.cpp:284-305):**
```cpp
static void cmd_password_get(const char* args) {
    // ...
    cdc::serial::Console::printf("Title: %s\r\n", entry.title);
    cdc::serial::Console::printf("Username: %s\r\n", entry.username);
    cdc::serial::Console::printf("Password: %s\r\n", entry.password);
    cdc::serial::Console::printf("URL: %s\r\n", entry.url);
    if (entry.totpSlot == PasswordStore::TOTP_SLOT_NONE) {
        cdc::serial::Console::printf("TOTP Slot: none\r\n");
    } else {
        cdc::serial::Console::printf("TOTP Slot: %u\r\n", entry.totpSlot);
    }
    cdc::serial::Console::printf("Notes: %s\r\n", entry.notes);
}
```

Output fields:
- `Title`
- `Username`
- `Password`
- `URL`
- `TOTP Slot` (shows "none" or slot number)
- `Notes`

## Recommended Fix
Update `docs/SERIAL_COMMANDS.md` to include output format:

```markdown
**PASSWORD_GET Output:**
```
Title: <title>
Username: <username>
Password: <password>
URL: <url>
TOTP Slot: <slot> or "none"
Notes: <notes>
```
```

Or add an example:
```bash
# Get password entry details
$ PASSWORD_GET 0
Title: GitHub
Username: john_doe
Password: secret123
URL: https://github.com
TOTP Slot: 5
Notes: Primary account
```

## References
- Implementation: `components/mod_password/src/PasswordModule.cpp:284-305`
