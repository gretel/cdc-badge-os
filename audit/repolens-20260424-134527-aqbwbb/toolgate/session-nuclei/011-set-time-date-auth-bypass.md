---
title: "[LOW] SET_TIME and SET_DATE commands allow time manipulation without authentication"
severity: LOW
domain: cdc-badge-os
lens: library/cdc-badge-os
labels:
  - "audit:toolgate/session-nuclei"
---

## Summary
The `SET_TIME` and `SET_DATE` serial commands allow modifying the system time without authentication. When `FEATURE_SECURE_SERIAL` is disabled (default), an attacker can set arbitrary system time, which affects TOTP code generation and any time-dependent features.

**Location**: `components/serial_cmd/src/SerialCmd.cpp:1458-1459` (registration), `components/serial_cmd/src/SerialCmd.cpp:694-726` (SET_TIME handler), `components/serial_cmd/src/SerialCmd.cpp:732-764` (SET_DATE handler)

## Impact
- **TOTP Synchronization**: TOTP codes depend on system time. An attacker can set time to match known TOTP secrets
- **Time-Based Features**: Any time-dependent functionality (lockouts, session timeouts) can be manipulated
- **Log Integrity**: Timestamps in logs become unreliable
- **Serial Access Required**: Physical or USB CDC connection at 115200 baud

## Evidence
```cpp
// Command registration at lines 1458-1459
reg.registerCommand({"SET_TIME", "Set time (HH:MM:SS)", cmdSetTime, "time", false});
reg.registerCommand({"SET_DATE", "Set date (DD.MM.YYYY)", cmdSetDate, "time", false});
// The last 'false' means requiresAuth = false

// SET_TIME handler at lines 694-726
static void cmdSetTime(const char* args) {
    if (!args || !*args) {
        Console::printf("Usage: SET_TIME HH:MM:SS\r\n");
        return;
    }
    int h, m, s;
    if (sscanf(args, "%d:%d:%d", &h, &m, &s) != 3) {
        Console::printf("ERROR: Invalid format. Use HH:MM:SS\r\n");
        return;
    }
    if (h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 60) {
        Console::printf("ERROR: Invalid time values\r\n");
        return;
    }

    struct timeval tv;
    struct tm* tm;
    if (getCurrentTime(tv, tm)) {
        tm->tm_hour = h;
        tm->tm_min = m;
        tm->tm_sec = s;
        if (setSystemTime(tm)) {
            Console::printf("OK: Time set to %02d:%02d:%02d\r\n", h, m, s);
            if (s_timeCallback) {
                s_timeCallback();
            }
        } else {
            Console::printf("ERROR: Failed to set time\r\n");
        }
    }
}
```

The TOTP module uses `time(nullptr)` to generate codes:
```cpp
// components/mod_totp/src/TotpStore.cpp:493
uint32_t code = generate(account.secret, account.secretLen, time(nullptr),
                         account.period, account.digits, ...);
```

## Recommended Fix
Add authentication requirement to both time commands:

```cpp
// Change registration from:
reg.registerCommand({"SET_TIME", "Set time (HH:MM:SS)", cmdSetTime, "time", false});
reg.registerCommand({"SET_DATE", "Set date (DD.MM.YYYY)", cmdSetDate, "time", false});

// To:
reg.registerCommand({"SET_TIME", "Set time (HH:MM:SS)", cmdSetTime, "time", true});
reg.registerCommand({"SET_DATE", "Set date (DD.MM.YYYY)", cmdSetDate, "time", true});
```

Alternatively, add explicit auth checks in the handlers:
```cpp
static void cmdSetTime(const char* args) {
#if FEATURE_SECURE_SERIAL
    if (!SerialCmd::isAuthenticated()) {
        Console::printf("ERROR: Authentication required. Use AUTH <pin> first.\r\n");
        return;
    }
#endif
    // ... rest of function
}
```

## References
- `components/serial_cmd/src/SerialCmd.cpp:1458-1459` - Command registration
- `components/serial_cmd/src/SerialCmd.cpp:694-726` - SET_TIME handler
- `components/serial_cmd/src/SerialCmd.cpp:732-764` - SET_DATE handler
- `components/mod_totp/src/TotpStore.cpp:493` - TOTP time usage
- CWE-200: Exposure of sensitive information to an unauthorized actor
- CWE-1313: Time-dependent behavior that can be manipulated
