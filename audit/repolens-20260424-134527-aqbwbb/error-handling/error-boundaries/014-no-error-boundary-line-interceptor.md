---
title: "[LOW] No error boundary around line interceptor callback"
severity: LOW
domain: error-handling
lens: error-boundaries
labels:
  - "error-handling"
  - "serial-commands"
---

## Summary
The `CommandRegistry::processCommand()` function in `components/serial_cmd/src/CommandRegistry.cpp:99-100` calls the line interceptor callback without error isolation. A single interceptor throwing will break command processing for all subsequent commands.

**Evidence:**
- `components/serial_cmd/src/CommandRegistry.cpp:99-100`:
```cpp
// Check line interceptor first (multiline input modes)
if (lineInterceptor_ && lineInterceptor_(line)) {
    return true;  // No error boundary!
}
```

- Called for every command line processed in the main loop:
```cpp
// In SerialCmd.cpp main loop
cdc::serial::getCommandRegistry().processCommand(line);
```

- Usage pattern (for multiline input modes):
```cpp
getCommandRegistry().setLineInterceptor([](const char* line) {
    // Multiline input logic that could throw
    if (inMultilineMode) {
        accumulateLine(line);
        return true;
    }
    return false;
});
```

## Impact
- **Command processing blackout**: One crashing interceptor stops all command processing
- **Serial console lockup**: User may need to reconnect to recover
- **Silent failures**: Interceptor errors not logged
- **Debug difficulty**: Hard to identify which interceptor failed

## Recommended Fix
Add error boundary around line interceptor callback:

```cpp
bool CommandRegistry::processCommand(const char* line) {
    if (!line || !*line) return false;

    // Check line interceptor first (multiline input modes)
    if (lineInterceptor_) {
        try {
            if (lineInterceptor_(line)) {
                return true;
            }
        } catch (const std::exception& e) {
            LOG_E(TAG, "Line interceptor exception: %s", e.what());
            Console::printf("ERROR: Line interceptor error\r\n");
            return true;  // Consume line to prevent unknown command error
        } catch (...) {
            LOG_E(TAG, "Line interceptor exception (unknown)");
            Console::printf("ERROR: Line interceptor error\r\n");
            return true;
        }
    }

    // ... (rest of command processing)
}
```

## References
- [Command Pattern Error Handling](https://en.cppreference.com/w/cpp/language/try)
- [Serial Command Best Practices](https://esp-idf.readthedocs.io/en/latest/api-reference/system/console.html)
