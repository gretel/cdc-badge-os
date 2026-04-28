---
title: "[HIGH] FIDO2 PIN token and verified state not cleared on factory reset"
severity: HIGH
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
The FIDO2 factory reset function `fido2_factory_reset()` **does not clear the PIN token or the `pin_verified` state**. After a factory reset, the session state remains valid, allowing authenticated operations without re-verifying the PIN.

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

    LOG_I("FIDO2", "Factory reset complete");
    // Missing: g_fido2.pin_verified = false;
    return true;
}
```

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

The `pin_token_valid` flag and `pin_token` are never cleared on factory reset.

**File**: `components/mod_fido2/src/fido2.cpp:28-32`
```cpp
static struct {
    bool initialized;
    fido2_user_presence_cb_t user_presence_cb;
    TaskHandle_t task_handle;
    bool pin_verified;  // PIN was verified via ClientPIN protocol
} g_fido2 = {};
```

The `pin_verified` flag is set but never cleared on reset.

## Impact
- **Authentication Bypass**: After factory reset, the PIN token remains valid
- **Session Persistence**: `pin_verified` state survives factory reset
- **Security Inconsistency**: Credentials are deleted but authentication state remains
- **Unexpected Behavior**: User expects complete reset but can still use old session

## Evidence
**File**: `components/mod_fido2/src/fido2.cpp:191-197`
```cpp
void fido2_set_pin_verified(bool verified) {
    g_fido2.pin_verified = verified;
    if (verified) {
        LOG_I("FIDO2", "PIN verified via ClientPIN - device PIN will be skipped");
    }
}
```

**File**: `components/mod_fido2/src/fido2.cpp:200-205`
```cpp
bool fido2_is_pin_verified(void) {
    return g_fido2.pin_verified;
}
```

The PIN verified state is checked but never cleared on reset.

**File**: `components/mod_fido2/src/ctap2.cpp:2585-2589`
```cpp
// PIN correct - reset retries and generate pinToken
g_client_pin.pin_retries = PIN_RETRIES_MAX;
secure_random_fill(g_client_pin.pin_token, PIN_TOKEN_SIZE);
g_client_pin.pin_token_valid = true;
```

The PIN token is generated when PIN is verified but never cleared.

**File**: `components/mod_fido2/src/ctap2.cpp:1125-1128`
```cpp
flags |= 0x04;  // UV=1 when PIN was verified
LOG_D("CTAP2", "UV flag set: pin_token_valid=%d, pin_verified=%d",
      g_client_pin.pin_token_valid, fido2_is_pin_verified());
```

Both states are used together to determine authentication state.

**File**: `components/mod_fido2/src/ctap2.cpp:2951-2963`
```cpp
uint8_t ctap2_reset(uint8_t *response, uint16_t *response_len) {
    // For now, just perform the reset
    if (!fido2_factory_reset()) {
        return CTAP2_ERR_UNEXPECTED_COMMAND;
    }

    LOG_I("CTAP2", "Factory reset complete");
    // Missing: Clear g_fido2.pin_verified and g_client_pin.pin_token_valid
    return CTAP2_OK;
}
```

The reset command calls factory reset but doesn't clear session state.

## Recommended Fix
Clear all session state in `fido2_factory_reset()`:

**File**: `components/mod_fido2/src/fido2.cpp`
```cpp
bool fido2_factory_reset(void) {
    LOG_W("FIDO2", "Factory reset requested");

    // Delete all credentials
    for (uint8_t slot = 0; slot < FIDO2_MAX_CREDENTIALS; slot++) {
        if (fido2_storage_slot_used(slot)) {
            fido2_storage_delete_credential(slot);
        }
    }

    // Clear PIN verified state
    g_fido2.pin_verified = false;

    // Clear PIN token (need to include ctap2.h or forward declare)
    extern void ctap2_clear_pin_session(void);
    ctap2_clear_pin_session();

    LOG_I("FIDO2", "Factory reset complete");
    return true;
}
```

**File**: `components/mod_fido2/src/ctap2.cpp`
```cpp
// Add new function after g_client_pin declaration
void ctap2_clear_pin_session(void) {
    // Clear PIN token
    mbedtls_platform_zeroize(g_client_pin.pin_token, sizeof(g_client_pin.pin_token));
    g_client_pin.pin_token_valid = false;
    
    // Clear permission flags
    g_client_pin.token_permissions = 0;
    g_client_pin.token_rp_id_set = false;
    
    // Reset retry counters
    g_client_pin.pin_retries = PIN_RETRIES_MAX;
    g_client_pin.uv_retries = PIN_UV_RETRIES_MAX;
    
    LOG_I("CTAP2", "PIN session cleared");
}

// Update ctap2_reset to use it
uint8_t ctap2_reset(uint8_t *response, uint16_t *response_len) {
    if (!fido2_factory_reset()) {
        return CTAP2_ERR_UNEXPECTED_COMMAND;
    }

    // Clear PIN session state
    ctap2_clear_pin_session();

    LOG_I("CTAP2", "Factory reset complete");
    return CTAP2_OK;
}
```

Alternatively, clear session state directly in `fido2_factory_reset()`:

```cpp
bool fido2_factory_reset(void) {
    LOG_W("FIDO2", "Factory reset requested");

    // Delete all credentials
    for (uint8_t slot = 0; slot < FIDO2_MAX_CREDENTIALS; slot++) {
        if (fido2_storage_slot_used(slot)) {
            fido2_storage_delete_credential(slot);
        }
    }

    // Clear PIN verified state
    g_fido2.pin_verified = false;

    // Clear PIN token (include ctap2.h or access extern state)
    extern struct {
        uint8_t pin_token[32];
        bool pin_token_valid;
        uint8_t token_permissions;
        bool token_rp_id_set;
        uint8_t pin_retries;
        uint8_t uv_retries;
    } g_client_pin;
    
    mbedtls_platform_zeroize(g_client_pin.pin_token, sizeof(g_client_pin.pin_token));
    g_client_pin.pin_token_valid = false;
    g_client_pin.token_permissions = 0;
    g_client_pin.token_rp_id_set = false;
    g_client_pin.pin_retries = 8;  // PIN_RETRIES_MAX
    g_client_pin.uv_retries = 3;   // PIN_UV_RETRIES_MAX

    LOG_I("FIDO2", "Factory reset complete");
    return true;
}
```

## References
- FIDO CTAP2 Specification - Reset Command
- OWASP Session Management - Session Invalidation
- CWE-613: Insufficient Session Expiration
- FIDO2 Level 2 conformance: Reset must clear all session state

</content>