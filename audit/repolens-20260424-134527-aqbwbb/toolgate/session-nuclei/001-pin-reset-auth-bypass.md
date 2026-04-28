---
title: "[MEDIUM] PIN_RESET serial command accessible without authentication"
severity: MEDIUM
domain: cdc-badge-os
lens: library/cdc-badge-os
labels:
  - "audit:toolgate/session-nuclei"
---

## Summary
The `PIN_RESET` serial command at `components/serial_cmd/src/SerialCmd.cpp:1478` resets badge PIN retry counters without requiring authentication. When `FEATURE_SECURE_SERIAL` is disabled (default), this allows an attacker with serial access to bypass PIN lockout by repeatedly calling `PIN_RESET`.

**Location**: `components/serial_cmd/src/SerialCmd.cpp:865-871` (handler), `components/serial_cmd/src/SerialCmd.cpp:1478` (registration)

## Impact
- **Security Bypass**: An attacker can reset PIN retries after exhausting the 3 attempts, effectively disabling the lockout mechanism
- **Brute Force Enablement**: Allows unlimited PIN guessing attempts by resetting retries between tries
- **Serial Access Required**: Physical or serial connection needed (115200 baud)

## Evidence
```cpp
// components/serial_cmd/src/SerialCmd.cpp:865-871
static void cmdPinReset(const char* args) {
    (void)args;
    core::PinManager::instance().resetBadgeRetries();
    Console::printf("OK: Badge PIN retries reset to %d\r\n",
                    core::PinManager::instance().getBadgeRetries());
}

// components/serial_cmd/src/SerialCmd.cpp:1478
reg.registerCommand({"PIN_RESET", "Reset PIN retries (debug)", cmdPinReset, "pin", false});
```

The last `false` parameter means `requiresAuth = false`. When `FEATURE_SECURE_SERIAL` is disabled, this command executes without any authentication check.

## Recommended Fix
Add authentication requirement to the `PIN_RESET` command:

```cpp
// Change registration from:
reg.registerCommand({"PIN_RESET", "Reset PIN retries (debug)", cmdPinReset, "pin", false});

// To:
reg.registerCommand({"PIN_RESET", "Reset PIN retries (debug)", cmdPinReset, "pin", true});
```

Alternatively, add an explicit auth check in the handler:
```cpp
static void cmdPinReset(const char* args) {
    (void)args;
#if FEATURE_SECURE_SERIAL
    if (!SerialCmd::isAuthenticated()) {
        Console::printf("ERROR: Authentication required\r\n");
        return;
    }
#endif
    core::PinManager::instance().resetBadgeRetries();
    Console::printf("OK: Badge PIN retries reset to %d\r\n",
                    core::PinManager::instance().getBadgeRetries());
}
```

## References
- `components/serial_cmd/src/SerialCmd.cpp:1478` - Command registration
- `components/serial_cmd/src/SerialCmd.cpp:865` - Command handler
- `components/cdc_core/include/cdc_core/feature_flags.h` - Feature flag definitions
