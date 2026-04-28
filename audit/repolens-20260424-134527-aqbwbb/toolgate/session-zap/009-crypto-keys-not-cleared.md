---
title: "[MEDIUM] AES keys and shared secrets not cleared after use"
severity: MEDIUM
domain: security
lens: toolgate/session-zap
labels:
  - "audit:toolgate/session-zap"
---

## Summary

The FIDO2 CTAP2 module computes AES keys and shared secrets for PIN authentication but does not clear them from stack memory after use. These cryptographic values persist on the stack until overwritten by subsequent function calls.

**Locations:** `components/mod_fido2/src/ctap2.cpp`
- `getPinToken()` - lines 2478-2620
- `getPinTokenWithPermissions()` - lines 2778-2850
- `client_pin_compute_shared_secret()` - lines 2023-2135

**Affected variables:**
- `shared_secret[32]` - AES-256 key derived from ECDH
- `decrypted_pin_hash[16]` - Decrypted PIN hash
- `decrypted[64]` - Temporary buffer for Protocol 1 decryption
- `encrypted_token[48]` - Encrypted pinToken with IV
- `prk[32]` - HKDF intermediate value (Protocol 2)
- `expand_input[32]` - HKDF expand input
- `ecdh_z[32]` - ECDH shared coordinate

## Impact

**Security implications:**

1. **Stack persistence**: All cryptographic values are stack-allocated and remain in memory after function return:
   - AES keys (32 bytes)
   - Decrypted PIN hashes (16 bytes)
   - Encrypted tokens (48 bytes)
   - ECDH intermediate values (32-64 bytes)

2. **Multiple copies**: The same sensitive data may exist in multiple locations:
   - `shared_secret` in `getPinToken()`
   - `shared_secret` in `getPinTokenWithPermissions()`
   - `ecdh_z` in `client_pin_compute_shared_secret()`

3. **Extended attack window**: An attacker with memory access can extract:
   - The AES key to decrypt future pinTokens
   - The decrypted PIN hash for offline brute-force
   - ECDH values for cryptographic analysis

4. **No secure stack unwinding**: Unlike heap memory which can be cleared before free(), stack memory relies on function return, leaving a window where data is accessible.

**Context:** In embedded systems like ESP32-S3 with limited RAM (~320KB SRAM), stack memory is particularly valuable and can be more easily dumped or analyzed.

## Evidence

**File: `components/mod_fido2/src/ctap2.cpp`**

Function scope variable declarations (lines 2478-2510):
```cpp
// Compute shared secret
uint8_t shared_secret[32];
if (!client_pin_compute_shared_secret(platform_key_x, platform_key_y, pin_protocol, shared_secret)) {
    response[0] = CTAP2_ERR_OTHER;
    *response_len = 1;
    return CTAP2_ERR_OTHER;
}

// Decrypt pinHashEnc
uint8_t decrypted_pin_hash[16];
if (pin_protocol == 2 && pin_hash_enc_len == 32) {
    const uint8_t *iv = pin_hash_enc;
    const uint8_t *ciphertext = pin_hash_enc + 16;
    if (!aes_256_cbc_decrypt_iv(shared_secret, iv, ciphertext, 16, decrypted_pin_hash)) {
        LOG_E("PIN", "PIN decryption failed");
        response[0] = CTAP2_ERR_OTHER;
        *response_len = 1;
        return CTAP2_ERR_OTHER;
    }
} else {
    uint8_t decrypted[64];
    if (!aes_256_cbc_decrypt(shared_secret, pin_hash_enc, pin_hash_enc_len, decrypted)) {
        LOG_E("PIN", "PIN decryption failed");
        response[0] = CTAP2_ERR_OTHER;
        *response_len = 1;
        return CTAP2_ERR_OTHER;
    }
    memcpy(decrypted_pin_hash, decrypted, 16);
}
```

AES encryption (lines 2595-2610):
```cpp
// Encrypt pinToken with shared secret
uint8_t encrypted_token[PIN_TOKEN_SIZE + 16];  // Extra space for IV in Protocol 2
size_t encrypted_len;

if (pin_protocol == 2) {
    if (!aes_256_cbc_encrypt_p2(shared_secret, g_client_pin.pin_token, PIN_TOKEN_SIZE, encrypted_token)) {
        response[0] = CTAP2_ERR_OTHER;
        *response_len = 1;
        return CTAP2_ERR_OTHER;
    }
    encrypted_len = PIN_TOKEN_SIZE + 16;
} else {
    if (!aes_256_cbc_encrypt(shared_secret, g_client_pin.pin_token, PIN_TOKEN_SIZE, encrypted_token)) {
        response[0] = CTAP2_ERR_OTHER;
        *response_len = 1;
        return CTAP2_ERR_OTHER;
    }
    encrypted_len = PIN_TOKEN_SIZE;
}
```

