---
title: "[LOW] No error boundary around serial command handler execution"
severity: LOW
domain: error-handling
lens: error-boundaries
labels:
  - "error-handling"
  - "serial-commands"
---

## Summary
The `CommandRegistry::processCommand()` function in `components/serial_cmd/src/CommandRegistry.cpp:151-153` executes serial command handlers without any error isolation. A single command handler throwing an exception or crashing will stop command processing and may leave the serial console in an inconsistent state.

**Evidence:**
- `components/serial_cmd/src/CommandRegistry.cpp:151-153`:
```cpp
if (commands_[i].handler) {
    commands_[i].handler(line);  // No error boundary!
}
```

- Command handlers are registered from modules (e.g., `components/mod_totp/src/TotpModule.cpp:201-250`):
```cpp
static void cmd_totp_list(const char* args) {
    if (!TotpStore::instance().hasSlotRange()) {
        cdc::serial::Console::printf("ERROR: slot map not configured\r\n");
        return;
    }
    // ... parsing and display logic that could throw
}
```

- Called from `components/serial_cmd/src/SerialCmd.cpp` main loop with no recovery:
```cpp
cdc::serial::getCommandRegistry().processCommand(line);
```

## Impact
- **Command blackout**: One crashing handler can stop all serial command processing
- **Console lockup**: User may need to reconnect serial to recover
- **Silent failures**: No logging when a command handler crashes
- **Debug difficulty**: Hard to identify which command failed

## Recommended Fix
Add error boundaries around command handler execution:

```cpp
bool CommandRegistry::processCommand(const char* line) {
    if (!line || !*line) return false;

    // ... (existing command parsing code)

    // Find and execute command
    for (size_t i = 0; i < count_; i++) {
        if (strcasecmp(commands_[i].name, cmdBuf) == 0) {
            if (commands_[i].requiresAuth && authCheck_ && !authCheck_()) {
                Console::printf("ERROR: Authentication required.\r\n");
                return true;
            }
            if (commands_[i].handler) {
                try {
                    commands_[i].handler(line);
                } catch (const std::exception& e) {
                    Console::printf("ERROR: Command '%s' exception: %s\r\n",
                                   cmdBuf, e.what());
                } catch (...) {
                    Console::printf("ERROR: Command '%s' exception (unknown)\r\n", cmdBuf);
                }
            }
            if (onCommandExecuted_) {
                onCommandExecuted_();
            }
            return true;
        }
    }

    Console::printf("ERROR: Unknown command '%s'\r\n", cmdBuf);
    return false;
}
```

## References
- [Command Pattern Error Handling](https://en.cppreference.com/w/cpp/language/try)
- [Serial Command Best Practices](https://esp-idf.readthedocs.io/en/latest/api-reference/system/console.html)
