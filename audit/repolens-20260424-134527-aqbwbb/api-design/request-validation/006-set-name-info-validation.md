---
title: "[MEDIUM] SET_NAME, SET_INFO, SET_INFO2 commands accept unbounded strings"
severity: MEDIUM
domain: api-design/request-validation
lens: serial-command-interface
labels:
  - "request-validation"
  - "serial-commands"
  - "display"
  - "input-sanitization"
---

## Summary

The `SET_NAME`, `SET_INFO`, and `SET_INFO2` commands in `components/serial_cmd/src/SerialCmd.cpp` accept arbitrary length strings from command arguments and pass them directly to the text callback without any length validation or character filtering.

**Location:** `components/serial_cmd/src/SerialCmd.cpp:771-800`

## Impact

- **Buffer overflow risk**: The text callback implementation might expect bounded strings
- **Display corruption**: Very long strings could overflow display buffers
- **Special characters**: Newlines, tabs, or ANSI escape sequences could corrupt the display or terminal output
- **Inconsistent with other commands**: Other commands like `NVS_READ` use bounded parsing

## Evidence

```cpp
// File: components/serial_cmd/src/SerialCmd.cpp:771-800
static void cmdSetName(const char* args) {
    if (!args) args = "";
    if (s_textCallback) {
        s_textCallback("name", args);  // Line 776 - no validation!
    }
    Console::printf("OK: Name set to \"%s\"\r\n", args);
}

static void cmdSetInfo(const char* args) {
    if (!args) args = "";
    if (s_textCallback) {
        s_textCallback("info", args);  // Line 785 - no validation!
    }
    Console::printf("OK: Info set to \"%s\"\r\n", args);
}

static void cmdSetInfo2(const char* args) {
    if (!args) args = "";
    if (s_textCallback) {
        s_textCallback("info2", args);  // Line 794 - no validation!
    }
    Console::printf("OK: Info2 set to \"%s\"\r\n", args);
}
```

Issues:
1. No length validation - strings could be arbitrarily long
2. No character validation - newlines, tabs, ANSI escapes could be included
3. No trimming of leading/trailing whitespace
4. The callback might not handle long strings gracefully

## Recommended Fix

Add length and character validation:

```cpp
// Define max lengths for display fields
static constexpr size_t NAME_MAX_LEN = 32;
static constexpr size_t INFO_MAX_LEN = 48;

static void cmdSetName(const char* args) {
    if (!args) args = "";
    
    // Trim leading whitespace
    while (*args && isspace(*args)) args++;
    
    // Check for empty string
    if (!*args) {
        Console::printf("ERROR: Name cannot be empty\r\n");
        return;
    }
    
    // Validate and copy with length limit
    char name[NAME_MAX_LEN + 1];
    size_t i = 0;
    while (*args && !isspace(*args) && i < NAME_MAX_LEN) {
        // Allow printable ASCII, exclude control characters
        if (*args < 32 || *args > 126) {
            Console::printf("ERROR: Name must contain printable characters only\r\n");
            return;
        }
        name[i++] = *args++;
    }
    name[i] = '\0';
    
    // Check for truncation
    if (*args && !isspace(*args)) {
        Console::printf("ERROR: Name too long (max %d chars)\r\n", NAME_MAX_LEN);
        return;
    }
    
    if (s_textCallback) {
        s_textCallback("name", name);
    }
    Console::printf("OK: Name set to \"%s\"\r\n", name);
}

static void cmdSetInfo(const char* args) {
    if (!args) args = "";
    
    // Trim leading whitespace
    while (*args && isspace(*args)) args++;
    
    // Allow empty info
    if (!*args) {
        if (s_textCallback) {
            s_textCallback("info", "");
        }
        Console::printf("OK: Info cleared\r\n");
        return;
    }
    
    // Validate and copy with length limit
    char info[INFO_MAX_LEN + 1];
    size_t i = 0;
    while (*args && i < INFO_MAX_LEN) {
        // Allow printable ASCII and common punctuation
        if (*args < 32 || *args > 126) {
            Console::printf("ERROR: Info must contain printable characters only\r\n");
            return;
        }
        // Stop at newline
        if (*args == '\n' || *args == '\r') break;
        info[i++] = *args++;
    }
    info[i] = '\0';
    
    // Check for truncation
    if (*args && i >= INFO_MAX_LEN) {
        Console::printf("ERROR: Info too long (max %d chars)\r\n", INFO_MAX_LEN);
        return;
    }
    
    if (s_textCallback) {
        s_textCallback("info", info);
    }
    Console::printf("OK: Info set to \"%s\"\r\n", info);
}

static void cmdSetInfo2(const char* args) {
    // Same pattern as cmdSetInfo
    cmdSetInfo(args);  // Reuse validation logic with different key
}
```

## References

- CWE-20: Improper Input Validation
- CWE-120: Buffer copy without checking size of input
- CWE-131: Incorrect calculation of buffer size
