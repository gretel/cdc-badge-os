---
title: "[MEDIUM] pinUvAuthParam retry counter not tracked for FIDO2 authentication"
severity: MEDIUM
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
The `uv_retries` counter is defined for FIDO2 ClientPIN protocol but **never decremented** when `pinUvAuthParam` verification fails. This means an attacker can make unlimited attempts to guess the correct HMAC for `pinUvAuthParam` without any lockout mechanism.

**File**: `components/mod_fido2/src/ctap2.cpp:78`
```cpp
#define PIN_UV_RETRIES_MAX      3
```

**File**: `components/mod_fido2/src/ctap2.cpp:114`
```cpp
static struct {
    // ...
    // Retry counters
    uint8_t pin_retries;
    uint8_t uv_retries;  // Defined but never used!
} g_client_pin = {};
```

**File**: `components/mod_fido2/src/ctap2.cpp:1572-1574` (getAssertion)
```cpp
if (memcmp(p->pin_uv_auth_param, expected_hmac, compare_len) != 0) {
    LOG_W("CTAP2", "pinUvAuthParam verification failed");
    return CTAP2_ERR_PIN_AUTH_INVALID;  // uv_retries NOT decremented!
}
```

**File**: `components/mod_fido2/src/ctap2.cpp:941-944` (makeCredential)
```cpp
if (p->pin_uv_auth_param_len < compare_len ||
    memcmp(p->pin_uv_auth_param, expected_hmac, compare_len) != 0) {
    LOG_W("CTAP2", "makeCredential: pinUvAuthParam verification failed");
    return CTAP2_ERR_PIN_AUTH_INVALID;  // uv_retries NOT decremented!
}
```

The `uv_retries` counter is initialized (line 2013, 2877) but never decremented on failed `pinUvAuthParam` verification.

## Impact
- **Unlimited Guessing**: An attacker can try unlimited `pinUvAuthParam` values without lockout
- **Brute-force HMAC**: The 32-byte HMAC can be brute-forced (though computationally expensive, it's theoretically possible)
- **Session Token Guessing**: An attacker could try to guess a valid `pin_token` by testing different HMACs
- **CTAP 2.1 Non-compliance**: The spec expects `uv_retries` to be tracked for `pinUvAuthToken`

## Evidence
**File**: `components/mod_fido2/src/ctap2.cpp:78`
```cpp
#define PIN_UV_RETRIES_MAX      3  // Defined but never used
```

**File**: `components/mod_fido2/src/ctap2.cpp:2013`
```cpp
g_client_pin.uv_retries = PIN_UV_RETRIES_MAX;  // Initialized
```

**File**: `components/mod_fido2/src/ctap2.cpp:2254`
```cpp
cbor_encode_uint(&w, g_client_pin.uv_retries);  // Reported in getRetries
```

The counter is initialized and reported, but **never decremented** on failed `pinUvAuthParam` verification.

**Search for decrement**:
```
grep -n "uv_retries--" components/mod_fido2/src/ctap2.cpp
# No results found!
```

Compare with `pin_retries` which IS decremented:
**File**: `components/mod_fido2/src/ctap2.cpp:2573`
```cpp
if (!pin_storage_verify_fido2_hash(decrypted_pin_hash)) {
    g_client_pin.pin_retries--;  // Decrement on failure
    LOG_W("PIN", "Invalid PIN, retries left: %d", g_client_pin.pin_retries);
```

## Recommended Fix
Add retry counter tracking for `pinUvAuthParam` verification failures:

```cpp
static uint8_t ga_verify_pin_auth(const GetAssertionParams *p, bool *uv_verified) {
    *uv_verified = false;

    if (p->pin_uv_auth_param_len == 0) {
        return CTAP2_OK;
    }

    if (!g_client_pin.pin_token_valid) {
        LOG_W("CTAP2", "pinUvAuthParam provided but no valid pinToken");
        return CTAP2_ERR_PIN_AUTH_INVALID;
    }

    // Compute HMAC-SHA256(pinToken, clientDataHash)
    uint8_t expected_hmac[32];
    mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                    g_client_pin.pin_token, PIN_TOKEN_SIZE,
                    p->client_data_hash, 32,
                    expected_hmac);

    size_t compare_len = (p->pin_uv_auth_protocol == 2) ? 32 : 16;
    if (p->pin_uv_auth_param_len < compare_len) {
        LOG_W("CTAP2", "pinUvAuthParam too short");
        return CTAP2_ERR_PIN_AUTH_INVALID;
    }

    if (memcmp(p->pin_uv_auth_param, expected_hmac, compare_len) != 0) {
        g_client_pin.uv_retries--;  // NEW: Decrement retry counter
        LOG_W("CTAP2", "pinUvAuthParam verification failed, retries left: %d",
              g_client_pin.uv_retries);

        if (g_client_pin.uv_retries == 0) {
            // Clear pin_token on UV retry exhaustion
            g_client_pin.pin_token_valid = false;
            LOG_W("PIN", "UV retries exhausted, pinToken cleared");
            return CTAP2_ERR_PIN_AUTH_INVALID;
        }
        return CTAP2_ERR_PIN_AUTH_INVALID;
    }

    LOG_I("CTAP2", "pinUvAuthParam verified - UV=1");
    *uv_verified = true;
    fido2_set_pin_verified(true);

    // Reset UV retries on success
    g_client_pin.uv_retries = PIN_UV_RETRIES_MAX;

    return CTAP2_OK;
}
```

Similarly for `verify_pin_uv_auth` (makeCredential):
```cpp
static uint8_t verify_pin_uv_auth(const MakeAssertionParams *p) {
    if (p->pin_uv_auth_param_len == 0) {
        return CTAP2_OK;
    }

    if (!g_client_pin.pin_token_valid) {
        g_client_pin.uv_retries--;  // NEW: Decrement on no token
        if (g_client_pin.uv_retries == 0) {
            g_client_pin.pin_token_valid = false;
        }
        return CTAP2_ERR_PIN_AUTH_INVALID;
    }

    // ... HMAC verification ...

    if (memcmp(p->pin_uv_auth_param, expected_hmac, compare_len) != 0) {
        g_client_pin.uv_retries--;  // NEW: Decrement retry counter
        if (g_client_pin.uv_retries == 0) {
            g_client_pin.pin_token_valid = false;
        }
        return CTAP2_ERR_PIN_AUTH_INVALID;
    }

    g_client_pin.uv_retries = PIN_UV_RETRIES_MAX;  // Reset on success
    return CTAP2_OK;
}
```

## References
- CTAP 2.1 Specification - ClientPIN Protocol
- FIDO Alliance CTAP 2.1 - pinUvAuthToken Permission Management
- OWASP Authentication Cheat Sheet - Session Management
- NIST SP 800-63B - Authentication and Session Management

</content>