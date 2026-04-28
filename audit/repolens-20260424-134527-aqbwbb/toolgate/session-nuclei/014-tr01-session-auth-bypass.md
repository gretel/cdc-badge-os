---
title: "[MEDIUM] TR01_SESSION command allows restarting secure element session without authentication"
severity: MEDIUM
domain: cdc-badge-os
lens: library/cdc-badge-os
labels:
  - "audit:toolgate/session-nuclei"
---

## Summary
The `TR01_SESSION` serial command allows restarting the TROPIC01 secure element session without authentication. When `FEATURE_SECURE_SERIAL` is disabled (default), any user with serial access can trigger session restarts, which could be used to refresh session state, potentially recover from session timeouts, or disrupt ongoing operations.

**Location**: `components/serial_cmd/src/SerialCmd.cpp:1473` (registration), `components/serial_cmd/src/SerialCmd.cpp:943-958` (handler)

## Impact
- **Session State Control**: Attacker can force session restarts without authentication
- **Disruption Potential**: Could interrupt ongoing secure element operations
- **Recovery Mechanism**: May allow attacker to recover from session timeouts or errors
- **Serial Access Required**: Physical or USB CDC connection at 115200 baud
- **Information Leakage**: Session restart output reveals session state information

## Evidence
```cpp
// Command registration at line 1473
reg.registerCommand({"TR01_SESSION", "Start/restart TR01 session", cmdTr01Session, "tr01", false});
// The last 'false' means requiresAuth = false

// Handler at lines 943-958
static void cmdTr01Session(const char* args) {
    (void)args;
    auto* se = getSecureElementWithCheck();
    if (!se) return;

    if (se->isSessionActive()) {
        Console::printf("Session already active, reconnecting...\r\n");
        se->sessionEnd();
    }

    if (se->sessionStart()) {
        Console::printf("OK: Session started\r\n");
    } else {
        Console::printf("ERROR: Session start failed\r\n");
    }
}
```

Example output revealing session state:
```
TR01_SESSION
Session already active, reconnecting...
OK: Session started
```

## Recommended Fix
Add authentication requirement to the `TR01_SESSION` command:

```cpp
// Change registration from:
reg.registerCommand({"TR01_SESSION", "Start/restart TR01 session", cmdTr01Session, "tr01", false});

// To:
reg.registerCommand({"TR01_SESSION", "Start/restart TR01 session", cmdTr01Session, "tr01", true});
```

## References
- `components/serial_cmd/src/SerialCmd.cpp:1473` - Command registration
- `components/serial_cmd/src/SerialCmd.cpp:943-958` - Command handler
- `components/cdc_hal/src/Tropic01Element.cpp` - Session implementation
- CWE-287: Improper Authentication
- CWE-613: Insufficient Session Expiration
