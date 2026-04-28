---
title: "[MEDIUM] FIDO2 pin_verified flag persists across reboots without timeout"
severity: MEDIUM
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
The `pin_verified` flag in the FIDO2 module is stored in RAM and never explicitly cleared. Once set via `fido2_set_pin_verified(true)`, it remains true indefinitely until a system reset. This allows authenticated operations to skip PIN verification even after significant time has passed.

**File**: `components/mod_fido2/src/fido2.cpp:192-197`
```cpp
void fido2_set_pin_verified(bool verified) {
    g_fido2.pin_verified = verified;
    if (verified) {
        LOG_I("FIDO2", "PIN verified via ClientPIN - device PIN will be skipped");
    }
}
```

**File**: `components/mod_fido2/src/fido2.cpp:28-33`
```cpp
static struct {
    bool initialized;
    fido2_user_presence_cb_t user_presence_cb;
    TaskHandle_t task_handle;
    bool pin_verified;  // PIN was verified via ClientPIN protocol
} g_fido2 = {};
```

The flag is used to skip PIN verification in `makeCredential` and `getAssertion`:

**File**: `components/mod_fido2/src/ctap2.cpp:256-262`
```cpp
bool pin_verified = fido2_is_pin_verified();
LOG_I("CTAP2", "Building authData: pin_verified=%d, cred_protect=%u", pin_verified, cred_protect);
if (pin_verified) {
    auth_data_flags |= FIDO2_AUTH_DATA_FLAG_UV;  // User verified
}
```

## Impact
- **Extended Session**: After verifying PIN once, all subsequent FIDO2 operations skip PIN verification indefinitely
- **No Idle Timeout**: Even if the device is left unattended for hours, the `pin_verified` flag remains true
- **Physical Access Risk**: An attacker with physical access can use the device without PIN if it was recently unlocked
- **UV Flag Misrepresentation**: The UV (User Verified) flag in FIDO2 authentication data suggests recent verification, but it could have been set hours ago

## Evidence
**File**: `components/mod_fido2/src/fido2.cpp:192-197`
The `pin_verified` flag is set but never cleared automatically:
```cpp
void fido2_set_pin_verified(bool verified) {
    g_fido2.pin_verified = verified;  // No expiry, no timeout
}
```

**File**: `components/mod_fido2/src/Fido2Ui.cpp:409`
```cpp
fido2_set_pin_verified(false);  // Only cleared manually in UI
```

The flag is only cleared when explicitly called from the UI layer, not automatically.

## Recommended Fix
Add an automatic timeout for the `pin_verified` flag:

```cpp
static struct {
    bool pin_verified;
    uint32_t pin_verified_time_ms;  // Timestamp when verified
} g_fido2 = {};

#define PIN_VERIFIED_TIMEOUT_MS  (5 * 60 * 1000)  // 5 minutes

static void fido2_set_pin_verified(bool verified) {
    g_fido2.pin_verified = verified;
    if (verified) {
        g_fido2.pin_verified_time_ms = esp_timer_get_time() / 1000;
        LOG_I("FIDO2", "PIN verified via ClientPIN - valid for 5 minutes");
    }
}

static bool fido2_is_pin_verified(void) {
    if (!g_fido2.pin_verified) return false;
    
    uint32_t now = esp_timer_get_time() / 1000;
    if (now - g_fido2.pin_verified_time_ms > PIN_VERIFIED_TIMEOUT_MS) {
        g_fido2.pin_verified = false;
        LOG_W("FIDO2", "PIN verification expired");
        return false;
    }
    return true;
}
```

Alternatively, clear the flag after each FIDO2 operation:

```cpp
// In makeCredential and getAssertion handlers
uint8_t ctap2_make_credential(...) {
    // ... existing code ...
    bool was_verified = fido2_is_pin_verified();
    
    // ... do the operation ...
    
    fido2_set_pin_verified(false);  // Clear after use
    return CTAP2_OK;
}
```

## References
- FIDO2 CTAP 2.1 Specification - UV (User Verified) Flag
- OWASP Session Management Cheat Sheet - Session Timeout
- NIST SP 800-63B - Authentication and Session Management

</content>