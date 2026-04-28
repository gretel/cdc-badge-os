---
title: "[MEDIUM] PIN Hash Enc Buffer Not Cleared After Use"
severity: MEDIUM
domain: cryptographic-implementation
lens: cdc-badge-os
labels:
  - fido2
  - pin-protocol
  - key-management
  - memory-clearing
---

## Summary

The `client_pin_get_pin_uv_auth_token_using_pin_with_permissions()` function in the FIDO2 module does not clear the sensitive intermediate buffers (`pin_hash_enc`, `shared_secret`, `decrypted_pin_hash`) after deriving the PIN token. These buffers contain encrypted PIN hash and the ECDH shared secret used for decryption.

**Location:** `components/mod_fido2/src/ctap2.cpp:2353-2632` (function ends at line 2633)

## Impact

- **Stack memory exposure**: The `pin_hash_enc` buffer (32-64 bytes) contains the encrypted PIN hash received from the host
- **Shared secret**: The `shared_secret` buffer (32 bytes) is the ECDH shared secret used to decrypt the PIN hash
- **Decrypted PIN hash**: The `decrypted_pin_hash` buffer (16 bytes) contains the actual PIN hash for comparison
- **Protocol 2 more affected**: Uses larger buffers (IV + ciphertext) compared to Protocol 1
- **Defense in depth**: Even with good RNG and strong algorithms, proper key clearing is essential for defense in depth

Note: The impact is limited because:
1. The secrets are on the stack (not heap), so they're only valid until the function returns
2. The function is relatively short-lived and called infrequently (during PIN verification)
3. The actual PIN is hashed before storage, so the raw PIN isn't in these buffers

## Evidence

File: `components/mod_fido2/src/ctap2.cpp` (lines 2353-2632)

```cpp
static uint8_t client_pin_get_pin_uv_auth_token_using_pin_with_permissions(
    const uint8_t *params, uint16_t params_len,
    uint8_t *response, uint16_t *response_len) {
    
    // Parse parameters
    cbor_reader_t r;
    cbor_reader_init(&r, params, params_len);

    uint8_t platform_key_x[32] = {0};
    uint8_t platform_key_y[32] = {0};
    uint8_t pin_hash_enc[64] = {0};  // <-- 64 bytes of encrypted PIN hash
    size_t pin_hash_enc_len = 0;
    // ...

    // Compute shared secret
    uint8_t shared_secret[32];  // <-- 32 bytes of ECDH shared secret
    if (!client_pin_compute_shared_secret(platform_key_x, platform_key_y, pin_protocol, shared_secret)) {
        // ...
    }

    // Decrypt pinHashEnc
    uint8_t decrypted_pin_hash[16];  // <-- 16 bytes of decrypted PIN hash
    if (pin_protocol == 2 && pin_hash_enc_len == 32) {
        const uint8_t *iv = pin_hash_enc;
        const uint8_t *ciphertext = pin_hash_enc + 16;
        if (!aes_256_cbc_decrypt_iv(shared_secret, iv, ciphertext, 16, decrypted_pin_hash)) {
            // ...
        }
    } else {
        uint8_t decrypted[64];
        if (!aes_256_cbc_decrypt(shared_secret, pin_hash_enc, pin_hash_enc_len, decrypted)) {
            // ...
        }
        memcpy(decrypted_pin_hash, decrypted, 16);
    }

    // Verify PIN hash
    if (!pin_storage_verify_fido2_hash(decrypted_pin_hash)) {
        // ...
    }

    // ... generate pinToken and return ...
    return CTAP2_OK;
    // <-- MISSING: pin_hash_enc, shared_secret, decrypted_pin_hash not cleared!
}
```

Compare with the GPG module which properly clears secrets (line 1406):
```cpp
mbedtls_platform_zeroize(shared_secret, sizeof(shared_secret));
```

Note: The related function `client_pin_compute_shared_secret()` has a similar issue documented in finding #008, but this function has additional buffers that need clearing.

## Recommended Fix

Add `mbedtls_platform_zeroize()` calls to clear the secret material before returning:

```cpp
static uint8_t client_pin_get_pin_uv_auth_token_using_pin_with_permissions(...) {
    // ... existing code ...

    uint8_t pin_hash_enc[64] = {0};
    uint8_t shared_secret[32];
    uint8_t decrypted_pin_hash[16];
    
    // ... use the buffers ...

    // Verify PIN hash
    if (!pin_storage_verify_fido2_hash(decrypted_pin_hash)) {
        // ...
    }

    // ... generate pinToken ...

    // Clear sensitive buffers before return
    mbedtls_platform_zeroize(decrypted_pin_hash, sizeof(decrypted_pin_hash));
    mbedtls_platform_zeroize(shared_secret, sizeof(shared_secret));
    mbedtls_platform_zeroize(pin_hash_enc, sizeof(pin_hash_enc));
    
    return CTAP2_OK;
}
```

For defensive programming, consider clearing the buffers at all exit points:
- On error returns
- After PIN verification (both success and failure)
- After the shared secret is no longer needed

Alternatively, use a single cleanup label:
```cpp
cleanup:
    mbedtls_platform_zeroize(decrypted_pin_hash, sizeof(decrypted_pin_hash));
    mbedtls_platform_zeroize(shared_secret, sizeof(shared_secret));
    mbedtls_platform_zeroize(pin_hash_enc, sizeof(pin_hash_enc));
    return result;
```

## References

- [Mbed TLS Platform Util Documentation](https://tls.mbed.org/api/platform__util_8h.html)
- [NIST SP 800-131A Rev 2 - Cryptographic Key Management](https://csrc.nist.gov/publications/detail/sp/800-131a/rev-2/final)
- [FIDO CTAP2 Pin Protocol](https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#pin-protocols)
- [Secure Memory Zeroing Best Practices](https://cheatsheetseries.owasp.org/cheatsheets/Cryptographic_Storage_Cheat_Sheet.html#secure-memory-zeroing)

(Related to finding #008: ECDH Shared Secret Not Cleared After Use)

</content>