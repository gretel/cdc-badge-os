---
title: "[LOW] PIN status commands expose retry counts aiding brute force"
severity: LOW
domain: cdc-badge-os
lens: library/cdc-badge-os
labels:
  - "audit:toolgate/session-nuclei"
---

## Summary
The `PIN_STATUS` command (`components/serial_cmd/src/SerialCmd.cpp:1477`) exposes the current PIN retry count without authentication. This allows an attacker to:
1. Know exactly how many attempts remain
2. Confirm when lockout is active
3. Time brute force attempts more effectively

**Location**: `components/serial_cmd/src/SerialCmd.cpp:880-886` (handler), `components/serial_cmd/src/SerialCmd.cpp:1477` (registration)

## Impact
- **Information Leakage**: Attackers know exact retry count (3, 2, 1, or 0)
- **Brute Force Optimization**: Can pause between attempts knowing lockout status
- **Serial Access Required**: Physical or serial connection needed

## Evidence
```cpp
// components/serial_cmd/src/SerialCmd.cpp:880-886
static void cmdPinStatus(const char* args) {
    (void)args;
    auto& pm = core::PinManager::instance();
    Console::printf("Badge PIN: retries=%d blocked=%s set=%s\r\n",
                    pm.getBadgeRetries(),
                    pm.isBadgeBlocked() ? "yes" : "no",
                    pm.isPinSet() ? "yes" : "no");
}

// components/serial_cmd/src/SerialCmd.cpp:1477
reg.registerCommand({"PIN_STATUS", "Show PIN status", cmdPinStatus, "pin", false});
```

The `false` parameter means no authentication required.

## Recommended Fix
Require authentication for PIN status:

```cpp
// Change registration from:
reg.registerCommand({"PIN_STATUS", "Show PIN status", cmdPinStatus, "pin", false});

// To:
reg.registerCommand({"PIN_STATUS", "Show PIN status", cmdPinStatus, "pin", true});
```

Alternatively, hide retry count when not authenticated:
```cpp
static void cmdPinStatus(const char* args) {
    (void)args;
    auto& pm = core::PinManager::instance();
#if FEATURE_SECURE_SERIAL
    if (!SerialCmd::isAuthenticated()) {
        Console::printf("Badge PIN: retries=? blocked=? set=%s\r\n",
                        pm.isPinSet() ? "yes" : "no");
        return;
    }
#endif
    Console::printf("Badge PIN: retries=%d blocked=%s set=%s\r\n",
                    pm.getBadgeRetries(),
                    pm.isBadgeBlocked() ? "yes" : "no",
                    pm.isPinSet() ? "yes" : "no");
}
```

## References
- `components/serial_cmd/src/SerialCmd.cpp:880-886` - Command handler
- `components/serial_cmd/src/SerialCmd.cpp:1477` - Command registration
- Information leakage patterns in authentication systems
