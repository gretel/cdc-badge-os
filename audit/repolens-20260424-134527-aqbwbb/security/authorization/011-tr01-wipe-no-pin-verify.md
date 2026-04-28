---
title: "[HIGH] TR01_WIPE destructive command lacks fresh PIN verification"
severity: HIGH
domain: authorization
lens: destructive-command-auth
labels:
  - "high:wipe-auth"
---

## Summary
The `TR01_WIPE` command performs a factory reset of the TROPIC01 secure element but only relies on the `requiresAuth` flag. When `FEATURE_SECURE_SERIAL` is disabled (default), no authentication is required, allowing any serial connection to wipe all secure element data.

**File:** `components/serial_cmd/src/SerialCmd.cpp`  
**Lines:** 1133-1188, 1481

## Evidence

**Command registration (line 1481):**
```cpp
reg.registerCommand({"TR01_WIPE", "Factory reset (TR01_WIPE CONFIRM)", cmdTr01Wipe, "tr01", true});
```

**Handler implementation (lines 1133-1188):**
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

    // ... wipes all ECC and R-Memory slots ...
}
```

The command uses `requiresAuth = true`, which only checks `FEATURE_SECURE_SERIAL` session authentication. When the feature is disabled, the session check becomes a no-op.

## Impact
An attacker with serial access can:
1. Wipe all ECC keys (GPG, CA, FIDO2 credentials)
2. Erase all R-Memory data (PINs, TOTP accounts, passwords)
3. Effectively brick the device's security functionality

This is a complete data loss event for all secure element contents.

## Recommended Fix
Add explicit PIN verification before executing the wipe:

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

    // Require fresh PIN verification
    auto& pm = core::PinManager::instance();
    char pinBuf[16] = {};
    Console::printf("Enter Badge PIN to confirm: ");
    // Read PIN silently (implement silent input)
    if (!pm.verifyBadgePin(pinBuf)) {
        Console::printf("ERROR: PIN verification failed\r\n");
        return;
    }

    Console::printf("=== TROPIC01 Factory Reset ===\r\n");
    // ... rest of wipe logic ...
}
```

## References
- CWE-287: Improper Authentication
- CWE-663: Use of a One-Time Key for a Long-Lived Key
- TROPIC01 Secure Element Documentation
