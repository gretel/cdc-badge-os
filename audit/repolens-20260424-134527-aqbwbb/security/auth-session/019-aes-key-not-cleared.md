---
title: "[LOW] AES key material not cleared after encryption"
severity: LOW
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
The AES key context (`mbedtls_aes_context aes`) is freed after encryption/decryption, but the key material itself may remain in the context structure. The `mbedtls_aes_free()` function may not clear all key material.

**File**: `components/mod_fido2/src/ctap2.cpp:2186-2202`
```cpp
static bool aes_256_cbc_encrypt(const uint8_t *key, const uint8_t *input,
                                 size_t len, uint8_t *output) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    uint8_t iv[16] = {0};

    int ret = mbedtls_aes_setkey_enc(&aes, key, 256);
    if (ret != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }

    ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, len, iv, input, output);
    mbedtls_aes_free(&aes);  // May not clear key material
    return ret == 0;
}
```

**File**: `components/mod_fido2/src/ctap2.cpp:2212-2234`
```cpp
static bool aes_256_cbc_encrypt_p2(const uint8_t *key, const uint8_t *input,
                                    size_t len, uint8_t *output) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    uint8_t iv[16];
    secure_random_fill(iv, 16);

    memcpy(output, iv, 16);

    int ret = mbedtls_aes_setkey_enc(&aes, key, 256);
    if (ret != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }

    ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, len, iv, input, output + 16);
    mbedtls_aes_free(&aes);  // May not clear key material
    return ret == 0;
}
```

The `shared_secret` is passed to `mbedtls_aes_setkey_enc()` and stored in the AES context.

## Impact
- **Key Material Leakage**: The 32-byte AES key remains in the AES context structure after `mbedtls_aes_free()`
- **Stack Memory Leak**: The AES context (which contains the expanded key) is on the stack and may not be cleared
- **RAM Dump Attack**: An attacker could recover the AES key from stack memory

## Evidence
**File**: `components/mod_fido2/src/ctap2.cpp:2186-2202`
```cpp
int ret = mbedtls_aes_setkey_enc(&aes, key, 256);  // Key stored in aes context
// ... encryption ...
mbedtls_aes_free(&aes);  // May not clear all key material
```

**File**: `components/mod_fido2/src/ctap2.cpp:2597-2605**
```cpp
if (!aes_256_cbc_encrypt_p2(shared_secret, g_client_pin.pin_token, PIN_TOKEN_SIZE, encrypted_token)) {
```

The `shared_secret` (32 bytes) is used as the AES key.

## Recommended Fix
Explicitly clear key material after use:

```cpp
static bool aes_256_cbc_encrypt(const uint8_t *key, const uint8_t *input,
                                 size_t len, uint8_t *output) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    uint8_t iv[16] = {0};

    int ret = mbedtls_aes_setkey_enc(&aes, key, 256);
    if (ret != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }

    ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, len, iv, input, output);
    
    // Clear the AES context (expanded key)
    mbedtls_aes_free(&aes);
    
    // Also clear the key variable
    uint8_t key_copy[32];
    memcpy(key_copy, key, 32);  // Copy to local variable
    memset(key_copy, 0, 32);    // Clear local copy
    
    return ret == 0;
}

// For Protocol 2
static bool aes_256_cbc_encrypt_p2(const uint8_t *key, const uint8_t *input,
                                    size_t len, uint8_t *output) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    uint8_t iv[16];
    secure_random_fill(iv, 16);

    memcpy(output, iv, 16);

    int ret = mbedtls_aes_setkey_enc(&aes, key, 256);
    if (ret != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }

    ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, len, iv, input, output + 16);
    
    // Clear AES context
    mbedtls_aes_free(&aes);
    
    return ret == 0;
}
```

Also clear the IV and other local variables:

```cpp
static bool aes_256_cbc_encrypt(const uint8_t *key, const uint8_t *input,
                                 size_t len, uint8_t *output) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    uint8_t iv[16] = {0};

    int ret = mbedtls_aes_setkey_enc(&aes, key, 256);
    if (ret != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }

    ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, len, iv, input, output);
    
    // Clear variables
    mbedtls_aes_free(&aes);
    memset(iv, 0, sizeof(iv));
    
    return ret == 0;
}
```

## References
- Mbed TLS Documentation - `mbedtls_aes_free()`
- CWE-459: Incomplete Cleanup of Sensitive Data
- NIST SP 800-132 - Password-Based Key Derivation

</content>