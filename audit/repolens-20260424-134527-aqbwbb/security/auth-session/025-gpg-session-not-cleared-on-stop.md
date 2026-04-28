---
title: "[MEDIUM] GPG session key not cleared when module stops"
severity: MEDIUM
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
The GPG module's `stop()` method **does not clear the session key** derived from the verified PIN. The session key (`sessionKey`) remains in memory after the module stops, allowing potential access to encrypted DEC private key without re-verifying the PIN.

**File**: `components/mod_gpg/src/GpgModule.cpp:601-604`
```cpp
void GpgModule::stop() {
    core::UsbManager::instance().unregisterInterface(core::UsbHidInterface::Ccid, getName());
    state_ = core::ServiceState::STOPPED;
}
```

The session should be cleared but `gpg_storage_clear_session()` is never called.

**File**: `components/mod_gpg/src/GpgStorage.cpp:79-80`
```cpp
// Session state for verified PIN
bool sessionActive = false;
uint8_t sessionKey[32];  // HKDF-derived key from PIN
```

The session state is stored in a static variable and is only cleared when `gpg_storage_clear_session()` is explicitly called.

## Impact
- **Session Persistence**: After PIN verification, the session key remains valid indefinitely
- **No Automatic Logout**: Stopping the module (e.g., switching to another app) doesn't require re-authentication
- **Memory Exposure**: Session key stays in RAM until module restart or device reset
- **Reduced Security**: User can forget to explicitly logout, leaving session accessible

## Evidence
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

The session is set when PIN is verified, but there's no automatic clearing.

**File**: `components/mod_gpg/src/GpgStorage.cpp:484-487`
```cpp
void gpg_storage_clear_session(void) {
    mbedtls_platform_zeroize(s_storage.sessionKey, sizeof(s_storage.sessionKey));
    s_storage.sessionActive = false;
    LOG_D(TAG, "Session cleared");
}
```

The function exists but is never called from `GpgModule::stop()`.

**File**: `components/mod_gpg/src/GpgModule.cpp:601-604`
```cpp
void GpgModule::stop() {
    core::UsbManager::instance().unregisterInterface(core::UsbHidInterface::Ccid, getName());
    state_ = core::ServiceState::STOPPED;
    // Missing: gpg_storage_clear_session();
}
```

Compare to FIDO2 module which clears PIN verification state:

**File**: `components/mod_fido2/src/fido2.cpp:263-275`
```cpp
bool fido2_factory_reset(void) {
    LOG_W("FIDO2", "Factory reset requested");

    // Delete all credentials
    for (uint8_t slot = 0; slot < FIDO2_MAX_CREDENTIALS; slot++) {
        if (fido2_storage_slot_used(slot)) {
            fido2_storage_delete_credential(slot);
        }
    }

    // Note: g_fido2.pin_verified is NOT cleared here either!
    LOG_I("FIDO2", "Factory reset complete");
    return true;
}
```

## Recommended Fix
Clear the GPG session when the module stops:

**File**: `components/mod_gpg/src/GpgModule.cpp`
```cpp
#include "mod_gpg/GpgStorage.h"  // Already included

void GpgModule::stop() {
    core::UsbManager::instance().unregisterInterface(core::UsbHidInterface::Ccid, getName());
    
    // Clear session key to require re-authentication on restart
    gpg_storage_clear_session();
    
    state_ = core::ServiceState::STOPPED;
}
```

Additionally, consider adding a session timeout:

```cpp
// Add to GpgModule.h or GpgStorage.h
static constexpr uint32_t GPG_SESSION_TIMEOUT_MS = 5 * 60 * 1000;  // 5 minutes

// Add to GpgStorage.cpp static struct
uint32_t sessionLastUsedMs = 0;

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
    return true;
}

// Call from GpgModule::start() or on each command
```

## References
- OWASP Session Management Cheat Sheet - Session Timeout
- NIST SP 800-63B - Session Inactivity Timeout
- CWE-613: Insufficient Session Expiration
- Common GPG smartcard behavior: session clears on deselect

</content>