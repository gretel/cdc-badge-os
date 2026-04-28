---
title: "[LOW] PIN Reset Command Bypasses Retry Counter Without Confirmation"
severity: LOW
domain: destructive-actions
lens: serial-commands
labels:
  - audit:ux-antipatterns/destructive-actions
---

## Summary
The serial command `PIN_RESET` in `components/serial_cmd/src/SerialCmd.cpp:870-875` executes immediate reset of badge PIN retries without any confirmation step. This command clears the retry counter and unblocks the badge, which can be useful for recovery but lacks friction for the operation.

**Evidence:**
- File: `components/serial_cmd/src/SerialCmd.cpp`
- Lines: 870-875
- Function: `cmdPinReset(const char* args)`

```cpp
static void cmdPinReset(const char* args) {
    (void)args;
    core::PinManager::instance().resetBadgeRetries();
    Console::printf("OK: Badge PIN retries reset to %d\r\n",
                    core::PinManager::instance().getBadgeRetries());
}
```

Registered as:
```cpp
registry.registerCommand({"PIN_RESET", "Reset PIN retries (debug)", cmdPinReset, "pin", false});
```

Note: The command is registered with `debug = false`, meaning it's available in normal operation (not just debug mode).

## Impact
- **Security Bypass Risk**: Resetting PIN retries allows users to bypass the lockout mechanism after failed PIN attempts
- **Brute Force Protection**: The retry counter is a key security feature; resetting it without confirmation allows unlimited PIN guessing attempts
- **Moderate Impact**: Unlike data deletion, this doesn't lose data but weakens security posture
- **Debug vs Production**: The command is marked as "debug" in its description but is not restricted to debug mode

## Recommended Fix
Add a confirmation step and consider restricting to debug mode only:

```cpp
static void cmdPinReset(const char* args) {
    (void)args;
    
    // Check for confirmation
    if (!args || strcmp(args, "CONFIRM") != 0) {
        Console::printf("WARNING: Reset PIN retry counter?\r\n");
        Console::printf("  This will unblock the badge and reset retries.\r\n");
        Console::printf("  To proceed, type: PIN_RESET CONFIRM\r\n");
        return;
    }
    
    core::PinManager::instance().resetBadgeRetries();
    Console::printf("OK: Badge PIN retries reset to %d\r\n",
                    core::PinManager::instance().getBadgeRetries());
}
```

Alternatively, restrict to debug mode by changing the registration:
```cpp
registry.registerCommand({"PIN_RESET", "Reset PIN retries (debug)", cmdPinReset, "pin", true});
```

## References
- PIN manager: `components/cdc_core/include/cdc_core/PinManager.h`
- Similar pattern: `TR01_WIPE` command uses `CONFIRM` argument pattern
