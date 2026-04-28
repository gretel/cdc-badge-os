---
title: "[MEDIUM] Log Injection via Display Commands (SET_NAME, SET_INFO, SET_INFO2)"
severity: MEDIUM
domain: Injection Vulnerabilities
lens: log-injection
labels:
  - log-injection
  - injection
  - serial-console
---

## Summary
The serial console commands `SET_NAME`, `SET_INFO`, and `SET_INFO2` accept user input that is later logged via `LOG_D` macro. When the command is executed, the full command line (including arguments) is logged at line 1413 in `SerialCmd.cpp`:

```cpp
LOG_D(TAG, "Executing: %s", cmd);
```

If a user sets a display value containing newline characters (`\r`, `\n`) or ANSI escape sequences, it can forge log entries or inject misleading information into the log stream.

**Location:** `components/serial_cmd/src/SerialCmd.cpp`
**Lines:** 774-804 (commands), 1413 (logging)

## Impact
Log injection can lead to:
1. **Log Forging** - An attacker can create fake log entries by including `\r\n` in display values
2. **Log Analysis Evasion** - Injection of special characters can break log parsers
3. **ANSI Escape Sequences** - Color codes or cursor movement can make logs harder to read/analyze
4. **Debug Confusion** - Developers reviewing logs may be misled by injected content

Example attack:
```
SET_NAME Admin\r\n[DEBUG] PIN reset completed
```

This would appear in logs as:
```
[DEBUG][SERIAL] Executing: SET_NAME Admin
[DEBUG][SERIAL] PIN reset completed
```

## Evidence
**Command Handlers:** `components/serial_cmd/src/SerialCmd.cpp:774-804`
```cpp
static void cmdSetName(const char* args) {
    if (!args) args = "";
    if (s_textCallback) {
        s_textCallback("name", args);
    }
    Console::printf("OK: Name set to \"%s\"\r\n", args);
}
```

**Command Logging:** `components/serial_cmd/src/SerialCmd.cpp:1413`
```cpp
LOG_D(TAG, "Executing: %s", cmd);
```

**Registration:** Lines 1462-1464
```cpp
reg.registerCommand({"SET_NAME", "Set display name", cmdSetName, "display", false});
reg.registerCommand({"SET_INFO", "Set Info line 1", cmdSetInfo, "display", false});
reg.registerCommand({"SET_INFO2", "Set Info line 2", cmdSetInfo2, "display", false});
```

**Command Parsing:** `components/serial_cmd/src/CommandRegistry.cpp:95-158`
The command line is parsed and logged before the handler is called, so the raw user input (including any newlines) is logged.

## Recommended Fix
Sanitize user input before logging by replacing control characters with escaped representations:

**Option 1 - Create a sanitize function:**
```cpp
/**
 * \brief Sanitize string for safe logging (escape control chars).
 * \param str Input string.
 * \param buf Output buffer.
 * \param bufSize Size of output buffer.
 * \return Pointer to buffer.
 */
static const char* sanitizeForLogging(const char* str, char* buf, size_t bufSize) {
    if (!str) str = "";
    size_t j = 0;
    for (size_t i = 0; str[i] && j < bufSize - 2; i++) {
        char c = str[i];
        if (c == '\r' && j < bufSize - 2) {
            buf[j++] = '\\';
            buf[j++] = 'r';
        } else if (c == '\n' && j < bufSize - 2) {
            buf[j++] = '\\';
            buf[j++] = 'n';
        } else if (c == '\t' && j < bufSize - 2) {
            buf[j++] = '\\';
            buf[j++] = 't';
        } else if (c == '\\' && j < bufSize - 2) {
            buf[j++] = '\\';
            buf[j++] = '\\';
        } else {
            buf[j++] = c;
        }
    }
    buf[j] = '\0';
    return buf;
}
```

**Option 2 - Strip control characters:**
```cpp
static void cmdSetName(const char* args) {
    if (!args) args = "";
    
    // Strip control characters for display
    char safeArgs[256];
    size_t j = 0;
    for (size_t i = 0; args[i] && j < sizeof(safeArgs) - 1; i++) {
        char c = args[i];
        if (c >= 0x20 && c < 0x7F) {  // Printable ASCII only
            safeArgs[j++] = c;
        }
    }
    safeArgs[j] = '\0';
    
    if (s_textCallback) {
        s_textCallback("name", safeArgs);
    }
    Console::printf("OK: Name set to \"%s\"\r\n", safeArgs);
}
```

Apply the same sanitization to `cmdSetInfo` and `cmdSetInfo2`.

## References
- [OWASP: Log Injection](https://owasp.org/www-community/attacks/Log_Injection)
- [CWE-117: Improper Output Neutralization for Logs](https://cwe.mitre.org/data/definitions/117.html)
- [CWE-134: Use of Externally-Controlled Format String](https://cwe.mitre.org/data/definitions/134.html)
