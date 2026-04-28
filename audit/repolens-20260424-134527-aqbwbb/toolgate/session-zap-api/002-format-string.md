---
title: "[LOW] Potential Format String Vulnerability in Serial Command Output"
severity: LOW
domain: api-security
lens: session-zap-api
labels:
  - audit:toolgate/session-zap-api
---

## Summary

The serial command interface uses `Console::printf()` with user-supplied strings in several places without proper format string protection. While currently limited by the command structure, an attacker could potentially inject format specifiers (like `%s`, `%x`, `%n`) that could leak stack memory or cause unexpected behavior.

**Affected File:** `components/serial_cmd/src/SerialCmd.cpp`
**Lines:** 772, 786, 800 (cmdSetName, cmdSetInfo, cmdSetInfo2)

## Impact

Format string vulnerabilities can lead to:
- **Information disclosure** - Stack memory leakage using `%x` or `%s`
- **Denial of service** - Crash from invalid format specifiers
- **Memory corruption** (theoretical) - Using `%n` to write to memory (requires more context)

## Evidence

```cpp
// Line 772 - cmdSetName
static void cmdSetName(const char* args) {
    if (!args) args = "";
    if (s_textCallback) {
        s_textCallback("name", args);
    }
    Console::printf("OK: Name set to \"%s\"\r\n", args);  // args is user input
}

// Line 786 - cmdSetInfo
static void cmdSetInfo(const char* args) {
    if (!args) args = "";
    if (s_textCallback) {
        s_textCallback("info", args);
    }
    Console::printf("OK: Info set to \"%s\"\r\n", args);  // args is user input
}

// Line 800 - cmdSetInfo2
static void cmdSetInfo2(const char* args) {
    if (!args) args = "";
    if (s_textCallback) {
        s_textCallback("info2", args);
    }
    Console::printf("OK: Info2 set to \"%s\"\r\n", args);  // args is user input
}
```

User input flows directly into `printf()` format string context.

## Recommended Fix

Use `"%s"` format explicitly and escape format specifiers in user input, or use a safer print function:

**Option 1 - Escape % characters:**
```cpp
static void cmdSetName(const char* args) {
    if (!args) args = "";
    if (s_textCallback) {
        s_textCallback("name", args);
    }
    // Escape % to %% to prevent format string injection
    const char* safeArgs = args;
    Console::printf("OK: Name set to \"");
    for (const char* p = args; *p; p++) {
        if (*p == '%') Console::print("%%");
        else Console::putchar(*p);
    }
    Console::print("\"\r\n");
}
```

**Option 2 - Use strnlen and copy to buffer:**
```cpp
static void cmdSetName(const char* args) {
    if (!args) args = "";
    char safeBuf[256];
    size_t len = strnlen(args, sizeof(safeBuf) - 1);
    strncpy(safeBuf, args, len);
    safeBuf[len] = '\0';
    // Replace % with %%
    for (size_t i = 0; i < len; i++) {
        if (safeBuf[i] == '%') safeBuf[i] = '%';  // Escape
    }
    if (s_textCallback) {
        s_textCallback("name", args);
    }
    Console::printf("OK: Name set to \"%s\"\r\n", safeBuf);
}
```

## References

- CWE-134: Use of externally-controlled format string
- CWE-400: Uncontrolled growth of format specifiers
- OWASP: Format String Attack
