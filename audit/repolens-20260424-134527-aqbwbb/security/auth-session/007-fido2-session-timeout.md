---
title: "[LOW] No session timeout for authenticated FIDO2 operations"
severity: LOW
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
After successful FIDO2 PIN verification via ClientPIN protocol, the authenticated session (`pin_token`) remains valid indefinitely. There is no automatic timeout or session expiration:

**File**: `components/mod_fido2/src/ctap2.cpp:2586-2588`
```cpp
// PIN correct - reset retries and generate pinToken
g_client_pin.pin_retries = PIN_RETRIES_MAX;
secure_random_fill(g_client_pin.pin_token, PIN_TOKEN_SIZE);
g_client_pin.pin_token_valid = true;  // Token valid until cleared
```

**File**: `components/mod_fido2/src/ctap2.cpp:103-106`
```cpp
// PIN token (regenerated on each getPinToken)
uint8_t pin_token[PIN_TOKEN_SIZE];
bool pin_token_valid;
```

The token is only cleared when:
- A new PIN verification occurs (regenerates the token)
- System reset (RAM cleared)
- Manual reset (not implemented)

## Impact
- **Extended Session**: An authenticated session could remain valid for hours or days
- **Physical Access Risk**: If the device is unlocked and left unattended, an attacker can use the session
- **No Automatic Logout**: Unlike web sessions, there's no idle timeout

## Evidence
**File**: `components/mod_fido2/src/ctap2.cpp:2586-2588`
```cpp
g_client_pin.pin_token_valid = true;  // No expiration set
```

**File**: `components/mod_fido2/src/fido2.cpp:192-200`
```cpp
void fido2_set_pin_verified(bool verified) {
    g_fido2.pin_verified = verified;
    if (verified) {
        LOG_I("FIDO2", "PIN verified via ClientPIN - device PIN will be skipped");
    }
}
```

The `pin_verified` flag is also persistent and not time-bounded.

## Recommended Fix
1. **Add token expiry**: Implement automatic token expiration after a configurable period (e.g., 5-10 minutes)
2. **Idle timeout**: Reset token after a period of inactivity
3. **Clear on operation**: Clear token after each authenticated operation (make credential, get assertion)

Example implementation:
```cpp
static struct {
    uint8_t pin_token[PIN_TOKEN_SIZE];
    bool pin_token_valid;
    uint32_t token_expiry_ms;  // New field
} g_client_pin = {};

static void set_token_expiry(uint32_t duration_ms) {
    g_client_pin.token_expiry_ms = esp_timer_get_time() / 1000 + duration_ms;
}

static bool is_token_expired(void) {
    if (!g_client_pin.pin_token_valid) return true;
    uint32_t now = esp_timer_get_time() / 1000;
    return now >= g_client_pin.token_expiry_ms;
}

// In getPinToken handler
if (is_token_expired()) {
    g_client_pin.pin_token_valid = false;
    // Require re-authentication
}
```

## References
- CTAP 2.1 Specification - pinUvAuthToken
- OWASP Session Management Cheat Sheet
- NIST SP 800-63B - Session Management
