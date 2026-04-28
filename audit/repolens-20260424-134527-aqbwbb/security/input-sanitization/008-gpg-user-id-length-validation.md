---
title: "[MEDIUM] GPG User ID Serial Command Missing Length Validation"
severity: MEDIUM
domain: input-sanitization
lens: serial-commands
labels:
  - "audit:security/input-sanitization"
---

## Summary
The `GPG_GENERATE` serial command handler in `components/mod_gpg/src/GpgModule.cpp` (lines 146-178) accepts a user ID string without proper length validation before storing it in the pending user ID buffer. The command parses the curve argument and then copies the remaining input directly into a 64-byte buffer using `strncpy`, but does not validate that the user ID fits within reasonable bounds or provide feedback when truncation occurs.

**Location:** `components/mod_gpg/src/GpgModule.cpp:146-178`

```cpp
static void cmd_gpg_generate(const char* args) {
    char curveBuf[8] = {};
    char userId[GPG_USER_ID_MAX] = {};  // 64 bytes

    const char* p = args;
    // ... curve parsing ...
    
    // User ID copied without length validation
    strncpy(userId, p, sizeof(userId) - 1);  // Line 174

    uint8_t curve = (atoi(curveBuf) == 2) ? CDC_CURVE_P256 : CDC_CURVE_ED25519;
    gpg_set_pending_user_id(userId);  // Line 176
    bool ok = gpg_generate_key(curve);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

## Impact
1. **Silent Truncation**: User IDs longer than 63 characters are silently truncated, potentially causing confusion when the generated key doesn't match the expected user ID.
2. **No Feedback**: The `gpg_set_pending_user_id()` function returns `true` even when truncation occurs, providing no feedback to the user.
3. **OpenPGP User ID Format**: No validation that the user ID follows OpenPGP format conventions (e.g., `Name <email@example.com>`). Malformed user IDs may cause interoperability issues with GPG tools.

## Evidence
**File:** `components/mod_gpg/src/GpgModule.cpp`

**Lines 146-178 (cmd_gpg_generate):**
```cpp
static void cmd_gpg_generate(const char* args) {
    char curveBuf[8] = {};
    char userId[GPG_USER_ID_MAX] = {};

    const char* p = args;
    while (p && *p && std::isspace(static_cast<unsigned char>(*p))) p++;
    if (!p || !*p) {
        cdc::serial::Console::printf("Usage: GPG_GENERATE <curve> <user_id>\r\n");
        return;
    }

    size_t i = 0;
    while (p[i] && !std::isspace(static_cast<unsigned char>(p[i])) && i + 1 < sizeof(curveBuf)) {
        curveBuf[i] = p[i];
        i++;
    }
    curveBuf[i] = '\0';
    p += i;
    while (p && *p && std::isspace(static_cast<unsigned char>(*p))) p++;
    if (!p || !*p) {
        cdc::serial::Console::printf("Usage: GPG_GENERATE <curve> <user_id>\r\n");
        return;
    }
    strncpy(userId, p, sizeof(userId) - 1);  // No length validation here!

    uint8_t curve = (atoi(curveBuf) == 2) ? CDC_CURVE_P256 : CDC_CURVE_ED25519;
    gpg_set_pending_user_id(userId);
    bool ok = gpg_generate_key(curve);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

**Lines 328-333 (gpg_set_pending_user_id in gpg.cpp):**
```cpp
bool gpg_set_pending_user_id(const char *user_id) {
    if (!user_id || !user_id[0]) return false;
    strncpy(s_pending_user_id, user_id, sizeof(s_pending_user_id) - 1);
    s_pending_user_id[sizeof(s_pending_user_id) - 1] = '\0';
    return true;  // Returns true even if truncation occurred
}
```

The function returns `true` even when truncation occurs, providing no feedback to the user.

## Recommended Fix
Add explicit length validation and provide clear error feedback when the user ID exceeds the maximum allowed length:

```cpp
static void cmd_gpg_generate(const char* args) {
    char curveBuf[8] = {};
    char userId[GPG_USER_ID_MAX] = {};

    const char* p = args;
    while (p && *p && std::isspace(static_cast<unsigned char>(*p))) p++;
    if (!p || !*p) {
        cdc::serial::Console::printf("Usage: GPG_GENERATE <curve> <user_id>\r\n");
        return;
    }

    size_t i = 0;
    while (p[i] && !std::isspace(static_cast<unsigned char>(p[i])) && i + 1 < sizeof(curveBuf)) {
        curveBuf[i] = p[i];
        i++;
    }
    curveBuf[i] = '\0';
    p += i;
    while (p && *p && std::isspace(static_cast<unsigned char>(*p))) p++;
    if (!p || !*p) {
        cdc::serial::Console::printf("Usage: GPG_GENERATE <curve> <user_id>\r\n");
        return;
    }
    
    // Validate user ID length
    size_t userIdLen = strlen(p);
    if (userIdLen >= GPG_USER_ID_MAX) {
        cdc::serial::Console::printf("ERROR: User ID too long (max %d chars)\r\n", GPG_USER_ID_MAX - 1);
        return;
    }
    
    strncpy(userId, p, sizeof(userId) - 1);
    userId[sizeof(userId) - 1] = '\0';

    uint8_t curve = (atoi(curveBuf) == 2) ? CDC_CURVE_P256 : CDC_CURVE_ED25519;
    gpg_set_pending_user_id(userId);
    bool ok = gpg_generate_key(curve);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

Alternatively, update `gpg_set_pending_user_id` to return `false` when truncation would occur and handle that in the command handler.

## References
- [OpenPGP User ID Format](https://tools.ietf.org/html/rfc4880#section-12.2)
- [CWE-134: Use of Externally-Controlled Format String](https://cwe.mitre.org/data/definitions/134.html)
- [CWE-20: Improper Input Validation](https://cwe.mitre.org/data/definitions/20.html)
