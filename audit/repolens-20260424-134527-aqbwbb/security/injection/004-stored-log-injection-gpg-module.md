---
title: "[MEDIUM] Stored Log Injection via GPG Module (GPG_GENERATE)"
severity: MEDIUM
domain: Injection Vulnerabilities
lens: stored-injection
labels:
  - log-injection
  - stored-injection
  - gpg-module
---

## Summary
The `GPG_GENERATE` command accepts a user_id parameter which is stored and later displayed via `GPG_STATUS`. When displayed, the stored user_id is printed directly using `Console::printf()` without sanitization. If an attacker provides a user_id with control characters (newlines, carriage returns) in the format string, those characters will be preserved and injected into the log output when the status is retrieved.

**Location:** `components/mod_gpg/src/GpgModule.cpp`
**Lines:** 132-144 (display), 150-179 (input parsing)

## Impact
Stored log injection can lead to:
1. **Log Forging** - Attackers can create fake log entries by including `\r\n` in the user_id field
2. **Log Parser Breaking** - Multiline entries can break log analysis tools
3. **Confusion** - Developers reviewing logs may be misled by injected content
4. **Persistence** - Unlike direct injection, this survives across sessions until the GPG key is regenerated

Example attack:
```
GPG_GENERATE 1 "admin@badge.com\r\n[INFO] PIN reset complete"
```

When displayed later via `GPG_STATUS`:
```
User-ID: admin@badge.com
[INFO] PIN reset complete
Curve: Ed25519
Created: 1234567890
Sign Count: 0
```

## Evidence
**Input Parsing:** `components/mod_gpg/src/GpgModule.cpp:150-173`
```cpp
static void cmd_gpg_generate(const char* args) {
    char curveBuf[8] = {};
    char userId[GPG_USER_ID_MAX] = {};

    const char* p = args;
    while (p && *p && std::isspace(static_cast<unsigned char>(*p))) p++;
    // ... parse curve ...
    while (p && *p && std::isspace(static_cast<unsigned char>(*p))) p++;
    if (!p || !*p) {
        cdc::serial::Console::printf("Usage: GPG_GENERATE <curve> <user_id>\r\n");
        return;
    }
    strncpy(userId, p, sizeof(userId) - 1);  // No sanitization of user_id

    uint8_t curve = (atoi(curveBuf) == 2) ? CDC_CURVE_ED25519 : CDC_CURVE_P256;
    gpg_set_pending_user_id(userId);
    bool ok = gpg_generate_key(curve);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

**Data Display:** `components/mod_gpg/src/GpgModule.cpp:132-144`
```cpp
static void cmd_gpg_status(const char* args) {
    (void)args;
    gpg_status_t status = {};
    if (!gpg_get_status(&status)) {
        cdc::serial::Console::printf("ERROR: No key configured\r\n");
        return;
    }
    cdc::serial::Console::printf("User-ID: %s\r\n", status.user_id);  // Stored value printed directly
    cdc::serial::Console::printf("Curve: %s\r\n",
                                 status.curve == CDC_CURVE_ED25519 ? "Ed25519" : "P-256");
    cdc::serial::Console::printf("Created: %lu\r\n", static_cast<unsigned long>(status.created_at));
    cdc::serial::Console::printf("Sign Count: %lu\r\n", static_cast<unsigned long>(status.sign_count));
}
```

**Input Flow:**
1. User provides input via `GPG_GENERATE <curve> <user_id>` command
2. Input is stored via `gpg_set_pending_user_id(userId)` and persisted during key generation
3. Later retrieved via `GPG_STATUS` command
4. Stored value printed directly without sanitization

## Recommended Fix
Sanitize input at storage time (in `cmd_gpg_generate`) to strip or escape control characters:

```cpp
/**
 * \brief Sanitize string by removing control characters.
 * \param src Source string.
 * \param dest Destination buffer.
 * \param destSize Destination buffer size.
 */
static void sanitizeString(const char* src, char* dest, size_t destSize) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j < destSize - 1; i++) {
        char c = src[i];
        // Keep printable ASCII only (0x20-0x7E)
        if (c >= 0x20 && c < 0x7F) {
            dest[j++] = c;
        }
    }
    dest[j] = '\0';
}

// In cmd_gpg_generate:
// After strncpy(userId, p, sizeof(userId) - 1);
sanitizeString(userId, userId, sizeof(userId));
```

Alternatively, create a helper function that can be reused across modules:

```cpp
// In a shared utility file (e.g., components/mod_password/src/PasswordModule.cpp or new utility)
static void sanitizeForStorage(const char* src, char* dest, size_t destSize) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j < destSize - 1; i++) {
        char c = src[i];
        // Keep printable ASCII and common whitespace (space, tab)
        if ((c >= 0x20 && c < 0x7F) || c == '\t') {
            dest[j++] = c;
        }
    }
    dest[j] = '\0';
}
```

## References
- [OWASP: Log Injection](https://owasp.org/www-community/attacks/Log_Injection)
- [CWE-117: Improper Output Neutralization for Logs](https://cwe.mitre.org/data/definitions/117.html)
- [CWE-93: Improper Neutralization of CRLF Sequences](https://cwe.mitre.org/data/definitions/93.html)
- Related to: #3 (Stored Log Injection via Password Module)
