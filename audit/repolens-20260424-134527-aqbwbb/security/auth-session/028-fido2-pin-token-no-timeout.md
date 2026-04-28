---
title: "[MEDIUM] FIDO2 PIN token has no idle timeout"
severity: MEDIUM
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
The FIDO2 CTAP2 implementation has **no idle timeout for the PIN token**. Once a PIN is verified via ClientPIN protocol, the `pin_token` remains valid indefinitely, allowing authenticated operations without re-entering the PIN.

**File**: `components/mod_fido2/src/ctap2.cpp:98-116`
```cpp
static struct {
    // ECDH key pair (generated on init, regenerated on reset)
    mbedtls_ecp_keypair ecdh_key;
    bool ecdh_valid;

    // PIN token (regenerated on each getPinToken)
    uint8_t pin_token[PIN_TOKEN_SIZE];
    bool pin_token_valid;

    // Permission flags for pinUvAuthToken
    uint8_t token_permissions;
    bool token_rp_id_set;
    char token_rp_id[64];

    // Retry counters
    uint8_t pin_retries;
    uint8_t uv_retries;
} g_client_pin = {};
```

No timestamp field to track when the PIN token was generated.

**File**: `components/mod_fido2/src/ctap2.cpp:2585-2589`
```cpp
// PIN correct - reset retries and generate pinToken
g_client_pin.pin_retries = PIN_RETRIES_MAX;
secure_random_fill(g_client_pin.pin_token, PIN_TOKEN_SIZE);
g_client_pin.pin_token_valid = true;
```

PIN token is generated but never expires.

**File**: `components/mod_fido2/src/ctap2.cpp:921-930`
```cpp
static uint8_t verify_pin_uv_auth(const MakeCredentialParams *p) {
    LOG_D("CTAP2", "pinToken valid=%d", g_client_pin.pin_token_valid);

    if (p->pin_uv_auth_param_len == 0) {
        return 0;  // No auth param, use UP (user presence)
    }

    if (!g_client_pin.pin_token_valid) {
        LOG_W("CTAP2", "makeCredential: pinUvAuthParam provided but no valid pinToken");
        return CTAP2_ERR_PIN_AUTH_INVALID;
    }
    // ... verify HMAC ...
}
```

The `pin_token_valid` flag is checked but never cleared due to timeout.

## Impact
- **No Automatic PIN Re-entry**: User can leave device unattended and session remains valid
- **Extended Attack Window**: PIN token valid indefinitely until reset or explicit logout
- **Inconsistent with Security Best Practices**: Most FIDO authenticators have session timeouts
- **Memory Exposure**: PIN token stays in RAM until device reset

## Evidence
**File**: `components/mod_fido2/src/ctap2.cpp:1537-1550`
```cpp
static uint8_t get_pin_uv_auth_token_with_permission(
    uint8_t permissions,
    const char *rp_id,
    uint8_t *response,
    uint16_t *response_len) {
    // Check if we already have a valid pinToken
    if (!g_client_pin.pin_token_valid) {
        LOG_W("CTAP2", "No valid pinToken for pinUvAuthToken");
        return CTAP2_ERR_PIN_AUTH_INVALID;
    }
    // ... generate token using pin_token ...
}
```

The function checks if `pin_token_valid` but never checks for timeout.

**File**: `components/mod_fido2/src/ctap2.cpp:1125-1128`
```cpp
flags |= 0x04;  // UV=1 when PIN was verified
LOG_D("CTAP2", "UV flag set: pin_token_valid=%d, pin_verified=%d",
      g_client_pin.pin_token_valid, fido2_is_pin_verified());
```

Both `pin_token_valid` and `pin_verified` are used together but neither has timeout.

**File**: `components/mod_fido2/src/fido2.cpp:191-197`
```cpp
void fido2_set_pin_verified(bool verified) {
    g_fido2.pin_verified = verified;
    if (verified) {
        LOG_I("FIDO2", "PIN verified via ClientPIN - device PIN will be skipped");
    }
}
```

The `pin_verified` state is also never cleared.

**Search for timeout constants**:
```
grep -rn "PIN.*TIMEOUT\|pin.*timeout\|SESSION.*TIMEOUT" components/mod_fido2/
# No results found - no timeout mechanism
```

## Recommended Fix
Add PIN token timeout tracking:

**File**: `components/mod_fido2/src/ctap2.cpp`
```cpp
// Add constant after PIN_TOKEN_SIZE definition
#define PIN_TOKEN_TIMEOUT_MS        (5 * 60 * 1000)  // 5 minutes

// Add to g_client_pin struct
static struct {
    // ... existing fields ...
    uint32_t pin_token_created_ms;  // Track when token was created
} g_client_pin = {};

// Add timeout check function
static bool check_pin_token_timeout(void) {
    if (!g_client_pin.pin_token_valid) return true;  // No token, no timeout

    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t elapsed = now - g_client_pin.pin_token_created_ms;

    if (elapsed > PIN_TOKEN_TIMEOUT_MS) {
        LOG_I("CTAP2", "PIN token timeout after %lu ms", (unsigned long)elapsed);
        g_client_pin.pin_token_valid = false;
        mbedtls_platform_zeroize(g_client_pin.pin_token, PIN_TOKEN_SIZE);
        return false;
    }
    return true;
}

// Update get_pin_token to set timestamp
static uint8_t client_pin_get_pin_token(const uint8_t *params, uint16_t params_len,
                                        uint8_t *response, uint16_t *response_len) {
    // ... existing code ...
    
    // Generate pinToken
    secure_random_fill(g_client_pin.pin_token, PIN_TOKEN_SIZE);
    g_client_pin.pin_token_valid = true;
    g_client_pin.pin_token_created_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;  // Add timestamp
    
    // ... rest of function ...
}

// Update verify_pin_uv_auth to check timeout
static uint8_t verify_pin_uv_auth(const MakeCredentialParams *p) {
    // Check timeout first
    if (!check_pin_token_timeout()) {
        LOG_W("CTAP2", "PIN token expired");
        return CTAP2_ERR_PIN_TOKEN_EXPIRED;  // Use existing error code
    }
    
    // ... rest of existing code ...
}

// Update get_pin_uv_auth_token_with_permission
static uint8_t get_pin_uv_auth_token_with_permission(
    uint8_t permissions,
    const char *rp_id,
    uint8_t *response,
    uint16_t *response_len) {
    // Check timeout first
    if (!check_pin_token_timeout()) {
        return CTAP2_ERR_PIN_TOKEN_EXPIRED;
    }
    
    // ... rest of existing code ...
}
```

**File**: `components/mod_fido2/src/fido2.cpp`
```cpp
// Update fido2_set_pin_verified to track time
void fido2_set_pin_verified(bool verified) {
    g_fido2.pin_verified = verified;
    if (verified) {
        LOG_I("FIDO2", "PIN verified via ClientPIN - device PIN will be skipped");
    }
}

// Add timeout check
bool fido2_is_pin_verified_timed(void) {
    if (!g_fido2.pin_verified) return false;
    
    // Check if too much time has passed since verification
    // (implement similar timestamp tracking)
    return true;
}
```

## References
- FIDO CTAP2 Specification - ClientPIN Protocol
- FIDO2 Level 2 Conformance - Session Timeout Requirements
- NIST SP 800-63B - Session Inactivity Timeout
- OWASP Session Management - Session Timeout

</content>