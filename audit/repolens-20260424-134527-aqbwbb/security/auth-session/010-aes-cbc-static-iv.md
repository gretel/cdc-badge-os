---
title: "[MEDIUM] AES-CBC static IV in Protocol 1 enables replay attacks"
severity: MEDIUM
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
FIDO2 PIN Protocol 1 uses a static IV of all zeros for AES-CBC encryption of the pinToken. This makes the encryption deterministic - the same PIN token will always produce the same ciphertext, enabling replay attacks.

**File**: `components/mod_fido2/src/ctap2.cpp:2186-2202`
```cpp
static bool aes_256_cbc_encrypt(const uint8_t *key, const uint8_t *input,
                                 size_t len, uint8_t *output) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    uint8_t iv[16] = {0};  // IV is all zeros for PIN protocol 1

    int ret = mbedtls_aes_setkey_enc(&aes, key, 256);
    if (ret != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }

    ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, len, iv, input, output);
    mbedtls_aes_free(&aes);
    return ret == 0;
}
```

**File**: `components/mod_fido2/src/ctap2.cpp:2605-2612`
```cpp
if (!aes_256_cbc_encrypt(shared_secret, g_client_pin.pin_token, PIN_TOKEN_SIZE, encrypted_token)) {
    response[0] = CTAP2_ERR_OTHER;
    *response_len = 1;
    return CTAP2_ERR_OTHER;
}
encrypted_len = PIN_TOKEN_SIZE;
```

Protocol 2 uses a random IV, but Protocol 1 (for legacy compatibility) uses a fixed IV.

## Impact
- **Replay Attack**: An attacker who captures an encrypted pinToken can replay it later, and it will decrypt to the same value
- **Deterministic Encryption**: The same shared key and pinToken always produce the same ciphertext
- **Protocol 1 Weakness**: Protocol 1 is vulnerable even though Protocol 2 uses random IVs

## Evidence
**File**: `components/mod_fido2/src/ctap2.cpp:2191`
```cpp
uint8_t iv[16] = {0};  // IV is all zeros for PIN protocol 1
```

**File**: `components/mod_fido2/src/ctap2.cpp:2212-2234`
Protocol 2 uses random IV:
```cpp
static bool aes_256_cbc_encrypt_p2(const uint8_t *key, const uint8_t *input,
                                    size_t len, uint8_t *output) {
    // Generate random IV using TROPIC01 TRNG
    uint8_t iv[16];
    secure_random_fill(iv, 16);
    // ...
}
```

But Protocol 1 (used for backward compatibility) uses static IV.

## Recommended Fix
Always use a random IV, even for Protocol 1. The recipient can extract the IV from the ciphertext since it's prepended:

```cpp
static bool aes_256_cbc_encrypt_any(const uint8_t *key, const uint8_t *input,
                                     size_t len, uint8_t *output, size_t *out_len) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    // Generate random IV
    uint8_t iv[16];
    secure_random_fill(iv, 16);

    // Copy IV to output (first 16 bytes)
    memcpy(output, iv, 16);

    int ret = mbedtls_aes_setkey_enc(&aes, key, 256);
    if (ret != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }

    ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, len, iv, input, output + 16);
    mbedtls_aes_free(&aes);
    
    *out_len = 16 + len;  // IV + ciphertext
    return ret == 0;
}
```

Update decryption to extract the IV:

```cpp
static bool aes_256_cbc_decrypt_any(const uint8_t *key, const uint8_t *input,
                                     size_t len, uint8_t *output) {
    // First 16 bytes are the IV
    const uint8_t *iv = input;
    const uint8_t *ciphertext = input + 16;
    
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    int ret = mbedtls_aes_setkey_dec(&aes, key, 256);
    if (ret != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }

    ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, len - 16, iv, ciphertext, output);
    mbedtls_aes_free(&aes);
    return ret == 0;
}
```

## References
- NIST SP 800-38A - CBC Mode
- CTAP 2.1 Specification - PIN Protocol
- CWE-323: Reusing a Nonce, Key Pair in Encryption

</content>