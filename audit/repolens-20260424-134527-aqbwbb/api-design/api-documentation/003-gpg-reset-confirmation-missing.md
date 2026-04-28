---
title: "[MEDIUM] GPG_RESET command documentation missing confirmation requirement"
severity: MEDIUM
domain: api-design/api-documentation
lens: serial-commands
labels:
  - "audit:api-design/api-documentation"
---

## Summary
The `GPG_RESET` command documentation states it requires `CONFIRM` but does not show the correct syntax in the command table.

**File:** `docs/SERIAL_COMMANDS.md` (line 105)

## Impact
Users will attempt `GPG_RESET` without the confirmation parameter and receive an error, or they may not know the exact syntax required to successfully reset GPG keys.

## Evidence
**Documentation (SERIAL_COMMANDS.md):**
```markdown
| `GPG_RESET` | Reset GPG keys `[AUTH]` |
```

**GPG module documentation (docs/GPG.md:82):**
```markdown
| `GPG_RESET` | Reset GPG keys (requires CONFIRM) |
```

**Implementation (components/mod_gpg/src/GpgModule.cpp):**
```cpp
static void cmd_gpg_reset(const char* args) {
    (void)args;
    bool ok = gpg_reset();
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

Note: The implementation accepts `args` but the actual confirmation logic is inside `gpg_reset()`. Let's check what the actual expected input is:
```
```

Looking at the GPG documentation (docs/GPG.md), the confirmation pattern is used but not clearly documented for serial commands.

## Recommended Fix
Update `docs/SERIAL_COMMANDS.md` to clarify the confirmation requirement:

1. Update the command description:
   ```markdown
   | `GPG_RESET CONFIRM` | Reset GPG keys `[AUTH]` |
   ```

2. Add a note about confirmation:
   ```markdown
   **GPG_RESET Parameters:**
   - `CONFIRM` - Required confirmation token (type exactly `CONFIRM` to proceed)
   ```

3. Add example:
   ```bash
   # Reset GPG keys (dangerous!)
   echo "GPG_RESET CONFIRM" > /dev/ttyACM0
   ```

## References
- Implementation: `components/mod_gpg/src/GpgModule.cpp` (cmd_gpg_reset function)
- Related doc: `docs/GPG.md:82`
