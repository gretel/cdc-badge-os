---
title: "[HIGH] AES-CBC encryption without authentication for PIN tokens"
severity: HIGH
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
The FIDO2 ClientPIN protocol uses AES-CBC (Cipher Block Chaining) to encrypt PIN tokens without any authentication tag. This allows an attacker to perform bit-flipping attacks on the encrypted token, potentially modifying the plaintext without knowing the key.

**File**: `components/mod_fido2/src/ctap2.cpp:2186-2234`

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

**File**: `components/mod_fido2/src/ctap2.cpp:2597-2612`
```cpp
if (!aes_256_cbc_encrypt_p2(shared_secret, g_client_pin.pin_token, PIN_TOKEN_SIZE, encrypted_token)) {
```

Protocol 1 uses a static IV of all zeros, and neither protocol includes an authentication tag (like in AES-GCM).

## Impact
- **Bit-flipping Attack**: An attacker who intercepts the encrypted pinToken can modify specific bits in the ciphertext, causing predictable changes to the decrypted plaintext
- **Token Forgery**: Without authentication, there's no way to detect if the token was tampered with
- **Session Hijacking**: An attacker could potentially forge a valid-looking token by manipulating ciphertext bytes
- **Protocol 1 Vulnerability**: Protocol 1 uses a fixed IV (all zeros), making it vulnerable to replay attacks

## Evidence
**File**: `components/mod_fido2/src/ctap2.cpp:2191`
```cpp
uint8_t iv[16] = {0};  // IV is all zeros for PIN protocol 1
```

**File**: `components/mod_fido2/src/ctap2.cpp:2199`
```cpp
ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, len, iv, input, output);
```

The code uses `mbedtls_aes_crypt_cbc()` which provides encryption only, not authenticated encryption.

## Recommended Fix
Replace AES-CBC with AES-GCM (Galois-Modes Counter Mode), which provides both encryption and authentication:

```cpp
static bool aes_256_gcm_encrypt(const uint8_t *key, const uint8_t *input,
                                 size_t len, uint8_t *output, size_t *out_len) {
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);

    // Generate random IV
    uint8_t iv[12];  // GCM uses 12-byte IV
    secure_random_fill(iv, 12);

    // Copy IV to output
    memcpy(output, iv, 12);

    // Encrypt with authentication
    uint8_t tag[16];
    size_t ciphertext_len = 0;
    int ret = mbedtls_cipher_setkey(&gcm, key, 256, MBEDTLS_ENCRYPT);
    if (ret != 0) {
        mbedtls_gcm_free(&gcm);
        return false;
    }

    ret = mbedtls_cipher_crypt(&gcm, iv, 12, input, len,
                               output + 12, &ciphertext_len, tag, 16);
    if (ret != 0) {
        mbedtls_gcm_free(&gcm);
        return false;
    }

    // Append authentication tag
    memcpy(output + 12 + ciphertext_len, tag, 16);
    *out_len = 12 + ciphertext_len + 16;  // IV + ciphertext + tag

    mbedtls_gcm_free(&gcm);
    return true;
}
```

Similarly, update decryption to verify the authentication tag:

```cpp
static bool aes_256_gcm_decrypt(const uint8_t *key, const uint8_t *input,
                                 size_t len, uint8_t *output, size_t *out_len) {
    // Extract IV and tag
    const uint8_t *iv = input;
    const uint8_t *ciphertext = input + 12;
    const uint8_t *tag = input + 12 + (len - 28);

    // Decrypt and verify tag
    size_t ciphertext_len = len - 28;
    int ret = mbedtls_cipher_crypt(&gcm, iv, 12, ciphertext, ciphertext_len,
                                   output, out_len, tag, 16);
    return ret == 0;
}
```

## References
- CTAP 2.1 Specification - pinUvAuthToken encryption
- NIST SP 800-38D - GCM Mode
- Mbed TLS Documentation - `mbedtls_gcm_context`
- CWE-327: Use of a Broken or Risky Cryptographic Algorithm

</content>