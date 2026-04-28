---
title: "[MEDIUM] Inconsistent error response formats"
severity: MEDIUM
domain: api-design
lens: serial-command-api
labels:
  - "audit:api-design/rest-conventions"
---

## Summary
Commands return errors in inconsistent formats:

**Error formats used:**
- `ERROR: <message>` (most common)
- `ERROR` (bare, no message)
- `<message>` (no prefix)
- `(no entries)` (informal)

**Success formats used:**
- `OK` (most common)
- `OK: <message>`
- `<result>` (no prefix)

**Files**:
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/serial_cmd/src/SerialCmd.cpp`
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/mod_totp/src/TotpModule.cpp`
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/mod_gpg/src/GpgModule.cpp`
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/mod_password/src/PasswordModule.cpp`

## Impact
**Parsing difficulty**: Serial clients cannot reliably parse success/failure:
- Is `ERROR` a prefix or part of the message?
- Is `OK` a status or a message?
- How to distinguish "no data" from "error"?

**User experience**: Inconsistent feedback makes the interface feel unpolished.

## Evidence
```cpp
// Error format variations:
Console::printf("ERROR: Invalid %s number\r\n", slotTypeName);  // "ERROR: " prefix
Console::printf("ERROR\r\n");  // Bare ERROR (GPG, Password)
Console::printf("(no entries)\r\n");  // Informal (TOTP, Password)
Console::printf("ERROR: slot map not configured\r\n");  // "ERROR: " prefix

// Success format variations:
Console::printf("OK: Badge PIN retries reset to %d\r\n", retries);  // "OK: " prefix
Console::printf("OK\r\n");  // Bare OK (TOTP, GPG, Password)
Console::printf("OK: Authenticated\r\n");  // "OK: " prefix
Console::printf("PONG\r\n");  // No prefix (PING)
```

**Module-specific examples:**
```cpp
// TOTP module
Console::printf("OK\r\n");  // success
Console::printf("ERROR\r\n");  // failure

// GPG module  
Console::printf("OK\r\n");  // success
Console::printf("ERROR\r\n");  // failure

// Password module
Console::printf("OK\r\n");  // success
Console::printf("ERROR\r\n");  // failure

// System commands
Console::printf("OK: Session started\r\n");  // success with detail
Console::printf("ERROR: Session start failed\r\n");  // failure with detail
```

## Recommended Fix
**Standardize on consistent format:**

**Error responses:**
```
ERROR: <concise description>
```
Examples:
- `ERROR: Invalid slot number`
- `ERROR: Slot map not configured`
- `ERROR: Authentication failed`

**Success responses:**
```
OK[: <concise detail>]
```
Examples:
- `OK` (for simple operations)
- `OK: Session started` (when detail is useful)
- `OK: 3 entries deleted`

**Empty results (not errors):**
```
(empty)
(no entries)
```

**Implementation:**
```cpp
// Helper functions for consistency
static void printSuccess(const char* detail) {
    if (detail) Console::printf("OK: %s\r\n", detail);
    else Console::printf("OK\r\n");
}

static void printError(const char* msg) {
    Console::printf("ERROR: %s\r\n", msg);
}

static void printEmpty() {
    Console::printf("(no entries)\r\n");
}
```

## References
- REST: HTTP status codes + consistent error body
- Unix exit codes: 0 = success, non-zero = error
- RPC: Consistent response envelope `{success, data, error}`
