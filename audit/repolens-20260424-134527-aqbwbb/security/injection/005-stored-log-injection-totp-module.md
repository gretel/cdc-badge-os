---
title: "[MEDIUM] Stored Log Injection via TOTP Module (TOTP_ADD)"
severity: MEDIUM
domain: Injection Vulnerabilities
lens: stored-injection
labels:
  - log-injection
  - stored-injection
  - totp-module
---

## Summary
The `TOTP_ADD` command accepts user-provided `name` and `issuer` fields which are stored and later displayed via `TOTP_LIST` and `TOTP_GET`. When displayed, the stored values are printed directly using `Console::printf()` without sanitization. If an attacker provides a name or issuer with control characters (newlines, carriage returns), those characters will be preserved and injected into the log output when the TOTP accounts are listed or retrieved.

**Location:** `components/mod_totp/src/TotpModule.cpp`
**Lines:** 208, 311 (display), 227-262 (input parsing)

## Impact
Stored log injection can lead to:
1. **Log Forging** - Attackers can create fake log entries by including `\r\n` in name/issuer fields
2. **Log Parser Breaking** - Multiline entries can break log analysis tools
3. **Confusion** - Developers reviewing logs may be misled by injected content
4. **Persistence** - Unlike direct injection, this survives across sessions until the TOTP account is deleted

Example attack:
```
TOTP_ADD "Google\r\n[INFO] Admin reset" SECRET123 issuer
```

When displayed later via `TOTP_LIST`:
```
1: Google
[INFO] Admin reset (slot 0)
```

Or via `TOTP_GET`:
```
123456 (30s) [Google
[INFO] Admin reset]
```

## Evidence
**Input Parsing:** `components/mod_totp/src/TotpModule.cpp:227-262`
```cpp
static void cmd_totp_add(const char* args) {
    char name[TotpStore::NAME_LEN + 1] = {};
    char secret[128] = {};
    char issuer[TotpStore::ISSUER_LEN + 1] = {};
    // ...
    const char* p = nextToken(args, name, sizeof(name));
    if (!p || !*name) {
        cdc::serial::Console::printf("Usage: TOTP_ADD name secret [issuer] [digits] [period] [algo]\r\n");
        return;
    }
    p = nextToken(p, secret, sizeof(secret));
    // ...
    p = nextToken(p, issuer, sizeof(issuer));
    // ...
    bool ok = TotpStore::instance().addAccount(
        name,
        issuer[0] ? issuer : nullptr,
        secret,
        digits,
        period,
        algo
    );
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

**Data Display (TOTP_LIST):** `components/mod_totp/src/TotpModule.cpp:208`
```cpp
auto cb = [](uint16_t slot, const cdc::core::TropicStorage::CacheEntry& entry, void* user) {
    auto* c = static_cast<ListCtx*>(user);
    uint16_t logical = 0;
    if (!TotpStore::instance().toLogicalSlot(slot, &logical)) return;
    cdc::serial::Console::printf("%u: %s (slot %u)\r\n", c->idx, entry.name, logical);  // Stored name printed
    c->idx++;
};
```

**Data Display (TOTP_GET):** `components/mod_totp/src/TotpModule.cpp:311`
```cpp
if (issuer && issuer[0]) {
    cdc::serial::Console::printf("%s (%ds) [%s]\r\n", code, remaining, issuer);  // Stored issuer printed
} else {
    cdc::serial::Console::printf("%s (%ds)\r\n", code, remaining);
}
```

**Data Storage:** `components/mod_totp/src/TotpStore.cpp:266-267`
```cpp
if (issuer) {
    strncpy(payload.issuer, issuer, sizeof(payload.issuer) - 1);  // No sanitization
}
```

**Input Flow:**
1. User provides input via `TOTP_ADD <name> <secret> [issuer]` command
2. Input is stored in TROPIC01 R-Memory via `TotpStore::addAccount()`
3. Later retrieved via `TOTP_LIST` or `TOTP_GET` commands
4. Stored values printed directly without sanitization

## Recommended Fix
Sanitize input at storage time (in `cmd_totp_add`) to strip or escape control characters:

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

// In cmd_totp_add:
// After strncpy(name, ...);
sanitizeString(name, name, sizeof(name));
// After strncpy(issuer, ...);
sanitizeString(issuer, issuer, sizeof(issuer));
```

Alternatively, create a shared utility function that can be reused across modules (GPG, TOTP, Password, etc.):

```cpp
// In a shared utility file
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
- Related to: #3 (Stored Log Injection via Password Module), #4 (Stored Log Injection via GPG Module)
