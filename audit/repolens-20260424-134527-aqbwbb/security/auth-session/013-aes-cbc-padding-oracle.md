---
title: "[LOW] AES-CBC decryption without padding validation"
severity: LOW
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
The AES-CBC decryption functions do not explicitly validate PKCS7 padding. This could allow an attacker to submit malformed ciphertext and potentially exploit padding oracle attacks to decrypt data without the key.

**File**: `components/mod_fido2/src/ctap2.cpp:2130-2178`
```cpp
static bool aes_256_cbc_decrypt(const uint8_t *key, const uint8_t *input,
                                 size_t len, uint8_t *output) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    uint8_t iv[16] = {0};  // IV is all zeros for PIN protocol 1

    int ret = mbedtls_aes_setkey_dec(&aes, key, 256);
    if (ret != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }

    ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, len, iv, input, output);
    mbedtls_aes_free(&aes);
    return ret == 0;
}
```

**File**: `components/mod_fido2/src/ctap2.cpp:2787-2805`
```cpp
if (pin_protocol == 2 && pin_hash_enc_len == 32) {
    const uint8_t *iv = pin_hash_enc;
    const uint8_t *ciphertext = pin_hash_enc + 16;
    if (!aes_256_cbc_decrypt_iv(shared_secret, iv, ciphertext, 16, decrypted_pin_hash)) {
        LOG_E("PIN", "PIN decryption failed");
        response[0] = CTAP2_ERR_OTHER;
        *response_len = 1;
        return CTAP2_ERR_OTHER;
    }
}
```

The decryption uses `mbedtls_aes_crypt_cbc()` which doesn't validate padding by default.

## Impact
- **Padding Oracle**: An attacker could submit many different ciphertexts and observe the error responses to gradually decrypt the original data
- **PIN Hash Recovery**: The encrypted pinHash could be decrypted through padding oracle attacks
- **Information Leakage**: Different error responses for padding vs. other errors could leak information

## Evidence
**File**: `components/mod_fido2/src/ctap2.cpp:2149`
```cpp
ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, len, iv, input, output);
```

The `mbedtls_aes_crypt_cbc()` function decrypts data but doesn't validate PKCS7 padding.

**File**: `components/mod_fido2/src/ctap2.cpp:2790-2795`
```cpp
if (!aes_256_cbc_decrypt_iv(shared_secret, iv, ciphertext, 16, decrypted_pin_hash)) {
    LOG_E("PIN", "PIN decryption failed");
    response[0] = CTAP2_ERR_OTHER;
```

All decryption failures return the same `CTAP2_ERR_OTHER` error, but the timing difference could still leak information.

## Recommended Fix
Validate PKCS7 padding after decryption:

```cpp
static bool aes_256_cbc_decrypt_padded(const uint8_t *key, const uint8_t *input,
                                        size_t len, uint8_t *output, size_t *out_len) {
    if (len % 16 != 0 || len == 0) {
        return false;  // Invalid length
    }

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    uint8_t iv[16] = {0};
    uint8_t last_block[16];

    int ret = mbedtls_aes_setkey_dec(&aes, key, 256);
    if (ret != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }

    // Decrypt all blocks except the last
    for (size_t i = 0; i < len - 16; i += 16) {
        mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, 16, iv, input + i, output + i);
    }

    // Decrypt last block
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, 16, iv, input + len - 16, last_block);

    // Validate PKCS7 padding
    uint8_t padding = last_block[15];
    if (padding == 0 || padding > 16) {
        mbedtls_aes_free(&aes);
        return false;  // Invalid padding
    }

    // Check all padding bytes
    for (int i = 15; i >= 15 - padding + 1; i--) {
        if (last_block[i] != padding) {
            mbedtls_aes_free(&aes);
            return false;  // Invalid padding
        }
    }

    // Copy decrypted data (excluding padding)
    memcpy(output + len - 16, last_block, 16 - padding);
    *out_len = len - padding;

    mbedtls_aes_free(&aes);
    return true;
}
```

Also ensure all decryption errors return the same response to prevent timing attacks:

```cpp
// In client_pin_get_pin_token
if (!aes_256_cbc_decrypt(shared_secret, pin_hash_enc, pin_hash_enc_len, decrypted)) {
    // Always return same error
    response[0] = CTAP2_ERR_PIN_INVALID;  // Not CTAP2_ERR_OTHER
    *response_len = 1;
    return response[0];
}
```

## References
- RFC 5649 - AES Key Wrap with Padding
- Mbed TLS Documentation - `mbedtls_aes_crypt_cbc()`
- CWE-693: Protection Mechanism Failure
- Padding Oracle Attacks (CVE-2002-2538, CVE-2016-2171)

</content>