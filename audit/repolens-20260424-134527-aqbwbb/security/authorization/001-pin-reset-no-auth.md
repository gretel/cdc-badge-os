---
title: "[HIGH] PIN_RESET command accessible without authentication"
severity: HIGH
domain: Authorization & Access Control
lens: authorization
labels:
  - audit:security/authorization
---

## Summary
The `PIN_RESET` serial command is registered with `requiresAuth = false`, allowing any user with serial access to reset the badge PIN retry counter without authentication. This bypasses the lockout mechanism designed to prevent brute-force PIN attacks.

**Location:** `components/serial_cmd/src/SerialCmd.cpp:1468`

## Impact
- **Horizontal privilege escalation:** An attacker can exhaust PIN retries (3 attempts), then use `PIN_RESET` to restore retries without knowing the PIN
- **Brute-force protection bypass:** The 60-second lockout timer can be circumvented by simply resetting retries
- **Security control failure:** The PIN-based access control becomes ineffective as retry limits can be reset indefinitely

## Evidence
Command registration at `components/serial_cmd/src/SerialCmd.cpp:1468`:
```cpp
// PIN debug commands
reg.registerCommand({"PIN_STATUS", "Show PIN status", cmdPinStatus, "pin", false});
reg.registerCommand({"PIN_RESET", "Reset PIN retries (debug)", cmdPinReset, "pin", false});
```

Command handler at `components/serial_cmd/src/SerialCmd.cpp:870-874`:
```cpp
static void cmdPinReset(const char* args) {
    (void)args;
    core::PinManager::instance().resetBadgeRetries();
    Console::printf("OK: Badge PIN retries reset to %d\r\n",
                    core::PinManager::instance().getBadgeRetries());
}
```

Command structure definition at `components/serial_cmd/include/serial_cmd/ICommandRegistry.h:17-22`:
```cpp
struct Command {
    const char* name;           // Command name (e.g., "TOTP_LIST")
    const char* help;           // Help text
    CommandHandler handler;     // Handler function
    const char* moduleName;     // Module that registered this command
    bool requiresAuth;          // Requires authentication (when FEATURE_SECURE_SERIAL)
};
```

The `PIN_RESET` command is registered with `requiresAuth = false` (5th field), while destructive commands like `TR01_WIPE` correctly use `requiresAuth = true`.

## Recommended Fix
Change the `PIN_RESET` command registration to require authentication:

```cpp
// Change line 1468 from:
reg.registerCommand({"PIN_RESET", "Reset PIN retries (debug)", cmdPinReset, "pin", false});

// To:
reg.registerCommand({"PIN_RESET", "Reset PIN retries (debug)", cmdPinReset, "pin", true});
```

This ensures the command follows the same authorization pattern as other privileged commands:
- `TR01_WIPE` (factory reset) - `requiresAuth = true`
- `TR01_CLEANUP` (slot cleanup) - `requiresAuth = true`
- `TR01_ECC_DEL` (delete ECC keys) - `requiresAuth = true`
- `NVS_CLEAR` (erase NVS) - `requiresAuth = true`
- `REBOOT` (restart device) - `requiresAuth = true`

## References
- OWASP Authentication Cheat Sheet: https://cheatsheetseries.owasp.org/cheatsheets/Authentication_Cheat_Sheet.html
- OWASP Authorization Cheat Sheet: https://cheatsheetseries.owasp.org/cheatsheets/Authorization_Cheat_Sheet.html
- FIDO2 specification requires secure PIN verification: https://fidoalliance.org/specs/fido2/
