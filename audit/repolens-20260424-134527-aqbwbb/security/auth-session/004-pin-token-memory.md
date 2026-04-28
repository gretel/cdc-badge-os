---
title: "[MEDIUM] PIN token stored in RAM without clearing"
severity: MEDIUM
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
The FIDO2 `pin_token` is stored in RAM (`g_client_pin.pin_token`) and is not cleared after use or on error. This 32-byte token persists in memory and could be recovered from a RAM dump:

**File**: `components/mod_fido2/src/ctap2.cpp:103-106`
```cpp
static struct {
    bool initialized;

    // ECDH key pair (generated on init, regenerated on reset)
    mbedtls_ecp_keypair ecdh_key;
    bool ecdh_valid;

    // PIN token (regenerated on each getPinToken)
    uint8_t pin_token[PIN_TOKEN_SIZE];  // 32 bytes in RAM
    bool pin_token_valid;

    // Token permissions (CTAP 2.1) - 0 means all permissions (legacy)
    uint8_t token_permissions;
    uint8_t token_rp_id_hash[32];   // RP restriction (if any)
    bool token_rp_id_set;

    // Retry counters
    uint8_t pin_retries;
    uint8_t uv_retries;
} g_client_pin = {};
```

**File**: `components/mod_fido2/src/ctap2.cpp:2586-2588`
```cpp
// PIN correct - reset retries and generate pinToken
g_client_pin.pin_retries = PIN_RETRIES_MAX;
secure_random_fill(g_client_pin.pin_token, PIN_TOKEN_SIZE);
g_client_pin.pin_token_valid = true;
```

The token is generated on successful PIN verification and remains valid until:
- Another PIN verification (regenerates the token)
- System reset (RAM cleared)
- Manual reset (not implemented)

## Impact
- **RAM Dump Attack**: An attacker with physical access could dump RAM and recover the PIN token
- **Session Persistence**: The token remains valid indefinitely, allowing extended session hijacking
- **No Token Rotation**: The token is not rotated after use, increasing exposure window

## Evidence
**File**: `components/mod_fido2/src/ctap2.cpp:104`
```cpp
uint8_t pin_token[PIN_TOKEN_SIZE];  // 32 bytes in RAM, never cleared
```

**File**: `components/mod_fido2/src/ctap2.cpp:2586-2588`
Token is generated but never explicitly cleared after use.

## Recommended Fix
1. **Clear token after use**: Clear `pin_token` immediately after it's used to derive a session token
2. **Add token expiry**: Implement automatic token expiration after a short period (e.g., 5 minutes)
3. **Use secure memory**: Store the token in ESP32's secure memory region if available
4. **Clear on error**: Clear token when PIN verification fails or lockout occurs

Example fix:
```cpp
// After using pin_token, clear it
static void clear_pin_token(void) {
    memset(g_client_pin.pin_token, 0, PIN_TOKEN_SIZE);
    g_client_pin.pin_token_valid = false;
}

// Call after authentication
void fido2_complete_auth(void) {
    // Use token for authentication...
    clear_pin_token();  // Clear after use
}

// Also clear on error
if (!pin_storage_verify_fido2_hash(decrypted_pin_hash)) {
    g_client_pin.pin_retries--;
    clear_pin_token();  // Clear on failure
    ...
}
```

## References
- CTAP 2.1 Specification - pinUvAuthToken
- NIST SP 800-63B - Session Management
- ESP32 Technical Reference Manual - Memory Security
- CWE-311: Missing Encryption of Sensitive Data
