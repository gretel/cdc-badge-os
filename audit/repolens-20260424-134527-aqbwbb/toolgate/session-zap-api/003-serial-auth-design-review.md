---
title: "[LOW] Serial command authentication requires manual review for consistency"
severity: LOW
domain: API Security
lens: session-zap-api
labels:
  - "audit:toolgate/session-zap-api"
---

## Summary
The serial command authentication system uses a mix of global authentication checks and per-command flags. While functional, this dual-layer approach requires careful manual review to ensure all sensitive commands have appropriate protection. Located in `components/serial_cmd/`.

## Impact
- **Inconsistency Risk**: Commands may be added without proper authentication flags
- **Feature Flag Dependency**: When `FEATURE_SECURE_SERIAL=0`, global auth is bypassed, relying solely on per-command `requiresAuth` flags
- **Maintenance Burden**: Developers must remember to set both global and per-command auth correctly

## Evidence
File: `components/serial_cmd/src/CommandRegistry.cpp`
Lines: 114-158

Authentication logic has two layers:

**Layer 1 (Global, when FEATURE_SECURE_SERIAL):**
```cpp
#if FEATURE_SECURE_SERIAL
    // Block ALL commands except PING and AUTH when not authenticated
    bool isAllowedWithoutAuth = (strcasecmp(cmdBuf, "PING") == 0 ||
                                  strcasecmp(cmdBuf, "AUTH") == 0);
    if (!isAllowedWithoutAuth && authCheck_ && !authCheck_()) {
        Console::printf("ERROR: Not authenticated.\r\n");
        return true;
    }
#endif
```

**Layer 2 (Per-command):**
```cpp
if (commands_[i].requiresAuth && authCheck_ && !authCheck_()) {
    Console::printf("ERROR: Authentication required.\r\n");
    return true;
}
```

Registration example (lines 1442-1468):
```cpp
reg.registerCommand({"HELP", "...", cmdHelp, "system", false});  // No auth
reg.registerCommand({"REBOOT", "...", cmdReboot, "system", true});  // Auth needed
reg.registerCommand({"PIN_RESET", "...", cmdPinReset, "pin", false});  // No auth - review needed!
```

When `FEATURE_SECURE_SERIAL=0`, only the per-command `requiresAuth` flag provides protection.

## Recommended Fix
1. **Audit all commands**: Review all 30+ registered commands and verify `requiresAuth` flag is set correctly:
   - System status commands (HELP, PING, STATUS, MEM) → `false`
   - Data read commands (NVS_READ, TR01_RMEM_READ) → `true`
   - Data write/delete commands (NVS_DEL, TR01_ECC_DEL) → `true`
   - Debug commands (PIN_RESET) → `true` or guarded by DEBUG_MODE

2. **Add compile-time guard for debug commands**:
   ```cpp
   #if DEBUG_MODE
   reg.registerCommand({"PIN_RESET", "...", cmdPinReset, "pin", false});
   #endif
   ```

3. **Document authentication policy**: Add comments in the registration section clarifying which commands need auth and why.

4. **Consider centralized configuration**: Use a table-driven approach for command registration to reduce errors.

## References
- OWASP API Security Top 10: **API2:2023 Broken Authentication**
- OWASP Cheat Sheet: Authentication
- CWE-287: Improper Authentication
- OWASP Cheat Sheet: Defense in Depth
