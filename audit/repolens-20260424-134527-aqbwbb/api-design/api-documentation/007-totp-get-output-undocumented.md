---
title: "[LOW] TOTP_GET command documentation missing output format"
severity: LOW
domain: api-design/api-documentation
lens: serial-commands
labels:
  - "audit:api-design/api-documentation"
---

## Summary
The `TOTP_GET` command documentation does not describe the output format or fields returned by the command.

**File:** `docs/SERIAL_COMMANDS.md` (line 85)

## Impact
Users do not know what format to expect when retrieving TOTP codes, making it harder to parse the output programmatically or understand the timing information provided.

## Evidence
**Documentation (SERIAL_COMMANDS.md):**
```markdown
| `TOTP_GET <index>` | Generate TOTP code by index `[AUTH]` |
```

**Implementation (components/mod_totp/src/TotpModule.cpp:284-307):**
```cpp
static void cmd_totp_get(const char* args) {
    // ...
    char code[9] = {};
    int8_t remaining = TotpStore::instance().generateCode(slot, code);
    // ...
    if (issuer && issuer[0]) {
        cdc::serial::Console::printf("%s (%ds) [%s]\r\n", code, remaining, issuer);
    } else {
        cdc::serial::Console::printf("%s (%ds)\r\n", code, remaining);
    }
}
```

Output formats:
- With issuer: `<code> (<seconds>) [<issuer>]`
- Without issuer: `<code> (<seconds>)`

Example outputs:
```
847293 (18s) [GitHub]
847293 (18s)
```

## Recommended Fix
Update `docs/SERIAL_COMMANDS.md` to include output format in the TOTP section:

```markdown
**TOTP_GET Output:**
- With issuer: `<code> (<seconds remaining>) [<issuer>]`
- Without issuer: `<code> (<seconds remaining>)`
```

Or add an example:
```bash
# Generate TOTP code
$ TOTP_GET 0
847293 (18s) [GitHub]
```

## References
- Implementation: `components/mod_totp/src/TotpModule.cpp:284-307`
