---
title: "[LOW] PASSWORD_ADD command lacks clear usage example"
severity: LOW
domain: api-design/api-documentation
lens: serial-command-api
labels:
  - "audit:api-design/api-documentation"
---

## Summary
The `docs/SERIAL_COMMANDS.md` lists the `PASSWORD_ADD` command but does not provide a clear usage example showing the parameter format. The documentation just shows:

```markdown
| Command | Description |
|---------|-------------|
| `PASSWORD_ADD <name> <user> <url> <password>` | Add password entry `[AUTH]` |
```

No examples are provided in the Examples section or anywhere else.

## Impact
- Users must guess the exact parameter order and format
- Unclear how to handle special characters (spaces, quotes) in fields
- Unclear what to do if a field is optional (e.g., URL or notes)
- Makes the password vault feature less discoverable and usable

## Evidence
**Documentation (docs/SERIAL_COMMANDS.md:100-108):**
```markdown
## Password Module

| Command | Description |
|---------|-------------|
| `PASSWORD_LIST` | List password entries `[AUTH]` |
| `PASSWORD_GET <index>` | Get password entry details `[AUTH]` |
| `PASSWORD_ADD <name> <user> <url> <password>` | Add password entry `[AUTH]` |
| `PASSWORD_DEL <index>` | Delete password entry `[AUTH]` |
```

**Missing:**
- No examples in the Examples section (docs/SERIAL_COMMANDS.md:118-134)
- No parameter format explanation
- No guidance on escaping special characters

**Implementation (components/mod_password/src/PasswordModule.cpp:248-269):**
From the code, the actual signature is:
```
PASSWORD_ADD <title> <username|- > <password> <url|- > [totpSlot] [notes]
```

This is more complex than the documentation suggests.

## Recommended Fix
Add a Password Module section with examples to `docs/SERIAL_COMMANDS.md`:

```markdown
## Password Module

| Command | Description |
|---------|-------------|
| `PASSWORD_LIST` | List password entries `[AUTH]` |
| `PASSWORD_GET <index>` | Get password entry details `[AUTH]` |
| `PASSWORD_ADD <name> <user> <url> <password>` | Add password entry `[AUTH]` |
| `PASSWORD_DEL <index>` | Delete password entry `[AUTH]` |

**PASSWORD_ADD Parameters:**
- `name` - Entry name/title (required)
- `user` - Username (use `-` for none)
- `password` - Password (required)
- `url` - Website URL (use `-` for none)
- `totpSlot` - Optional TOTP slot number
- `notes` - Optional notes field

**Examples:**
```bash
# Add simple password
echo "PASSWORD_ADD GitHub jdoe MySecret123 https://github.com" > /dev/ttyACM0

# Add with no username
echo "PASSWORD_ADD WiFi Guest - Wifipass" > /dev/ttyACM0

# List all entries
echo "PASSWORD_LIST" > /dev/ttyACM0

# Get details of entry 0
echo "PASSWORD_GET 0" > /dev/ttyACM0
```
```

## References
- docs/SERIAL_COMMANDS.md:100-108 (current password docs)
- components/mod_password/src/PasswordModule.cpp:248-269 (actual command handler)
