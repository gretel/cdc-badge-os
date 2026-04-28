---
title: "[LOW] ERROR_LOG command dumps error log without authentication"
severity: LOW
domain: cdc-badge-os
lens: library/cdc-badge-os
labels:
  - "audit:toolgate/session-nuclei"
---

## Summary
The `ERROR_LOG` serial command dumps the system error log without requiring authentication. When `FEATURE_SECURE_SERIAL` is disabled (default), an attacker with serial access can view error and warning messages that may contain sensitive information about device operations, module states, and potential vulnerabilities.

**Location**: `components/serial_cmd/src/SerialCmd.cpp:1446` (registration), `components/serial_cmd/src/SerialCmd.cpp:474-481` (handler)

## Impact
- **Information Disclosure**: Error log may contain:
  - Module names and states
  - PIN verification failures
  - Secure element operation details
  - Memory allocation failures
  - Timing information
- **Reconnaissance**: Helps attacker understand device behavior and identify potential attack vectors
- **Clear Without Auth**: The `ERROR_LOG CLEAR` command can also be executed without authentication
- **Serial Access Required**: Physical or USB CDC connection at 115200 baud

## Evidence
```cpp
// Command registration at line 1446
reg.registerCommand({"ERROR_LOG", "Show error log (CLEAR to reset)", cmdErrorLog, "system", false});
// The last 'false' means requiresAuth = false

// Handler at lines 474-481
static void cmdErrorLog(const char* args) {
    if (args && strcmp(args, "CLEAR") == 0) {
        error_log_clear();
        Console::printf("Error log cleared.\r\n");
    } else {
        error_log_dump();
    }
}

// Error log dump function at components/cdc_log/src/cdc_log.cpp
void error_log_dump(void) {
    if (s_error_log_count == 0) {
        console_printf("Error log: (empty)\r\n");
        return;
    }

    console_printf("Error log (%zu entries):\r\n", s_error_log_count);

    for (size_t i = 0; i < s_error_log_count; i++) {
        error_log_entry_t* e = &s_error_log[idx];
        console_printf("[%02lu:%02lu:%02lu][%s] %s\r\n",
                       hours % 24, mins % 60, secs % 24,
                       e->level == CDC_LOG_LEVEL_ERROR ? "E" : "W",
                       e->message);  // <-- Exposes log messages!
    }
}
```

Example log output that might reveal information:
```
ERROR_LOG
Error log (5 entries):
[14:23:45][E] PIN verification failed, 2 retries left
[14:24:12][E] TR01 session start failed
[14:25:30][W] Cache rebuild: slot 32 mismatched module
[14:26:00][E] NVS write failed (0x102)
[14:27:15][W] Heap low: 5000 bytes free
```

## Recommended Fix
Add authentication requirement to the `ERROR_LOG` command:

```cpp
// Change registration from:
reg.registerCommand({"ERROR_LOG", "Show error log (CLEAR to reset)", cmdErrorLog, "system", false});

// To:
reg.registerCommand({"ERROR_LOG", "Show error log (CLEAR to reset)", cmdErrorLog, "system", true});
```

## References
- `components/serial_cmd/src/SerialCmd.cpp:1446` - Command registration
- `components/serial_cmd/src/SerialCmd.cpp:474-481` - Command handler
- `components/cdc_log/src/cdc_log.cpp` - Error log implementation
- CWE-532: Insertion of sensitive information into log file
- CWE-200: Exposure of sensitive information to an unauthorized actor
