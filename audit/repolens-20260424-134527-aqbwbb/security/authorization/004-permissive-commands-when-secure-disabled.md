---
title: "[MEDIUM] Permissive command execution when FEATURE_SECURE_SERIAL is disabled"
severity: MEDIUM
domain: Authorization & Access Control
lens: authorization
labels:
  - audit:security/authorization
---

## Summary
When `FEATURE_SECURE_SERIAL` is disabled, the `requiresAuth` flag on commands is completely ignored. Commands like `REBOOT`, `TR01_WIPE`, `NVS_CLEAR`, and `TR01_CLEANUP` execute without any authentication check, providing full device control to anyone with serial access.

**Location:** `components/serial_cmd/src/CommandRegistry.cpp:140-159`

## Impact
- **Privilege escalation:** Any serial connection can execute privileged commands without PIN
- **Destructive operations exposed:** Factory reset, NVS clear, and ECC key deletion are accessible
- **No audit trail:** Unauthenticated command execution leaves no indication of access
- **Physical security dependency:** Security relies entirely on physical access control

## Evidence
Command execution logic at `components/serial_cmd/src/CommandRegistry.cpp:140-159`:
```cpp
// Find and execute command
for (size_t i = 0; i < count_; i++) {
    if (strcasecmp(commands_[i].name, cmdBuf) == 0) {
        // Per-command auth check (for commands that require auth even when
        // FEATURE_SECURE_SERIAL is disabled)
        if (commands_[i].requiresAuth && authCheck_ && !authCheck_()) {
            Console::printf("ERROR: Authentication required. Use AUTH <pin> first.\r\n");
            return true;  // Command found but not executed
        }
        if (commands_[i].handler) {
            commands_[i].handler(line);
        }
        // Signal successful command execution for timer reset
        if (onCommandExecuted_) {
            onCommandExecuted_();
        }
        return true;
    }
}
```

The per-command auth check on line 147 only executes when `authCheck_` is set. Looking at initialization in `SerialCmd.cpp:1202-1205`:
```cpp
#if FEATURE_SECURE_SERIAL
    getCommandRegistry().setAuthProvider(isAuthenticated);
    getCommandRegistry().setOnCommandExecuted(resetAuthTimer);
#endif
```

When `FEATURE_SECURE_SERIAL = 0`, `authCheck_` is never set (remains `nullptr`), so the condition `authCheck_ && !authCheck_()` short-circuits and the check is skipped entirely.

Commands marked with `requiresAuth = true` at `components/serial_cmd/src/SerialCmd.cpp:1440-1485`:
```cpp
reg.registerCommand({"REBOOT", "Restart the device", cmdReboot, "system", true});
reg.registerCommand({"NVS_DEL", "Delete NVS key/namespace", cmdNvsDel, "nvs", true});
reg.registerCommand({"NVS_CLEAR", "Erase entire NVS (NVS_CLEAR YES)", cmdNvsClear, "nvs", true});
reg.registerCommand({"TR01_ECC_DEL", "Delete ECC key slot", cmdTr01EccDel, "tr01", true});
reg.registerCommand({"TR01_RMEM_DEL", "Delete R-Memory slot", cmdTr01RmemDel, "tr01", true});
reg.registerCommand({"TR01_CLEANUP", "Cleanup mismatched slots + rebuild cache", cmdTr01Cleanup, "tr01", true});
reg.registerCommand({"TR01_WIPE", "Factory reset (TR01_WIPE CONFIRM)", cmdTr01Wipe, "tr01", true});
```

These commands check `requiresAuth` but the check is ineffective when `authCheck_` is `nullptr`.

## Recommended Fix
Ensure `authCheck_` is always initialized, even when `FEATURE_SECURE_SERIAL` is disabled. Modify `components/serial_cmd/src/SerialCmd.cpp:1200-1207`:

```cpp
// Current code:
#if FEATURE_SECURE_SERIAL
    getCommandRegistry().setAuthProvider(isAuthenticated);
    getCommandRegistry().setOnCommandExecuted(resetAuthTimer);
#endif

// Change to:
getCommandRegistry().setAuthProvider(isAuthenticated);
#if FEATURE_SECURE_SERIAL
    getCommandRegistry().setOnCommandExecuted(resetAuthTimer);
#endif
```

And update `isAuthenticated()` at `components/serial_cmd/src/SerialCmd.cpp:1346-1360` to always return a meaningful value:
```cpp
bool SerialCmd::isAuthenticated() {
#if FEATURE_SECURE_SERIAL
    if (!s_authenticated) return false;
    uint64_t now = esp_timer_get_time();
    if ((now - s_authTimestamp) > (AUTH_TIMEOUT_MS * 1000ULL)) {
        s_authenticated = false;
        LOG_I(TAG, "Session timed out");
        return false;
    }
    return true;
#else
    return true;  // When secure serial is disabled, all commands are allowed
#endif
}
```

This ensures the `requiresAuth` flag is always respected, providing consistent authorization behavior regardless of the `FEATURE_SECURE_SERIAL` setting.

## References
- OWASP Authorization Cheat Sheet: https://cheatsheetseries.owasp.org/cheatsheets/Authorization_Cheat_Sheet.html
- NIST SP 800-53 Rev. 5 AC-3: Access Enforcement
- Principle of least privilege: https://en.wikipedia.org/wiki/Principle_of_least_privilege
