---
title: "[MEDIUM] Serial session timeout timer updated but never checked"
severity: MEDIUM
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
The serial command authentication timer (`s_authTimestamp`) is updated on each command execution but **never checked** to expire the session after inactivity. The `resetAuthTimer()` function exists but there's no corresponding timeout check.

**File**: `components/serial_cmd/src/SerialCmd.cpp:71-73`
```cpp
static bool s_authenticated = false;
static uint64_t s_authTimestamp = 0;  // Timestamp stored but never checked!
```

**File**: `components/serial_cmd/src/SerialCmd.cpp:84-89`
```cpp
#if FEATURE_SECURE_SERIAL
static void resetAuthTimer() {
    if (s_authenticated) {
        s_authTimestamp = esp_timer_get_time();  // Timer updated
    }
}
#endif
```

**File**: `components/serial_cmd/src/CommandRegistry.cpp:154-157`
```cpp
// Signal successful command execution for timer reset
if (onCommandExecuted_) {
    onCommandExecuted_();  // Calls resetAuthTimer()
}
```

The timer is reset on each command but there's **no code that checks if the session has expired**.

## Impact
- **No Session Expiry**: Once authenticated, the session remains valid indefinitely
- **Unattended Access**: If the device is left unattended, anyone can use the serial session
- **No Idle Timeout**: Unlike typical web sessions, there's no automatic logout after inactivity
- **Security Risk**: Physical access to the device = permanent access until reboot

## Evidence
**File**: `components/serial_cmd/src/SerialCmd.cpp:71-73`
```cpp
static bool s_authenticated = false;
static uint64_t s_authTimestamp = 0;  // Never read!
```

**File**: `components/serial_cmd/src/SerialCmd.cpp:1390-1393`
```cpp
s_authenticated = true;
s_authTimestamp = esp_timer_get_time();  // Set on login
LOG_I(TAG, "Authenticated via serial");
```

**File**: `components/serial_cmd/src/SerialCmd.cpp:1399-1402`
```cpp
void SerialCmd::logout() {
    s_authenticated = false;
    s_authTimestamp = 0;  // Cleared on explicit logout only
    LOG_I(TAG, "Logged out");
}
```

The timestamp is set on login and cleared on logout, but **never checked for expiration**.

**Search for timeout check**:
```
grep -n "SESSION_TIMEOUT\|auth.*timeout\|inactivity" components/serial_cmd/src/*.cpp
# No results found!
```

**File**: `components/serial_cmd/include/serial_cmd/ICommandRegistry.h:73-77`
```cpp
/**
 * Set callback for successful command execution
 * Used to reset auth timer when FEATURE_SECURE_SERIAL is enabled
 */
virtual void setOnCommandExecuted(void (*callback)()) = 0;
```

The comment says "reset auth timer" but there's no corresponding "check auth timer" function.

## Recommended Fix
Add a session timeout constant and check it before executing commands:

```cpp
// Add constant after line 25
static constexpr uint64_t SESSION_TIMEOUT_MS = 10 * 1000;  // 10 minutes

// Add function after resetAuthTimer()
#if FEATURE_SECURE_SERIAL
static bool checkSessionTimeout() {
    if (!s_authenticated) return true;  // Not authenticated, no timeout
    
    uint64_t now = esp_timer_get_time() / 1000;  // Convert to ms
    uint64_t elapsed = now - s_authTimestamp / 1000;
    
    if (elapsed > SESSION_TIMEOUT_MS) {
        LOG_I(TAG, "Session expired after %lu ms", (unsigned long)elapsed);
        s_authenticated = false;
        s_authTimestamp = 0;
        return false;  // Session expired
    }
    return true;  // Session still valid
}
#endif

// Update process() to check timeout
bool SerialCmd::process() {
    int c = Console::getchar();
    if (c < 0) return false;
    
#if FEATURE_SECURE_SERIAL
    // Check session timeout before processing
    if (!checkSessionTimeout()) {
        Console::printf("Session expired. Use AUTH <pin> to login.\r\n");
    }
#endif
    // ... rest of process() ...
}
```

Alternatively, check in `CommandRegistry::processCommand()`:

```cpp
bool CommandRegistry::processCommand(const char* line) {
#if FEATURE_SECURE_SERIAL
    // Check session timeout before executing
    static bool (*timeoutCheck)() = nullptr;
    if (timeoutCheck && !timeoutCheck()) {
        Console::printf("Session expired. Use AUTH <pin> to login.\r\n");
        return true;
    }
#endif
    // ... rest of processCommand() ...
}
```

## References
- OWASP Session Management Cheat Sheet - Session Timeout
- NIST SP 800-63B - Session Management
- CWE-613: Insufficient Session Expiration

</content>