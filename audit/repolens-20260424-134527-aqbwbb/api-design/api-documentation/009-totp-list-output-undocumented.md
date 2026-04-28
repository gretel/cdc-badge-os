---
title: "[LOW] TOTP_LIST command documentation missing output format"
severity: LOW
domain: api-design/api-documentation
lens: serial-commands
labels:
  - "audit:api-design/api-documentation"
---

## Summary
The `TOTP_LIST` command documentation does not describe the output format or fields returned by the command.

**File:** `docs/SERIAL_COMMANDS.md` (line 82)

## Impact
Users do not know what format to expect when listing TOTP accounts, making it harder to parse the output programmatically.

## Evidence
**Documentation (SERIAL_COMMANDS.md):**
```markdown
| `TOTP_LIST` | List all TOTP accounts `[AUTH]` |
```

**Implementation (components/mod_totp/src/TotpModule.cpp:194-222):**
```cpp
static void cmd_totp_list(const char* args) {
    // ...
    auto cb = [](uint16_t slot, const cdc::core::TropicStorage::CacheEntry& entry, void* user) {
        // ...
        cdc::serial::Console::printf("%u: %s (slot %u)\r\n", c->idx, entry.name, logical);
        // ...
    };
    // ...
    if (ctx.idx == 0) {
        cdc::serial::Console::printf("(no entries)\r\n");
    }
}
```

Output format:
- `<index>: <name> (slot <slot_number>)`
- `(no entries)` when empty

Example outputs:
```
0: GitHub (slot 1)
1: Google (slot 2)
2: AWS (slot 3)
```

## Recommended Fix
Update `docs/SERIAL_COMMANDS.md` to include output format in the TOTP Module section:

```markdown
**TOTP_LIST Output:**
```
<index>: <name> (slot <slot_number>)
```

Or add an example:
```bash
# List TOTP accounts
$ TOTP_LIST
0: GitHub (slot 1)
1: Google (slot 2)
2: AWS (slot 3)
```

## References
- Implementation: `components/mod_totp/src/TotpModule.cpp:194-222`
