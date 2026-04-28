---
title: "[LOW] No error boundary around authentication check callback"
severity: LOW
domain: error-handling
lens: error-boundaries
labels:
  - "error-handling"
  - "serial-commands"
---

## Summary
The `CommandRegistry::processCommand()` function in `components/serial_cmd/src/CommandRegistry.cpp:135-137` and `147-149` calls the authentication check callback without error isolation. A single auth check throwing will break command processing for all subsequent commands.

**Evidence:**
- `components/serial_cmd/src/CommandRegistry.cpp:135-137`:
```cpp
// When secure serial is enabled, block ALL commands except PING and AUTH
// when not authenticated
bool isAllowedWithoutAuth = (strcasecmp(cmdBuf, "PING") == 0 ||
                              strcasecmp(cmdBuf, "AUTH") == 0);
if (!isAllowedWithoutAuth && authCheck_ && !authCheck_()) {  // No error boundary!
    Console::printf("ERROR: Not authenticated. Use AUTH <pin> to login.\r\n");
    return true;  // Command blocked
}
```

- `components/serial_cmd/src/CommandRegistry.cpp:147-149`:
```cpp
// Per-command auth check (for commands that require auth even when
// FEATURE_SECURE_SERIAL is disabled)
if (commands_[i].requiresAuth && authCheck_ && !authCheck_()) {  // No error boundary!
    Console::printf("ERROR: Authentication required. Use AUTH <pin> first.\r\n");
    return true;  // Command found but not executed
}
```

- Usage pattern:
```cpp
getCommandRegistry().setAuthProvider([]() {
    // Authentication check logic that could throw
    return PinManager::instance().isAuthenticated();
});
```

## Impact
- **Command processing blackout**: One crashing auth check stops all command processing
- **Silent failures**: Auth errors not logged
- **Debug difficulty**: Hard to identify if auth check failed
- **User lockout**: May appear as if authentication is required when it's actually an error

## Recommended Fix
Add error boundary around auth check callback:

```cpp
bool CommandRegistry::processCommand(const char* line) {
    if (!line || !*line) return false;

    // ... (existing command name parsing)

#if FEATURE_SECURE_SERIAL
    // Check if PIN is blocked (lockout or retries exhausted)
    auto& pm = cdc::core::PinManager::instance();
    if (pm.isBadgeBlocked()) {
        // ... (existing blocked PIN logic)
    } else {
        // When secure serial is enabled, block ALL commands except PING and AUTH
        // when not authenticated
        bool isAllowedWithoutAuth = (strcasecmp(cmdBuf, "PING") == 0 ||
                                      strcasecmp(cmdBuf, "AUTH") == 0);
        if (!isAllowedWithoutAuth && authCheck_) {
            bool isAuthenticated = false;
            try {
                isAuthenticated = authCheck_();
            } catch (const std::exception& e) {
                LOG_E(TAG, "Auth check exception: %s", e.what());
                Console::printf("ERROR: Auth check failed\r\n");
                return true;  // Consume to prevent unknown command error
            } catch (...) {
                LOG_E(TAG, "Auth check exception (unknown)");
                Console::printf("ERROR: Auth check failed\r\n");
                return true;
            }
            if (!isAuthenticated) {
                Console::printf("ERROR: Not authenticated. Use AUTH <pin> to login.\r\n");
                return true;
            }
        }
    }
#endif

    // Find and execute command
    for (size_t i = 0; i < count_; i++) {
        if (strcasecmp(commands_[i].name, cmdBuf) == 0) {
            // Per-command auth check
            if (commands_[i].requiresAuth && authCheck_) {
                bool isAuthenticated = false;
                try {
                    isAuthenticated = authCheck_();
                } catch (const std::exception& e) {
                    LOG_E(TAG, "Auth check exception: %s", e.what());
                    Console::printf("ERROR: Auth check failed\r\n");
                    return true;
                } catch (...) {
                    LOG_E(TAG, "Auth check exception (unknown)");
                    Console::printf("ERROR: Auth check failed\r\n");
                    return true;
                }
                if (!isAuthenticated) {
                    Console::printf("ERROR: Authentication required.\r\n");
                    return true;
                }
            }
            // ... (rest of command execution)
        }
    }

    // ... (rest of processCommand)
}
```

## References
- [Command Pattern Error Handling](https://en.cppreference.com/w/cpp/language/try)
- [Serial Command Best Practices](https://esp-idf.readthedocs.io/en/latest/api-reference/system/console.html)
