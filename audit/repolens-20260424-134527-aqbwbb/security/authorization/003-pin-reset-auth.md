---
title: "[LOW] PIN_RESET command allows bypassing PIN lockout without authentication"
severity: LOW
domain: authorization
lens: pin-reset
labels:
  - "pin-reset"
  - "lockout-reset"
---

## Summary

The `PIN_RESET` serial command (defined in `components/serial_cmd/src/SerialCmd.cpp:863-868`) resets Badge PIN retries to the default value of 3, effectively bypassing the lockout mechanism. This command is registered as **not requiring authentication**:

```cpp
// Line 1469 in SerialCmd.cpp
reg.registerCommand({"PIN_RESET", "Reset PIN retries (debug)", cmd_pin_reset, "pin", false});
```

The `false` at the end means `requiresAuth = false`, so the command can be executed without prior authentication via `AUTH <pin>`.

The command implementation:
```cpp
static void cmd_pin_reset(const char* args) {
    (void)args;
    core::PinManager::instance().resetBadgeRetries();
    Console::printf("OK: Badge PIN retries reset to %d\r\n",
                    core::PinManager::instance().getBadgeRetries());
}
```

## Impact

1. **Lockout bypass** - After 3 failed PIN attempts, the device enters a 60-second lockout (or permanent lockout). An attacker can use `PIN_RESET` to immediately reset the counter and continue brute-forcing.

2. **No authentication required** - The command executes without checking if the user is authenticated, making it accessible to anyone with serial access.

3. **Facilitates brute-force attacks** - Without this command, an attacker would need to wait 60 seconds per 3 attempts. With `PIN_RESET`, they can try continuously.

## Evidence

**File: `components/serial_cmd/src/SerialCmd.cpp`**
- Lines 863-868: `cmd_pin_reset()` implementation
- Line 1469: Command registration with `requiresAuth = false`

**File: `components/cdc_core/include/cdc_core/PinManager.h`**
- Line 68: `resetBadgeRetries()` method declaration
- Lines 108-109: `MAX_RETRIES = 3` constant

**File: `components/serial_cmd/src/CommandRegistry.cpp`**
- Lines 128-138: When `FEATURE_SECURE_SERIAL` is enabled, commands require authentication unless they're `PING` or `AUTH`
- `PIN_RESET` is not in the allowed list, but since `requiresAuth=false`, it bypasses the check

## Recommended Fix

1. **Require authentication for PIN_RESET** by changing the registration:
   ```cpp
   reg.registerCommand({"PIN_RESET", "Reset PIN retries (debug)", cmd_pin_reset, "pin", true});
   ```

2. **Add a confirmation prompt** to prevent accidental use:
   ```cpp
   static void cmd_pin_reset(const char* args) {
       if (!args || strcmp(args, "CONFIRM") != 0) {
           Console::printf("WARNING: This bypasses PIN lockout!\r\n");
           Console::printf("To proceed, type: PIN_RESET CONFIRM\r\n");
           return;
       }
       core::PinManager::instance().resetBadgeRetries();
       Console::printf("OK: Badge PIN retries reset to %d\r\n",
                       core::PinManager::instance().getBadgeRetries());
   }
   ```

3. **Consider limiting PIN_RESET to DEBUG_MODE only**:
   ```cpp
   #if DEBUG_MODE
   reg.registerCommand({"PIN_RESET", "Reset PIN retries (debug)", cmd_pin_reset, "pin", true});
   #endif
   ```

## References

- [OWASP Authentication Cheat Sheet - Brute Force](https://cheatsheetseries.owasp.org/cheatsheets/Authentication_Cheat_Sheet.html#brute-force)
- [NIST SP 800-63B - Authenticator and Verifier Requirements](https://pages.nist.gov/800-63-3/sp800-63b.html#ver-impl)
