---
title: "[HIGH] PASSWORD_ADD documentation parameters do not match implementation"
severity: HIGH
domain: api-design/api-documentation
lens: serial-commands
labels:
  - "audit:api-design/api-documentation"
---

## Summary
The `PASSWORD_ADD` command documentation shows incorrect parameter order and missing parameters compared to the actual implementation.

**File:** `docs/SERIAL_COMMANDS.md` (line 96)

## Impact
Users following the documented syntax will experience command failures because:
1. Parameter order is different (documented: name, user, url, password; actual: title, username, password, url)
2. Missing optional parameters (totpSlot, notes)
3. Missing support for optional "-" placeholder syntax

## Evidence
**Documentation (SERIAL_COMMANDS.md):**
```markdown
| `PASSWORD_ADD <name> <user> <url> <password>` | Add password entry `[AUTH]` |
```

**Implementation (components/mod_password/src/PasswordModule.cpp:310-325):**
```cpp
static void cmd_password_add(const char* args) {
    // ...
    const char* p = nextToken(args, title, sizeof(title));
    if (!p || !title[0]) {
        cdc::serial::Console::printf("Usage: PASSWORD_ADD <title> <username|- > <password> <url|- > [totpSlot] [notes]\r\n");
        return;
    }
    p = nextToken(p, username, sizeof(username));
    p = nextToken(p, password, sizeof(password));
    p = nextToken(p, url, sizeof(url));
    // ...
    p = nextToken(p, totpBuf, sizeof(totpBuf));
    const char* notes = skipSpaces(p);
    // ...
}
```

**Actual command signature:**
```
PASSWORD_ADD <title> <username|- > <password> <url|- > [totpSlot] [notes]
```

Key differences:
1. **Parameter order mismatch:** Documentation shows `user, url, password` but implementation uses `username, password, url`
2. **Missing optional parameters:** `totpSlot` and `notes` are not documented
3. **Missing placeholder syntax:** Documentation doesn't show how to skip optional username/url using `-`

## Recommended Fix
Update `docs/SERIAL_COMMANDS.md` to accurately reflect the implementation:

1. Update the command signature:
   ```markdown
   | `PASSWORD_ADD <title> <username> <password> <url> [totpSlot] [notes]` | Add password entry `[AUTH]` |
   ```

2. Add parameter details section:
   ```markdown
   **PASSWORD_ADD Parameters:**
   - `title` - Entry title (required)
   - `username` - Username (use `-` for none)
   - `password` - Password (required)
   - `url` - URL (use `-` for none)
   - `totpSlot` - Associated TOTP slot number (optional)
   - `notes` - Notes field (optional, rest of line)
   ```

3. Add examples:
   ```bash
   # Add password with all fields
   echo "PASSWORD_ADD GitHub github_user secret123 https://github.com 5" > /dev/ttyACM0

   # Add password without username and URL
   echo "PASSWORD_ADD LocalApp - secret456 -" > /dev/ttyACM0

   # Add password with notes
   echo "PASSWORD_APP MySite user123 pass789 https://mysite.com 3 Login before noon" > /dev/ttyACM0
   ```

## References
- Implementation: `components/mod_password/src/PasswordModule.cpp:310-350`
- Usage output: `components/mod_password/src/PasswordModule.cpp:321`
