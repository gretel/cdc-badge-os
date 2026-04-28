---
title: "[LOW] CommandRegistry doesn't handle leading/trailing whitespace in command names"
severity: LOW
domain: serial_cmd/CommandRegistry
lens: edge-case-testing
labels:
  - "audit:testing/edge-cases"
---

## Summary
In `CommandRegistry::processCommand()` (file: `components/serial_cmd/src/CommandRegistry.cpp:95-163`), the command name parsing at lines 100-106 extracts the first word but doesn't trim leading whitespace. A command like `"  HELP"` (with leading spaces) will fail to match.

Lines 100-106:
```cpp
char cmdBuf[64];
size_t cmdLen = 0;
while (*line && !isspace(*line) && cmdLen < sizeof(cmdBuf) - 1) {
    cmdBuf[cmdLen++] = *line++;
}
cmdBuf[cmdLen] = '\0';
```

If `line` starts with spaces, the first character is a space, so `isspace(*line)` is true, and the loop exits immediately with an empty `cmdBuf`.

## Impact
- **UX frustration**: Users typing `"  HELP"` get "Unknown command" error
- **Script compatibility**: Scripts with inconsistent whitespace may fail
- **Debugging**: Hard to see if command was mistyped or had whitespace

## Evidence
File: `components/serial_cmd/src/CommandRegistry.cpp`, lines 95-163

```cpp
bool processCommand(const char* line) override {
    if (!line || !*line) return false;

    // Check line interceptor first (multiline input modes)
    if (lineInterceptor_ && lineInterceptor_(line)) {
        return true;
    }

    // Find command name (first word)
    char cmdBuf[64];
    size_t cmdLen = 0;
    while (*line && !isspace(*line) && cmdLen < sizeof(cmdBuf) - 1) {  // Line 100
        cmdBuf[cmdLen++] = *line++;
    }
    cmdBuf[cmdLen] = '\0';  // Line 106

    // Skip whitespace to get to arguments
    while (*line && isspace(*line)) line++;  // Line 109
    // ...
}
```

Edge cases:
1. `"  HELP"` → `cmdBuf = ""` (empty), fails to match "HELP"
2. `"  "` (only spaces) → `cmdBuf = ""`, returns false
3. `"HELP  "` → Works correctly (trailing spaces go to args)

## Recommended Fix
Add leading whitespace trimming before parsing command name:

```cpp
bool processCommand(const char* line) override {
    if (!line || !*line) return false;

    // Check line interceptor first (multiline input modes)
    if (lineInterceptor_ && lineInterceptor_(line)) {
        return true;
    }

    // Skip leading whitespace
    while (*line && isspace(*line)) line++;

    // If line is now empty (only whitespace), return false
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

## References
- CWE-131: Incorrect Calculation of Multi-Byte String Length
- Common UX patterns for command-line interfaces
