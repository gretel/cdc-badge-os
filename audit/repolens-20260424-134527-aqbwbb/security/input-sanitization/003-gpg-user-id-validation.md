---
title: "[LOW] Missing format validation for GPG user ID input"
severity: LOW
domain: input-sanitization
lens: serial-commands
labels:
  - "audit:security/input-sanitization"
---

## Summary
The `cmd_gpg_generate` function in `components/mod_gpg/src/GpgModule.cpp` accepts a user ID string via the serial command `GPG_GENERATE` but does not validate the format. The user ID is passed directly to `gpg_set_pending_user_id()` with only a null-check, allowing potentially malformed or unexpected values to be stored.

**Location:** `components/mod_gpg/src/GpgModule.cpp:151-176`
**Location:** `components/mod_gpg/src/gpg.cpp:334-341` (gpg_set_pending_user_id)

## Impact
- **Format flexibility**: While OpenPGP allows flexible user ID formats, accepting completely arbitrary strings could lead to:
  - Non-standard user IDs that may not display correctly in GPG clients
  - Potential issues with special characters or control characters
  - Confusion when exporting keys to systems expecting standard format
- **No feedback**: Users don't get guidance on acceptable user ID formats
- **Buffer safety**: The current implementation does use `strncpy()` with bounds checking, so buffer overflow is not a concern

## Evidence
```cpp
// components/mod_gpg/src/GpgModule.cpp:151-176
static void cmd_gpg_generate(const char* args) {
    char curveBuf[8] = {};
    char userId[GPG_USER_ID_MAX] = {};

    const char* p = args;
    // ... parsing code ...
    strncpy(userId, p, sizeof(userId) - 1);  // Only bounds check, no format validation

    uint8_t curve = (atoi(curveBuf) == 2) ? CDC_CURVE_P256 : CDC_CURVE_ED25519;
    gpg_set_pending_user_id(userId);  // User ID passed without validation
    bool ok = gpg_generate_key(curve);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

```cpp
// components/mod_gpg/src/gpg.cpp:334-341
bool gpg_set_pending_user_id(const char *user_id) {
    if (!user_id || !user_id[0]) return false;  // Only null/empty check
    strncpy(s_pending_user_id, user_id, sizeof(s_pending_user_id) - 1);
    s_pending_user_id[sizeof(s_pending_user_id) - 1] = '\0';
    return true;
}
```

Standard OpenPGP user ID format is typically: `Name (Comment) <email@example.com>`

## Recommended Fix
Add basic format validation to ensure the user ID contains at least some expected structure. At minimum, validate that the length is reasonable and optionally check for common patterns:

```cpp
static void cmd_gpg_generate(const char* args) {
    char curveBuf[8] = {};
    char userId[GPG_USER_ID_MAX] = {};

    const char* p = args;
    // ... parsing code ...
    strncpy(userId, p, sizeof(userId) - 1);

    // Validate user ID format
    size_t userIdLen = strlen(userId);
    if (userIdLen < 3) {  // Minimum reasonable length
        cdc::serial::Console::printf("ERROR: User ID too short (min 3 chars)\r\n");
        return;
    }
    
    // Optional: Check for common patterns
    // At least one of: name, email, or comment should be present
    bool hasEmail = strchr(userId, '@') != nullptr;
    bool hasName = userIdLen > 0;
    
    if (!hasName) {
        cdc::serial::Console::printf("ERROR: User ID should contain name or email\r\n");
        return;
    }

    uint8_t curve = (atoi(curveBuf) == 2) ? CDC_CURVE_P256 : CDC_CURVE_ED25519;
    gpg_set_pending_user_id(userId);
    bool ok = gpg_generate_key(curve);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

For a more strict validation, check for standard OpenPGP format:
```cpp
// Check for standard format: "Name <email>" or just "Name"
bool isValidUserId(const char* userId) {
    size_t len = strlen(userId);
    if (len < 3 || len > GPG_USER_ID_MAX) return false;
    
    // Allow any non-empty string for flexibility
    // Could add stricter checks here if needed
    return true;
}
```

## References
- RFC 4880: OpenPGP Message Format - Section 5.11 User ID Packet
- OpenPGP Best Practices for User ID formatting
- CWE-20: Improper Input Validation