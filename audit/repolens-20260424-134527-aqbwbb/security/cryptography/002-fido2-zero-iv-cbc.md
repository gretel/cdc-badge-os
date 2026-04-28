---
title: "[MEDIUM] Fixed Zero IV Used for AES-CBC in FIDO2 ClientPIN Protocol 1"
severity: MEDIUM
domain: cryptographic-implementation
lens: cdc-badge-os
labels:
  - aes
  - cbc
  - iv
  - fido2
---

## Summary

The FIDO2 ClientPIN Protocol 1 implementation uses a fixed zero IV for AES-256-CBC encryption. While this follows the CTAP2 specification for Protocol 1 compatibility, using a static IV can leak patterns in the plaintext, especially when encrypting similar data with the same key.

**Location:** `components/mod_fido2/src/ctap2.cpp:2172-2202`

## Impact

- **Pattern leakage**: ECB-like behavior when the same plaintext blocks appear at the same position
- **Known plaintext attacks**: If an attacker knows part of the plaintext, they can recover the corresponding ciphertext blocks more easily
- **Protocol 2 is stronger**: Protocol 2 implementation correctly generates a random IV for each encryption

Note: The impact is limited because:
1. This follows CTAP2 spec for backward compatibility
2. Protocol 2 (with random IV) is available and preferred
3. The encrypted data (PIN hash) is fixed-length and relatively short

## Evidence

File: `components/mod_fido2/src/ctap2.cpp`

Protocol 1 (fixed zero IV):
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

Protocol 2 (random IV - correct implementation):
```cpp
static bool aes_256_cbc_encrypt_with_random_iv(const uint8_t *key, const uint8_t *input,
                                                size_t len, uint8_t *output) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    uint8_t iv[16];
    ctap2_random_fill(iv, sizeof(iv));  // Random IV for Protocol 2

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

## Recommended Fix

1. **Add documentation** explaining the zero IV is for CTAP2 Protocol 1 spec compliance:
   ```cpp
   // CTAP2 ClientPIN Protocol 1 uses a fixed zero IV for backward compatibility
   // Protocol 2 uses a random IV (see aes_256_cbc_encrypt_with_random_iv)
   // Consider preferring Protocol 2 for new implementations
   ```

2. **Add a helper function** to get the protocol version and prefer Protocol 2:
   ```cpp
   static uint8_t get_pin_protocol_version() {
       // Return 2 for Protocol 2 (preferred) or 1 for backward compatibility
       return 2;
   }
   ```

3. **Update calling code** to use Protocol 2 by default when the authenticator supports it

## References

- [FIDO CTAP2 Specification - ClientPIN Command](https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#clientpin-command)
- [NIST SP 800-38A - Recommendation for Block Cipher Modes of Operation](https://csrc.nist.gov/publications/detail/sp/800-38a/final)
- [Mbed TLS AES-CBC Documentation](https://tls.mbed.org/api/aes_8h.html)
