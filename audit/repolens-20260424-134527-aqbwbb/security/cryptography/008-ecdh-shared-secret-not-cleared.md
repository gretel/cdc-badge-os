---
title: "[MEDIUM] ECDH Shared Secret Not Cleared After Use"
severity: MEDIUM
domain: cryptographic-implementation
lens: cdc-badge-os
labels:
  - ecdh
  - key-management
  - memory-clearing
---

## Summary

The FIDO2 ClientPIN ECDH shared secret computation function does not clear the intermediate secret material (`ecdh_z` and `prk` variables) after deriving the AES key. This leaves sensitive cryptographic material in memory until the stack is reused.

**Location:** `components/mod_fido2/src/ctap2.cpp:2062-2113`

## Impact

- **Stack memory exposure**: The 32-byte ECDH shared secret (`ecdh_z`) and 32-byte PRK (`prk`) remain on the stack after the function returns
- **Potential data leakage**: If the stack is later used for other purposes or dumped (e.g., core dump, crash), the secrets could be recovered
- **Protocol 2 is more affected**: Uses both `ecdh_z` and `prk` (64 bytes total), while Protocol 1 only uses `ecdh_z` (32 bytes)
- **Defense in depth**: Even with good RNG and strong algorithms, proper key clearing is essential for defense in depth

Note: The impact is limited because:
1. The secrets are on the stack (not heap), so they're only valid until the function returns
2. The function is relatively short-lived and called infrequently
3. The TROPIC01 secure element handles the actual ECDH computation, so the private key isn't on the stack

## Evidence

File: `components/mod_fido2/src/ctap2.cpp` (lines 2062-2133)

```cpp
static bool client_pin_compute_shared_secret(const uint8_t *platform_key_x,
                                              const uint8_t *platform_key_y,
                                              uint8_t pin_protocol,
                                              uint8_t *shared_secret) {
    // ... setup code ...

    // Extract x coordinate as bytes (Z = ECDH shared secret)
    uint8_t ecdh_z[32];  // <-- 32 bytes of secret material
    ret = mbedtls_mpi_write_binary(&shared_x, ecdh_z, 32);
    if (ret != 0) goto cleanup;

    if (pin_protocol == 1) {
        // Protocol 1: sharedSecret = SHA256(Z)
        mbedtls_sha256(ecdh_z, 32, shared_secret, 0);
    } else {
        // Protocol 2: Use HKDF-SHA256 to derive AES key
        const char *info = "CTAP2 AES key";
        uint8_t prk[32];  // <-- Another 32 bytes of secret material
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
    }

cleanup:
    mbedtls_ecp_point_free(&platform_point);
    mbedtls_mpi_free(&shared_x);
    // <-- MISSING: ecdh_z and prk not cleared!
    return ret == 0;
}
```

Compare with GPG module which properly clears secrets (line 124):
```cpp
mbedtls_platform_zeroize(chip_id, sizeof(chip_id));
```

## Recommended Fix

Add `mbedtls_platform_zeroize()` calls to clear the secret material before returning:

```cpp
static bool client_pin_compute_shared_secret(...) {
    // ... existing code ...

    uint8_t ecdh_z[32];
    // ... use ecdh_z ...

    if (pin_protocol == 1) {
        mbedtls_sha256(ecdh_z, 32, shared_secret, 0);
    } else {
        uint8_t prk[32];
        // ... use prk ...
        mbedtls_md_hmac(..., prk, ...);
        
        // Clear PRK after use
        mbedtls_platform_zeroize(prk, sizeof(prk));
    }

cleanup:
    mbedtls_ecp_point_free(&platform_point);
    mbedtls_mpi_free(&shared_x);
    
    // Clear ECDH shared secret
    mbedtls_platform_zeroize(ecdh_z, sizeof(ecdh_z));
    
    return ret == 0;
}
```

Alternatively, use `mbedtls_platform_zeroize()` for all intermediate secrets:
- `ecdh_z` (32 bytes) - ECDH shared secret
- `prk` (32 bytes) - HKDF pseudorandom key (Protocol 2)
- `expand_input` (32 bytes) - HKDF expansion input (Protocol 2)
- `zero_salt` (32 bytes) - less critical but good practice

## References

- [Mbed TLS Platform Util Documentation](https://tls.mbed.org/api/platform__util_8h.html)
- [NIST SP 800-131A Rev 2 - Cryptographic Key Management](https://csrc.nist.gov/publications/detail/sp/800-131a/rev-2/final)
- [FIDO CTAP2 Pin Protocol](https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#pin-protocols)
- [Secure Memory Zeroing Best Practices](https://cheatsheetseries.owasp.org/cheatsheets/Cryptographic_Storage_Cheat_Sheet.html#secure-memory-zeroing)
