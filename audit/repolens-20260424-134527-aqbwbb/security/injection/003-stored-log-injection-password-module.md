---
title: "[MEDIUM] Stored Log Injection via Password Module (PASSWORD_ADD)"
severity: MEDIUM
domain: Injection Vulnerabilities
lens: stored-injection
labels:
  - log-injection
  - stored-injection
  - password-module
---

## Summary
The `PASSWORD_ADD` command accepts user input (title, username, password, URL, notes) which is stored and later displayed via `PASSWORD_GET`. When displayed, the stored values are printed directly using `Console::printf()` without sanitization. If an attacker adds a password entry with control characters (newlines, carriage returns) in any field, those characters will be preserved and injected into the log output when the entry is retrieved.

**Location:** `components/mod_password/src/PasswordModule.cpp`
**Lines:** 236-245 (display), 255-300 (input parsing)

## Impact
Stored log injection can lead to:
1. **Log Forging** - Attackers can create fake log entries by including `\r\n` in password fields
2. **Log Parser Breaking** - Multiline entries can break log analysis tools
3. **Confusion** - Developers reviewing logs may be misled by injected content
4. **Persistence** - Unlike direct injection, this survives across sessions

Example attack:
```
PASSWORD_ADD "Test\r\n[INFO] Admin PIN: 1234" user pass url notes
```

When displayed later:
```
Title: Test
[INFO] Admin PIN: 1234
Username: user
...
```

## Evidence
**Input Parsing:** `components/mod_password/src/PasswordModule.cpp:255-300`
```cpp
static void cmd_password_add(const char* args) {
    PasswordEntry entry = {};
    char title[PasswordStore::TITLE_LEN + 1] = {};
    char username[PasswordStore::USERNAME_LEN + 1] = {};
    char password[PasswordStore::PASSWORD_LEN + 1] = {};
    char url[PasswordStore::URL_LEN + 1] = {};
    char notes[PasswordStore::NOTES_LEN + 1] = {};

    const char* p = nextToken(args, title, sizeof(title));
    // ... parsing ...
    strncpy(entry.title, title, sizeof(entry.title) - 1);
    strncpy(entry.password, password, sizeof(entry.password) - 1);
    // ... notes handling ...
    strncpy(entry.notes, notes, sizeof(entry.notes) - 1);

    bool ok = PasswordStore::instance().addEntry(entry);
}
```

**Data Display:** `components/mod_password/src/PasswordModule.cpp:236-245`
```cpp
static void cmd_password_get(const char* args) {
    // ...
    cdc::serial::Console::printf("Title: %s\r\n", entry.title);
    cdc::serial::Console::printf("Username: %s\r\n", entry.username);
    cdc::serial::Console::printf("Password: %s\r\n", entry.password);
    cdc::serial::Console::printf("URL: %s\r\n", entry.url);
    cdc::serial::Console::printf("Notes: %s\r\n", entry.notes);
}
```

**Input Flow:**
1. User provides input via `PASSWORD_ADD` command
2. Input is stored in NVS via `PasswordStore::addEntry()`
3. Later retrieved via `PASSWORD_GET` command
4. Stored values printed directly without sanitization

## Recommended Fix
Sanitize input at storage time (in `cmd_password_add`) to strip or escape control characters:

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
        // Keep printable ASCII and common whitespace
        if ((c >= 0x20 && c < 0x7F) || c == ' ') {
            dest[j++] = c;
        }
    }
    dest[j] = '\0';
}

// In cmd_password_add:
sanitizeString(title, entry.title, sizeof(entry.title));
sanitizeString(username, entry.username, sizeof(entry.username));
sanitizeString(password, entry.password, sizeof(entry.password));
sanitizeString(url, entry.url, sizeof(entry.url));
sanitizeString(notes, entry.notes, sizeof(entry.notes));
```

Alternatively, sanitize at display time in `cmd_password_get` if you want to preserve raw data for other operations.

## References
- [OWASP: Log Injection](https://owasp.org/www-community/attacks/Log_Injection)
- [CWE-117: Improper Output Neutralization for Logs](https://cwe.mitre.org/data/definitions/117.html)
- [CWE-93: Improper Neutralization of CRLF Sequences](https://cwe.mitre.org/data/definitions/93.html)
