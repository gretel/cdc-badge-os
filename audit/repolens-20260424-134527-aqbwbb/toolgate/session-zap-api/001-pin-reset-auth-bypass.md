---
title: "[MEDIUM] PIN reset command lacks authentication protection"
severity: MEDIUM
domain: API Security
lens: session-zap-api
labels:
  - "audit:toolgate/session-zap-api"
---

## Summary
The `PIN_RESET` serial command resets the badge PIN retry counter without requiring authentication. Located in `components/serial_cmd/src/SerialCmd.cpp:870-875`, this command can be executed by anyone with physical serial access, potentially bypassing lockout protection.

## Impact
- **Authentication Bypass**: An attacker can reset the PIN retry counter after exhausting retries, effectively nullifying the lockout mechanism
- **Brute Force Enablement**: Without persistent lockout, an attacker can attempt unlimited PIN guesses by resetting between attempts
- **Physical Security**: For a hardware security key, serial access typically implies physical possession, but the command should still require authentication

## Evidence
File: `components/serial_cmd/src/SerialCmd.cpp`
Lines: 870-875

```cpp
/**
 * \brief Resets badge PIN retry counters for debugging.
 * \param args Unused command arguments.
 */
static void cmdPinReset(const char* args) {
    (void)args;
    core::PinManager::instance().resetBadgeRetries();
    Console::printf("OK: Badge PIN retries reset to %d\r\n",
                    core::PinManager::instance().getBadgeRetries());
}
```

Registration (line 1469):
```cpp
reg.registerCommand({"PIN_RESET", "Reset PIN retries (debug)", cmdPinReset, "pin", false});
```

The command is registered with `requiresAuth = false`, and `PIN_STATUS` (line 1468) is also unprotected.

## Recommended Fix
1. **Add authentication requirement** to the `PIN_RESET` command registration:
   ```cpp
   reg.registerCommand({"PIN_RESET", "Reset PIN retries (debug)", cmdPinReset, "pin", true});
   ```

2. **Consider limiting scope**: Since this is a debug command, consider:
   - Adding a `DEBUG_MODE` check to only allow when `DEBUG_MODE=1`
   - Adding a confirmation argument (e.g., `PIN_RESET CONFIRM`)
   - Moving it behind a compile-time flag

3. **Apply same protection** to `PIN_STATUS` if sensitive information is exposed

## References
- OWASP API Security Top 10: **API2:2023 Broken Authentication**
- CWE-287: Improper Authentication
- OWASP Cheat Sheet: Authentication
