---
title: "[MEDIUM] AUTH command lacks PIN format validation"
severity: MEDIUM
domain: api-design/request-validation
lens: serial-command-interface
labels:
  - "request-validation"
  - "serial-commands"
  - "authentication"
  - "security"
---

## Summary

The `AUTH` command in `components/serial_cmd/src/SerialCmd.cpp` accepts a PIN string from command arguments without validating its format, length, or character set. The PIN is passed directly to `PinManager::verifyBadgePin()` without any preprocessing.

**Location:** `components/serial_cmd/src/SerialCmd.cpp:810-850`

## Impact

- **Security inconsistency**: PIN validation should be consistent between hardware keypad and serial commands
- **Edge cases**: Very long PINs, special characters, or whitespace could cause unexpected behavior
- **User experience**: No clear feedback on PIN format requirements

## Evidence

```cpp
// File: components/serial_cmd/src/SerialCmd.cpp:810-850
#if FEATURE_SECURE_SERIAL
static void cmdAuth(const char* args) {
    auto& pm = core::PinManager::instance();

    if (pm.isBadgeBlocked()) {
        // ... lockout handling ...
        return;
    }

    if (!args || !*args) {
        Console::printf("Usage: AUTH <pin>\r\n");
        Console::printf("Retries: %d\r\n", pm.getBadgeRetries());
        return;
    }

    if (SerialCmd::authenticate(args)) {  // Line 837 - passed directly!
        Console::printf("OK: Authenticated\r\n");
    } else {
        // ... error handling ...
    }
}
```

And in `SerialCmd::authenticate()`:

```cpp
bool SerialCmd::authenticate(const char* pin) {
    auto& pm = core::PinManager::instance();

    if (pm.isBadgeBlocked()) {
        // ...
        return false;
    }

    if (!pin || !*pin) {  // Only checks for empty string
        LOG_W(TAG, "Empty PIN provided");
        return false;
    }

    if (!pm.verifyBadgePin(pin)) {  // Passes raw PIN
        LOG_W(TAG, "Authentication failed, %d retries remaining", pm.getBadgeRetries());
        return false;
    }

    s_authenticated = true;
    s_authTimestamp = esp_timer_get_time();
    LOG_I(TAG, "Authenticated via serial");
    return true;
}
```

Issues:
1. No length validation - PINs could be very long
2. No character validation - special characters allowed
3. No whitespace trimming - leading/trailing spaces could cause authentication failure
4. No validation consistency with hardware keypad (which likely has stricter rules)

## Recommended Fix

Add PIN validation before authentication:

```cpp
static constexpr size_t PIN_MIN_LEN = 4;
static constexpr size_t PIN_MAX_LEN = 32;

#if FEATURE_SECURE_SERIAL
static void cmdAuth(const char* args) {
    auto& pm = core::PinManager::instance();

    if (pm.isBadgeBlocked()) {
        // ... lockout handling ...
        return;
    }

    if (!args || !*args) {
        Console::printf("Usage: AUTH <pin>\r\n");
        Console::printf("Retries: %d\r\n", pm.getBadgeRetries());
        return;
    }

    // Trim leading whitespace
    while (*args && isspace(*args)) args++;
    
    // Check for empty after trim
    if (!*args) {
        Console::printf("ERROR: PIN required\r\n");
        return;
    }
    
    // Validate PIN length
    size_t len = 0;
    const char* p = args;
    while (*p && !isspace(*p)) {
        len++;
        p++;
    }
    
    if (len < PIN_MIN_LEN) {
        Console::printf("ERROR: PIN too short (min %d digits)\r\n", PIN_MIN_LEN);
        return;
    }
    if (len > PIN_MAX_LEN) {
        Console::printf("ERROR: PIN too long (max %d digits)\r\n", PIN_MAX_LEN);
        return;
    }
    
    // Validate PIN contains only digits
    p = args;
    for (size_t i = 0; i < len; i++) {
        if (!isdigit(*p++)) {
            Console::printf("ERROR: PIN must contain digits only\r\n");
            return;
        }
    }
    
    // Copy PIN for authentication
    char pin[PIN_MAX_LEN + 1];
    strncpy(pin, args, len);
    pin[len] = '\0';

    if (SerialCmd::authenticate(pin)) {
        Console::printf("OK: Authenticated\r\n");
    } else {
        // ... error handling ...
    }
}
```

And in `SerialCmd::authenticate()`:

```cpp
bool SerialCmd::authenticate(const char* pin) {
    auto& pm = core::PinManager::instance();

    if (pm.isBadgeBlocked()) {
        return false;
    }

    if (!pin || !*pin) {
        LOG_W(TAG, "Empty PIN provided");
        return false;
    }
    
    // Basic sanity check (length)
    size_t len = strlen(pin);
    if (len < PIN_MIN_LEN || len > PIN_MAX_LEN) {
        LOG_W(TAG, "PIN length invalid: %zu", len);
        return false;
    }

    if (!pm.verifyBadgePin(pin)) {
        LOG_W(TAG, "Authentication failed, %d retries remaining", pm.getBadgeRetries());
        return false;
    }

    s_authenticated = true;
    s_authTimestamp = esp_timer_get_time();
    LOG_I(TAG, "Authenticated via serial");
    return true;
}
```

## References

- CWE-20: Improper Input Validation
- CWE-308: Use of a password store where passwords are not hashed (if PIN is stored)
- NIST SP 800-63B Digital Identity Guidelines (password/PIN composition)
