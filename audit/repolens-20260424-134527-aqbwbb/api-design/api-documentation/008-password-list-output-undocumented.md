---
title: "[LOW] PASSWORD_LIST command documentation missing output format"
severity: LOW
domain: api-design/api-documentation
lens: serial-commands
labels:
  - "audit:api-design/api-documentation"
---

## Summary
The `PASSWORD_LIST` command documentation does not describe the output format or fields returned by the command.

**File:** `docs/SERIAL_COMMANDS.md` (line 98)

## Impact
Users do not know what format to expect when listing password entries, making it harder to parse the output programmatically.

## Evidence
**Documentation (SERIAL_COMMANDS.md):**
```markdown
| `PASSWORD_LIST` | List password entries `[AUTH]` |
```

**Implementation (components/mod_password/src/PasswordModule.cpp:184-212):**
```cpp
static void cmd_password_list(const char* args) {
    // ...
    for (uint16_t i = 0; i < count; i++) {
        cdc::serial::Console::printf("%u: %s (slot %u)\r\n", i, list[i].title, list[i].slot);
    }
}
```

Output format:
- `<index>: <title> (slot <slot_number>)`
- `(no entries)` when empty

Example outputs:
```
0: GitHub (slot 150)
1: AWS Console (slot 151)
2: Google (slot 152)
```

## Recommended Fix
Update `docs/SERIAL_COMMANDS.md` to include output format in the Password Module section:

```markdown
**PASSWORD_LIST Output:**
```
<index>: <title> (slot <slot_number>)
```

Or add an example:
```bash
# List password entries
$ PASSWORD_LIST
0: GitHub (slot 150)
1: AWS Console (slot 151)
2: Google (slot 152)
```

## References
- Implementation: `components/mod_password/src/PasswordModule.cpp:184-212`
