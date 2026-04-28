---
title: "[CRITICAL] PIN_RESET serial command bypasses authentication"
severity: CRITICAL
domain: authorization
lens: serial-command-auth
labels:
  - "critical:pin-bypass"
---

## Summary
The `PIN_RESET` serial command allows resetting the badge PIN retry counter without any authentication. The command is registered with `requiresAuth = false`, enabling anyone with serial access to reset the lockout counter and brute-force the PIN.

**File:** `components/serial_cmd/src/SerialCmd.cpp`  
**Lines:** 1468, 870-877

## Evidence

**Command registration (line 1468):**
```cpp
reg.registerCommand({"PIN_RESET", "Reset PIN retries (debug)", cmdPinReset, "pin", false});
```

**Handler implementation (lines 870-875):**
```cpp
static void cmdPinReset(const char* args) {
    (void)args;
    core::PinManager::instance().resetBadgeRetries();
    Console::printf("OK: Badge PIN retries reset to %d\r\n",
                    core::PinManager::instance().getBadgeRetries());
}
```

The command accepts `false` for `requiresAuth`, meaning it does not check for an authenticated session.

## Impact
An attacker can:
1. Attempt to brute-force the PIN (3 attempts by default)
2. When locked out, call `PIN_RESET` to reset retries
3. Repeat indefinitely with unlimited attempts

This completely undermines the PIN lockout mechanism, allowing unlimited PIN guessing.

## Recommended Fix
Change the `requiresAuth` parameter from `false` to `true`:

```cpp
reg.registerCommand({"PIN_RESET", "Reset PIN retries (debug)", cmdPinReset, "pin", true});
```

This ensures the command requires an authenticated session (when `FEATURE_SECURE_SERIAL=1`) or can be restricted further to require explicit PIN verification.

For stronger security, consider adding explicit PIN verification in the handler:
```cpp
static void cmdPinReset(const char* args) {
    (void)args;
    auto& pm = core::PinManager::instance();
    
    // Require fresh PIN verification, not just session auth
    char pin[16];
    Console::printf("Enter PIN to reset retries: ");
    // Read PIN (silent input) and verify
    if (!pm.verifyBadgePin(pin)) {
        Console::printf("ERROR: PIN verification failed\r\n");
        return;
    }
    
    pm.resetBadgeRetries();
    Console::printf("OK: Badge PIN retries reset\r\n");
}
```

## References
- CWE-287: Improper Authentication
- CWE-307: Improper Restriction of Excessive Authentication Attempts
- [CDA Badge OS Serial Command Documentation](components/serial_cmd/README.md)