HKDF intermediate values (lines 2090-2112):
```cpp
uint8_t prk[32];
uint8_t zero_salt[32] = {0};

// HKDF Extract: PRK = HMAC-SHA256(salt, IKM=Z)
mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                zero_salt, 32, ecdh_z, 32, prk);

// HKDF Expand: OKM = HMAC-SHA256(PRK, info || 0x01)
uint8_t expand_input[32];
size_t info_len = strlen(info);
memcpy(expand_input, info, info_len);
expand_input[info_len] = 0x01;

mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                prk, 32, expand_input, info_len + 1, shared_secret);
```

**No clearing code found:** None of these variables are cleared with `memset()` before the function returns.

## Recommended Fix

Clear all cryptographic variables before function return. Use a cleanup pattern:

**Option 1: Add memset calls before each return**

```cpp
// At the end of getPinToken(), before each return statement:

// Success path (after response is built)
uint8_t shared_secret[32];
// ... use shared_secret ...
// Before returning success:
memset(shared_secret, 0, sizeof(shared_secret));
memset(decrypted_pin_hash, 0, sizeof(decrypted_pin_hash));
memset(encrypted_token, 0, sizeof(encrypted_token));
return CTAP2_OK;

// Error paths
memset(shared_secret, 0, sizeof(shared_secret));
memset(decrypted_pin_hash, 0, sizeof(decrypted_pin_hash));
return CTAP2_ERR_XXX;
```

**Option 2: Use a goto cleanup pattern (recommended)**

```cpp
static int getPinToken(...) {
    uint8_t shared_secret[32];
    uint8_t decrypted_pin_hash[16];
    uint8_t encrypted_token[PIN_TOKEN_SIZE + 16];
    int result = CTAP2_OK;

    // ... main logic ...

cleanup:
    // Clear all sensitive stack variables
    memset(shared_secret, 0, sizeof(shared_secret));
    memset(decrypted_pin_hash, 0, sizeof(decrypted_pin_hash));
    memset(encrypted_token, 0, sizeof(encrypted_token));
    return result;
}

// Then change all returns to:
result = CTAP2_ERR_XXX;
goto cleanup;
```

**Option 3: Use a scope block**

```cpp
static int getPinToken(...) {
    int result = CTAP2_OK;

    {  // Start scope for crypto variables
        uint8_t shared_secret[32];
        uint8_t decrypted_pin_hash[16];
        uint8_t encrypted_token[PIN_TOKEN_SIZE + 16];

        // ... use variables ...

        // Clear before leaving scope
        memset(shared_secret, 0, sizeof(shared_secret));
        memset(decrypted_pin_hash, 0, sizeof(decrypted_pin_hash));
        memset(encrypted_token, 0, sizeof(encrypted_token));
    }  // Variables go out of scope

    return result;
}
```

**Additional recommendations:**

1. Clear `ecdh_z` in `client_pin_compute_shared_secret()`:
```cpp
static bool client_pin_compute_shared_secret(...) {
    uint8_t ecdh_z[32];
    // ... compute ecdh_z ...

cleanup:
    memset(ecdh_z, 0, sizeof(ecdh_z));
    return true;
}
```

2. Clear HKDF intermediate values:
```cpp
uint8_t prk[32];
uint8_t expand_input[32];
// ... use values ...
memset(prk, 0, sizeof(prk));
memset(expand_input, 0, sizeof(expand_input));
```

## References

- CWE-200: Exposure of Sensitive Information to an Unauthorized Actor
- CWE-312: Secure Data Removal
- [FIDO2 CTAP2 Spec - Client PIN Protocol](https://fidoalliance.org/specs/fido-v2.1-rd-20201209/fido-client-to-authenticator-protocol-v2.1-rd-20201209.html#authenticatorClientPIN)
- [OWASP: Cryptographic Failures](https://cheatsheetseries.owasp.org/cheatsheets/Cryptographic_Failures_Cheat_Sheet.html)
- [mbedtls AES API](https://mbed-tls.readthedocs.io/en/latest/api-reference/aes/)
