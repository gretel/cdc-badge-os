---
title: "[HIGH] GPG_GENERATE command lacks user ID validation"
severity: HIGH
domain: api-design/request-validation
lens: serial-command-interface
labels:
  - "request-validation"
  - "serial-commands"
  - "gpg"
  - "input-sanitization"
---

## Summary

The `GPG_GENERATE` command in `components/mod_gpg/src/GpgModule.cpp` accepts a user ID string from serial command arguments but does not validate its format, length, or character set before passing it to the key generation function. The user ID can contain any characters including newlines, special characters, or be longer than expected.

**Location:** `components/mod_gpg/src/GpgModule.cpp:146-179`

## Impact

- **Injection attacks**: User ID could contain newlines or carriage returns to break console output formatting
- **Buffer overflow potential**: While `GPG_USER_ID_MAX` is 64, the parsing logic uses `strncpy` which may not null-terminate if input exceeds buffer
- **Display corruption**: Special characters could corrupt the display or serial output
- **Metadata pollution**: Invalid user IDs stored in NVS could cause issues with downstream consumers

## Evidence

```cpp
// File: components/mod_gpg/src/GpgModule.cpp:168-175
static void cmd_gpg_generate(const char* args) {
    char curveBuf[8] = {};
    char userId[GPG_USER_ID_MAX] = {};

    const char* p = args;
    // ... parsing curve ...
    
    // Line 172-175: No validation of user ID
    if (!p || !*p) {
        cdc::serial::Console::printf("Usage: GPG_GENERATE <curve> <user_id>\r\n");
        return;
    }
    strncpy(userId, p, sizeof(userId) - 1);  // No null termination check!

    uint8_t curve = (atoi(curveBuf) == 2) ? CDC_CURVE_P256 : CDC_CURVE_ED25519;
    gpg_set_pending_user_id(userId);  // Passed without validation
    bool ok = gpg_generate_key(curve);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

Issues:
1. `strncpy(userId, p, sizeof(userId) - 1)` does not guarantee null termination if `p` is longer than buffer
2. No validation that user ID contains only printable ASCII
3. No validation that user ID doesn't contain control characters (newlines, tabs, etc.)
4. No minimum length check (empty user IDs are valid but semantically questionable)

## Recommended Fix

Add comprehensive user ID validation:

```cpp
static void cmd_gpg_generate(const char* args) {
    char curveBuf[8] = {};
    char userId[GPG_USER_ID_MAX] = {};

    // ... existing curve parsing ...

    // Parse and validate user ID
    if (!p || !*p) {
        cdc::serial::Console::printf("Usage: GPG_GENERATE <curve> <user_id>\r\n");
        return;
    }
    
    // Copy and validate user ID character by character
    size_t i = 0;
    const char* start = p;
    while (*p && !isspace(*p) && i < sizeof(userId) - 1) {
        // Allow printable ASCII only (32-126)
        if (*p < 32 || *p > 126) {
            cdc::serial::Console::printf("ERROR: User ID must contain printable ASCII only\r\n");
            return;
        }
        userId[i++] = *p++;
    }
    userId[i] = '\0';
    
    // Check for truncation (user ID too long)
    if (*p && !isspace(*p)) {
        cdc::serial::Console::printf("ERROR: User ID too long (max %d chars)\r\n", GPG_USER_ID_MAX);
        return;
    }
    
    // Minimum length check
    if (i < 3) {
        cdc::serial::Console::printf("ERROR: User ID too short (min 3 chars)\r\n");
        return;
    }
    
    // Trim trailing whitespace
    while (i > 0 && (userId[i-1] == ' ' || userId[i-1] == '\t')) {
        userId[--i] = '\0';
    }
    
    uint8_t curve = (atoi(curveBuf) == 2) ? CDC_CURVE_P256 : CDC_CURVE_ED25519;
    gpg_set_pending_user_id(userId);
    bool ok = gpg_generate_key(curve);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

## References

- OpenPGP Smart Card Application 3.4.1 specification
- CWE-20: Improper Input Validation
- CWE-120: Buffer copy without checking size of input
- CWE-131: Incorrect calculation of buffer size
