---
title: "[MEDIUM] GPG session has no idle timeout"
severity: MEDIUM
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
The GPG module session has **no idle timeout mechanism**. The comment in the header says the session should clear "on deselect or timeout" but no timeout is ever checked or enforced.

**File**: `components/mod_gpg/include/mod_gpg/GpgStorage.h:76-80`
```cpp
/**
 * Clear session key (on deselect or timeout)
 */
void gpg_storage_clear_session(void);
```

The comment mentions timeout but there's no code to implement it.

**File**: `components/mod_gpg/src/GpgStorage.cpp:484-487`
```cpp
void gpg_storage_clear_session(void) {
    mbedtls_platform_zeroize(s_storage.sessionKey, sizeof(s_storage.sessionKey));
    s_storage.sessionActive = false;
    LOG_D(TAG, "Session cleared");
}
```

The function exists but is only called when explicitly requested (e.g., when `set_session_pin(nullptr)` is called).

**File**: `components/mod_gpg/src/GpgModule.cpp:601-604`
```cpp
void GpgModule::stop() {
    core::UsbManager::instance().unregisterInterface(core::UsbHidInterface::Ccid, getName());
    state_ = core::ServiceState::STOPPED;
    // No timeout check, no session clear
}
```

No timeout check or session clearing happens on module stop.

## Impact
- **No Automatic Logout**: Session remains valid indefinitely after PIN verification
- **User Inattention Risk**: User can walk away with session still active
- **Memory Exposure**: Session key stays in RAM until explicit logout or reboot
- **Inconsistent with Standards**: Most smartcard implementations have idle timeouts (typically 5-15 minutes)

## Evidence
**File**: `components/mod_gpg/src/GpgStorage.cpp:79-80`
```cpp
// Session state for verified PIN
bool sessionActive = false;
uint8_t sessionKey[32];  // HKDF-derived key from PIN
```

No timestamp field to track last activity.

**File**: `components/mod_gpg/src/GpgStorage.cpp:455-464`
```cpp
void gpg_storage_set_session_pin(const char* pin) {
    if (!pin) {
        gpg_storage_clear_session();
        return;
    }

    if (derive_key_from_pin(pin, s_storage.sessionKey)) {
        s_storage.sessionActive = true;
        LOG_D(TAG, "Session key derived from PIN");
    }
}
```

Session is set but no timeout tracking.

**File**: `components/mod_gpg/src/GpgStorage.cpp:472-478`
```cpp
bool gpg_storage_get_session_key(uint8_t* key_out) {
    if (!s_storage.sessionActive || !key_out) {
        return false;
    }

    memcpy(key_out, s_storage.sessionKey, 32);
    return true;
}
```

No timeout check when retrieving session key.

**Search for timeout constants**:
```
grep -rn "TIMEOUT\|idle\|expire" components/mod_gpg/
# Only found: comment mentioning "timeout" with no implementation
```

## Recommended Fix
Add session timeout tracking and check:

**File**: `components/mod_gpg/src/GpgStorage.cpp`
```cpp
// Add to static struct after sessionKey
uint32_t sessionLastUsedMs = 0;

// Add constant
static constexpr uint32_t GPG_SESSION_TIMEOUT_MS = 5 * 60 * 1000;  // 5 minutes

// Update gpg_storage_set_session_pin
void gpg_storage_set_session_pin(const char* pin) {
    if (!pin) {
        gpg_storage_clear_session();
        return;
    }

    if (derive_key_from_pin(pin, s_storage.sessionKey)) {
        s_storage.sessionActive = true;
        s_storage.sessionLastUsedMs = esp_timer_get_time() / 1000;  // Track start time
        LOG_D(TAG, "Session key derived from PIN");
    }
}

// Add timeout check function
bool gpg_storage_check_session_timeout(void) {
    if (!s_storage.sessionActive) return true;  // No session, no timeout

    uint32_t now = esp_timer_get_time() / 1000;
    uint32_t elapsed = now - s_storage.sessionLastUsedMs;

    if (elapsed > GPG_SESSION_TIMEOUT_MS) {
        LOG_I(TAG, "GPG session timeout after %lu ms", (unsigned long)elapsed);
        gpg_storage_clear_session();
        return false;
    }
    
    // Update last used time
    s_storage.sessionLastUsedMs = now;
    return true;
}

// Update gpg_storage_get_session_key to check timeout
bool gpg_storage_get_session_key(uint8_t* key_out) {
    if (!gpg_storage_check_session_timeout()) {
        return false;  // Session expired
    }

    if (!s_storage.sessionActive || !key_out) {
        return false;
    }

    memcpy(key_out, s_storage.sessionKey, 32);
    return true;
}
```

**File**: `components/mod_gpg/include/mod_gpg/GpgStorage.h`
```cpp
/**
 * Check if session has timed out (returns false if expired)
 * Also updates last-used timestamp
 */
bool gpg_storage_check_session_timeout(void);
```

**File**: `components/mod_gpg/src/GpgModule.cpp`
```cpp
void GpgModule::stop() {
    core::UsbManager::instance().unregisterInterface(core::UsbHidInterface::Ccid, getName());
    
    // Clear session on stop
    gpg_storage_clear_session();
    
    state_ = core::ServiceState::STOPPED;
}
```

## References
- FIDO2 CTAP2 Specification - Session Management
- Common Criteria - Session Timeout Requirements
- NIST SP 800-63B - Session Inactivity Timeout
- Smartcard Best Practices - Idle Timeout (typically 5-15 minutes)

</content>