---
title: "[LOW] TR01_WIPE command lacks authenticated session requirement"
severity: LOW
domain: database
lens: query-safety
labels:
  - "audit:database/query-safety"
---

## Summary
The `TR01_WIPE` command at `components/serial_cmd/src/SerialCmd.cpp:1121-1188` performs a factory reset of the TROPIC01 secure element with only a `CONFIRM` argument check, but doesn't require an authenticated serial session (when `FEATURE_SECURE_SERIAL` is enabled).

## Impact
- **Accidental Wipe Risk**: Anyone with serial access can wipe all secure element data with `TR01_WIPE CONFIRM`
- **No Authentication Barrier**: Unlike other destructive commands that could benefit from auth, this one is accessible to unauthenticated users
- **Irreversible Data Loss**: All ECC keys (slots 0-31) and R-Memory data (slots 0-511) are erased

## Evidence
File: `components/serial_cmd/src/SerialCmd.cpp`

Lines 1121-1188 (cmdTr01Wipe function):
```cpp
static void cmdTr01Wipe(const char* args) {
    auto* se = getSecureElementWithCheck();
    if (!se) return;

    if (!args || strcmp(args, "CONFIRM") != 0) {
        Console::printf("WARNING: This will ERASE ALL data on TROPIC01!\r\n");
        Console::printf("  - All ECC keys (slots 0-31)\r\n");
        Console::printf("  - All R-Memory data (slots 0-511)\r\n");
        Console::printf("\r\nTo proceed, type: TR01_WIPE CONFIRM\r\n");
        return;
    }

    Console::printf("=== TROPIC01 Factory Reset ===\r\n");
    Console::flush();

    if (!se->isSessionActive()) {
        if (!se->sessionStart()) {
            Console::printf("ERROR: Cannot start session\r\n");
            return;
        }
    }

    // Erases all ECC keys and R-Memory slots...
}
```

Compare to `NVS_CLEAR` at lines 489-508 which has similar confirmation logic but could benefit from auth check:
```cpp
static void cmdNvsClear(const char* args) {
    if (!args || strcmp(args, "YES") != 0) {
        Console::printf("WARNING: This will ERASE ALL NVS data!\r\n");
        // ...
        return;
    }
    // No auth check here either - both commands could be improved
}
```

Command registration at line 1482:
```cpp
reg.registerCommand({"TR01_WIPE", "Factory reset (TR01_WIPE CONFIRM)", cmdTr01Wipe, "tr01", true});
```
Note: `true` indicates it's a privileged command, but no auth check is performed in the handler.

## Recommended Fix
Add authenticated session check for `TR01_WIPE`:

1. Add authentication check at start of `cmdTr01Wipe`:
```cpp
#if FEATURE_SECURE_SERIAL
static void cmdTr01Wipe(const char* args) {
    // Require authentication for destructive operation
    if (!SerialCmd::isAuthenticated()) {
        Console::printf("ERROR: Authentication required. Use AUTH <pin> first.\r\n");
        return;
    }
    
    auto* se = getSecureElementWithCheck();
    if (!se) return;
    // ... rest of function
}
#endif

#if !FEATURE_SECURE_SERIAL
static void cmdTr01Wipe(const char* args) {
    // Original implementation for non-secure mode
    // ...
}
#endif
```

2. Alternatively, require a more explicit confirmation like `TR01_WIPE CONFIRM_<PIN>`

3. Add a second confirmation step after `CONFIRM`:
```cpp
if (strcmp(args, "CONFIRM") == 0) {
    Console::printf("WARNING: Second confirmation required!\r\n");
    Console::printf("Type: TR01_WIPE CONFIRM_AGAIN\r\n");
    return;
}
if (strcmp(args, "CONFIRM_AGAIN") != 0) { ... }
```

## References
- TROPIC01 Secure Element documentation
- Serial command registration: `SerialCmd.cpp:1482`
- Authentication system: `SerialCmd.cpp:1363-1390`
- Similar pattern: `NVS_CLEAR` command at `SerialCmd.cpp:489`
