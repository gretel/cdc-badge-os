---
title: "[MEDIUM] GPG cross-signing commands documented but not implemented in code"
severity: MEDIUM
domain: api-design/api-documentation
lens: serial-command-api
labels:
  - "audit:api-design/api-documentation"
---

## Summary
The GPG module documentation (`docs/GPG.md` lines 84-91) lists 4 cross-signing commands that do not exist in the actual implementation:

**Documented commands (docs/GPG.md:84-91):**
- `GPG_RECV_LIST` - List received cross-signing keys
- `GPG_RECV_INFO <index>` - Show received key details
- `GPG_CROSS_SIGN <index>` - Sign a received key
- `GPG_RECV_DELETE <index>` - Delete a received key

**Actual implementation (components/mod_gpg/src/GpgModule.cpp:464-472):**
Only 4 commands are registered:
- `GPG_STATUS`
- `GPG_GENERATE`
- `GPG_EXPORT`
- `GPG_RESET`

## Impact
- Users following the documentation will encounter "Unknown command" errors when trying to use cross-signing features
- The cross-signing workflow described in the docs is incomplete without these commands
- Developers may implement features assuming the commands exist, leading to confusion
- The feature may be partially implemented (UI exists) but missing serial command interface

## Evidence
**Documentation (docs/GPG.md:84-91):**
```markdown
| Command | Description |
|---------|-------------|
| `GPG_STATUS` | Show key status (User-ID, fingerprints, curves) |
| `GPG_GENERATE <curve> <user_id>` | Generate keys (1=Ed25519, 2=P-256) |
| `GPG_EXPORT` | Export public keys as PEM |
| `GPG_RESET` | Delete all keys (requires CONFIRM) |
| `GPG_RECV_LIST` | List received cross-signing keys |
| `GPG_RECV_INFO <index>` | Show received key details |
| `GPG_CROSS_SIGN <index>` | Sign a received key |
| `GPG_RECV_DELETE <index>` | Delete a received key |
```

**Implementation (components/mod_gpg/src/GpgModule.cpp:464-472):**
```cpp
static void registerCommands() {
    if (s_commandsRegistered) return;
    s_commandsRegistered = true;
    auto& registry = cdc::serial::getCommandRegistry();
    registry.registerCommand({"GPG_STATUS", "Show GPG status", cmd_gpg_status, CMD_MODULE, true});
    registry.registerCommand({"GPG_GENERATE", "Generate GPG keys", cmd_gpg_generate, CMD_MODULE, true});
    registry.registerCommand({"GPG_EXPORT", "Export public keys", cmd_gpg_export, CMD_MODULE, true});
    registry.registerCommand({"GPG_RESET", "Reset GPG keys", cmd_gpg_reset, CMD_MODULE, true});
}
```

## Recommended Fix
Choose one approach:

**Option A: Implement the missing commands**
1. Add `GPG_RECV_LIST`, `GPG_RECV_INFO`, `GPG_CROSS_SIGN`, `GPG_RECV_DELETE` handlers in `GpgModule.cpp`
2. Register them with the command registry
3. Update `docs/GPG.md` to reflect the actual parameter syntax

**Option B: Remove undocumented commands from documentation**
1. Update `docs/GPG.md` to only list the 4 implemented commands
2. If cross-signing is a future feature, add a "Planned Features" section noting it's under development

## References
- docs/GPG.md:84-91 (documented commands)
- components/mod_gpg/src/GpgModule.cpp:464-472 (actual registration)
- components/mod_gpg/include/mod_gpg/GpgModule.h (module interface)
