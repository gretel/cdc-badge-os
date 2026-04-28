---
title: "[LOW] CommandRegistry command name parsing: empty command after whitespace"
severity: LOW
domain: serial/commands
lens: edge-cases
labels:
  - "audit:testing/edge-cases"
---

## Summary

The `CommandRegistry::processCommand()` function (`components/serial_cmd/src/CommandRegistry.cpp:103-109`) parses command names by reading characters until whitespace. However, when a command line consists **only of whitespace**, the parsing loop at line 106 never executes, leaving `cmdBuf[0] = '\0'`. The subsequent `strcasecmp(cmdBuf, "PING")` comparison works correctly, but an empty command name is passed to the command lookup loop, which is a minor edge case that could be handled more explicitly.

**Location**: `components/serial_cmd/src/CommandRegistry.cpp:103-112`

Additionally, if the input line starts with whitespace (e.g., `"   PING"`), the current code skips leading whitespace in the argument parsing (line 112) but **not** in the command name extraction loop (line 106), which could lead to unexpected behavior.

## Impact

**User Experience**: Commands with leading whitespace (e.g., `"   HELP"`) will be treated as unknown commands with the name `"   "` (three spaces) instead of `"HELP"`.

**Minor Parsing Inconsistency**: The code handles trailing whitespace correctly (stops at first whitespace) but doesn't handle leading whitespace, which is a common edge case in interactive shells.

## Evidence

**Code at line 95-112** (`processCommand()`):
```cpp
bool processCommand(const char* line) override {
    if (!line || !*line) return false;  // Handles empty line

    // Check line interceptor first (multiline input modes)
    if (lineInterceptor_ && lineInterceptor_(line)) {
        return true;
    }

    // Find command name (first word)
    char cmdBuf[64];
    size_t cmdLen = 0;
    while (*line && !isspace(*line) && cmdLen < sizeof(cmdBuf) - 1) {
        cmdBuf[cmdLen++] = *line++;
    }
    cmdBuf[cmdLen] = '\0';

    // Skip whitespace to get to arguments
    while (*line && isspace(*line)) line++;
    // ...
}
```

**Test cases that expose edge cases**:
1. **Leading whitespace**: `"   HELP"` → `cmdBuf = "   "` (spaces, not "HELP")
2. **Tabs**: `"\tPING"` → `cmdBuf = "\t"` (tab character)
3. **Mixed whitespace**: `"  PING  "` → `cmdBuf = "  "` (two spaces)

**Current behavior**:
- Empty line: Returns `false` immediately (line 96)
- Whitespace-only line: `cmdBuf = ""`, falls through to "Unknown command ''" error
- Leading whitespace: Command name includes leading whitespace chars

## Recommended Fix

Skip leading whitespace before parsing the command name:

```cpp
bool processCommand(const char* line) override {
    if (!line || !*line) return false;

    // Check line interceptor first (multiline input modes)
    if (lineInterceptor_ && lineInterceptor_(line)) {
        return true;
    }

    // Skip leading whitespace
    while (*line && isspace(*line)) line++;
    
    // Handle case where line was only whitespace
    if (!*line) return false;

    // Find command name (first word)
    char cmdBuf[64];
    size_t cmdLen = 0;
    while (*line && !isspace(*line) && cmdLen < sizeof(cmdBuf) - 1) {
        cmdBuf[cmdLen++] = *line++;
    }
    cmdBuf[cmdLen] = '\0';

    // Skip whitespace to get to arguments
    while (*line && isspace(*line)) line++;
    // ...
}
```

This ensures:
1. Leading whitespace is properly trimmed
2. Whitespace-only lines are handled efficiently
3. Command parsing is consistent with typical shell behavior

## References

- [POSIX Shell Command Parsing](https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html)
- [C `isspace()` behavior](https://en.cppreference.com/w/c/string/byte/isspace)
