---
title: "[MEDIUM] TOTP_ADD command documentation missing algorithm parameter"
severity: MEDIUM
domain: api-design/api-documentation
lens: serial-commands
labels:
  - "audit:api-design/api-documentation"
---

## Summary
The `TOTP_ADD` serial command documentation in `docs/SERIAL_COMMANDS.md` does not include the `algorithm` parameter, but the implementation supports it.

**File:** `docs/SERIAL_COMMANDS.md` (lines 76-83)

## Impact
Users following the documentation cannot discover that they can specify SHA256 or SHA512 algorithms instead of the default SHA1. This limits the functionality available to users and may cause compatibility issues with TOTP accounts that require non-SHA1 algorithms.

## Evidence
**Documentation (SERIAL_COMMANDS.md):**
```markdown
| `TOTP_ADD <name> <secret> [issuer] [digits] [period]` | Add TOTP account `[AUTH]` |

**TOTP_ADD Parameters:**
- `name` - Account name (required)
- `secret` - Base32 encoded secret (required)
- `issuer` - Issuer name (optional)
- `digits` - Code length: 6, 7, or 8 (default: 6)
- `period` - Time period in seconds (default: 30)
```

**Implementation (components/mod_totp/src/TotpModule.cpp:225-262):**
```cpp
static void cmd_totp_add(const char* args) {
    // ...
    char algoBuf[8] = {};
    // ...
    p = nextToken(p, algoBuf, sizeof(algoBuf));
    uint8_t algo = parseAlgo(algoBuf);
    // ...
}
```

The `parseAlgo()` function (line 136-147) supports:
- `sha1`, `sha256`, `sha512` (text)
- Numeric values (1, 2, 3)

## Recommended Fix
Update `docs/SERIAL_COMMANDS.md` to include the algorithm parameter:

1. Update the command signature:
   ```markdown
   | `TOTP_ADD <name> <secret> [issuer] [digits] [period] [algo]` | Add TOTP account `[AUTH]` |
   ```

2. Add algorithm parameter description:
   ```markdown
   - `algo` - Algorithm: sha1, sha256, sha512, or 1/2/3 (default: sha1)
   ```

3. Add an example:
   ```bash
   # Add TOTP with SHA256 algorithm
   echo "TOTP_ADD Google JBSWY3DPEHPK3PXP Google sha256" > /dev/ttyACM0
   ```

## References
- Implementation: `components/mod_totp/src/TotpModule.cpp:136-147` (parseAlgo function)
- Implementation: `components/mod_totp/src/TotpModule.cpp:225-262` (cmd_totp_add function)
