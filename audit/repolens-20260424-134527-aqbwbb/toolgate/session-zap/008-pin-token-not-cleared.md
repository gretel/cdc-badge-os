---
title: "[MEDIUM] FIDO2 pinToken not cleared after use"
severity: MEDIUM
domain: security
lens: toolgate/session-zap
labels:
  - "audit:toolgate/session-zap"
---

## Summary

The FIDO2 CTAP2 module stores the `pinToken` in the global `g_client_pin` structure but does not clear it after it expires or when the structure is reinitialized. The token persists in memory until overwritten by a new token.

**Location:** `components/mod_fido2/src/ctap2.cpp` lines 95-115 (structure definition), 2585-2610 (token generation)

**Affected code paths:**
1. `getPinToken()` - generates and stores pinToken
2. `getPinTokenWithPermissions()` - generates and stores pinToken
3. `makeCredential()` - uses pinToken for authentication
4. `getAssertion()` - uses pinToken for authentication

## Impact

**Security implications:**

1. **Memory persistence**: The 16-byte pinToken remains in RAM after use, accessible via:
   - Memory dumps
   - Crash core files
   - Cold boot attacks (ESP32-S3 has limited SRAM, making memory forensics easier)

2. **No automatic expiration**: The `pin_token_valid` flag controls logical expiration, but the actual token bytes persist until:
   - A new pinToken is generated (overwrites the old one)
   - The structure is explicitly cleared (only happens on module init)

3. **Extended attack window**: An attacker with physical access can potentially extract the token from memory even after the logical session has ended.

**Context:** The pinToken is the FIDO2 equivalent of a session cookie - it grants authenticated access to FIDO2 operations. While it's encrypted when transmitted to the host, the plaintext version in RAM is the critical secret.

## Evidence

**File: `components/mod_fido2/src/ctap2.cpp`**

Structure definition (lines 95-115):
```cpp
static struct {
    bool initialized;

    // ECDH key pair (generated on init, regenerated on reset)
    mbedtls_ecp_keypair ecdh_key;
    bool ecdh_valid;

    // PIN token (regenerated on each getPinToken)
    uint8_t pin_token[PIN_TOKEN_SIZE];
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

Token generation (lines 2585-2588):
```cpp
// PIN correct - reset retries and generate pinToken
g_client_pin.pin_retries = PIN_RETRIES_MAX;
secure_random_fill(g_client_pin.pin_token, PIN_TOKEN_SIZE);
g_client_pin.pin_token_valid = true;
```

Token usage (lines 932-936, 1544-1548):
```cpp
// Verify HMAC-SHA-256(pinToken, clientDataHash)
uint8_t hmac_result[32];
sha256_hmac(g_client_pin.pin_token, sizeof(g_client_pin.pin_token),
            client_data_hash, 32, hmac_result);
```

**No clearing code found:** The `pin_token` buffer is never explicitly cleared with `memset()` after use. The only initialization is the global `= {}` which zeroes it at startup.

## Recommended Fix

Clear the pinToken after it's no longer needed. Add `memset()` calls to clear sensitive data:

**Option 1: Clear after use in getPinToken functions**

After encrypting and returning the token, clear the plaintext:

```cpp
// After line 2610 (after encrypted token is prepared for response)
// Clear the plaintext pinToken
memset(g_client_pin.pin_token, 0, PIN_TOKEN_SIZE);
```

**Option 2: Clear on logical expiration**

Clear the token when setting `pin_token_valid = false`:

```cpp
// Create a helper function to invalidate the token
static void client_pin_invalidate_token(void) {
    memset(g_client_pin.pin_token, 0, PIN_TOKEN_SIZE);
    g_client_pin.pin_token_valid = false;
    g_client_pin.token_permissions = 0;
    g_client_pin.token_rp_id_set = false;
}

// Use it wherever pin_token_valid is set to false
```

**Option 3: Clear on structure reset**

Update the `clear()` method if one exists for g_client_pin:

```cpp
static void client_pin_reset(void) {
    // Clear ECDH key
    mbedtls_ecp_keypair_free(&g_client_pin.ecdh_key);
    mbedtls_ecp_keypair_init(&g_client_pin.ecdh_key);
    g_client_pin.ecdh_valid = false;

    // Clear pinToken
    memset(g_client_pin.pin_token, 0, PIN_TOKEN_SIZE);
    g_client_pin.pin_token_valid = false;

    // Clear permissions
    g_client_pin.token_permissions = 0;
    memset(g_client_pin.token_rp_id_hash, 0, sizeof(g_client_pin.token_rp_id_hash));
    g_client_pin.token_rp_id_set = false;

    // Reset counters
    g_client_pin.pin_retries = PIN_RETRIES_MAX;
    g_client_pin.uv_retries = UV_RETRIES_MAX;
}
```

**Recommended approach:** Option 2 provides the best balance - the token is cleared as soon as it's logically expired, reducing the window of exposure.

## References

- CWE-200: Exposure of Sensitive Information to an Unauthorized Actor
- CWE-312: Secure Data Removal
- [FIDO2 CTAP2 Spec - Client PIN Protocol](https://fidoalliance.org/specs/fido-v2.1-rd-20201209/fido-client-to-authenticator-protocol-v2.1-rd-20201209.html#authenticatorClientPIN)
- [OWASP: Session Management Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Session_Management_Cheat_Sheet.html)
- ESP32-S3 memory architecture: SRAM is limited (~320KB), making memory dumps more feasible
