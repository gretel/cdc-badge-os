---
title: "[LOW] AES-256-CBC Used Instead of GCM for FIDO2 PIN Protocol"
severity: LOW
domain: cryptographic-implementation
lens: cdc-badge-os
labels:
  - aes
  - cbc
  - gcm
  - fido2
---

## Summary

The FIDO2 PIN protocol uses AES-256-CBC for encryption, which provides confidentiality but not integrity. GCM mode would provide both confidentiality and authenticated encryption. The current implementation follows the CTAP2 specification, but GCM would be cryptographically stronger.

**Location:** `components/mod_fido2/src/ctap2.cpp:2145-2202`

## Impact

- **No authenticated encryption**: CBC mode doesn't verify ciphertext integrity
- **Padding oracle potential**: CBC with PKCS7 padding could be vulnerable to padding oracle attacks (though limited by the protocol design)
- **HMAC provides integrity**: The pin_token is verified with HMAC-SHA-256, mitigating the lack of authenticated encryption

Note: The impact is limited because:
1. The CTAP2 spec uses CBC for compatibility
2. HMAC verification of pin_token provides integrity
3. The encrypted data is relatively short (PIN hash)

## Evidence

File: `components/mod_fido2/src/ctap2.cpp`

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

PIN verification uses HMAC:
```cpp
// Verify HMAC-SHA-256(pinToken, clientDataHash)
mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                pin_token, PIN_TOKEN_SIZE, client_data_hash, 32, expected);
// Compare with expected...
```

## Recommended Fix

1. **Add documentation** explaining the HMAC provides integrity:
   ```cpp
   // AES-256-CBC per CTAP2 spec. Integrity is provided by HMAC-SHA-256
   // verification of the pin_token (see verifyPinToken())
   ```

2. **Consider GCM for future protocol versions** if CTAP3 supports it:
   ```cpp
   static bool aes_256_gcm_encrypt(const uint8_t *key, const uint8_t *input,
                                    size_t len, uint8_t *output) {
       mbedtls_gcm_context gcm;
       mbedtls_gcm_init(&gcm);

       uint8_t iv[12];  // 96-bit IV for GCM
       ctap2_random_fill(iv, sizeof(iv));

       int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
       if (ret != 0) {
           mbedtls_gcm_free(&gcm);
           return false;
       }

       uint8_t tag[16];
       ret = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, len,
                                       iv, sizeof(iv), NULL, 0,
                                       input, output, 16, tag);
       mbedtls_gcm_free(&gcm);
       return ret == 0;
   }
   ```

3. **Verify no padding oracle vulnerability** by ensuring error messages don't leak padding information

## References

- [FIDO CTAP2 Specification](https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html)
- [NIST SP 800-38D - GCM Mode](https://csrc.nist.gov/publications/detail/sp/800-38d/final)
- [Mbed TLS GCM Documentation](https://tls.mbed.org/api/gcm_8h.html)
- [Padding Oracle Attacks](https://en.wikipedia.org/wiki/Padding_oracle_attack)
