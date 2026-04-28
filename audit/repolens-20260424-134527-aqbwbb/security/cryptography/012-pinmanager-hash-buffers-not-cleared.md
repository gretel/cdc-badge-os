---
title: "[LOW] PinManager Hash Buffers Not Cleared After Use"
severity: LOW
domain: cryptographic-implementation
lens: cdc-badge-os
labels:
  - pin-manager
  - memory-clearing
  - key-management
---

## Summary

The `PinManager` class in `components/cdc_core/src/PinManager.cpp` does not clear sensitive hash buffers after use. The `computeBadgeHash()` function stores the SHA-256 hash in a stack buffer `fullHash[32]` which is then copied to the output but the local buffer itself is not cleared. Similarly, `computeKdfHash()` uses a stack buffer `buffer[64]` containing salt + PIN that is not cleared.

**Location:** `components/cdc_core/src/PinManager.cpp:221-274`

## Impact

- **Stack memory exposure**: The `fullHash` buffer contains the complete SHA-256 hash of the PIN
- **PIN exposure**: The `buffer` in `computeKdfHash()` contains the concatenation of salt + PIN (up to 64 bytes)
- **Defense in depth**: Even though the buffers are on the stack, clearing them provides defense in depth
- **Consistency**: The GPG and FIDO2 modules use `mbedtls_platform_zeroize()` for similar buffers, but PinManager doesn't

Note: The impact is limited because:
1. The buffers are on the stack (not heap), so they're only valid until the function returns
2. The actual PIN is passed as a string reference, not copied into these buffers (except in `computeKdfHash`)
3. The functions are relatively short-lived

## Evidence

File: `components/cdc_core/src/PinManager.cpp` (lines 221-234)

```cpp
bool PinManager::computeBadgeHash(const char* pin, uint8_t* hashOut) {
    if (!pin || !hashOut) return false;

    uint8_t fullHash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, (const uint8_t*)pin, strlen(pin));
    mbedtls_sha256_finish(&ctx, fullHash);
    mbedtls_sha256_free(&ctx);

    memcpy(hashOut, fullHash, BADGE_HASH_SIZE);
    return true;  // <-- MISSING: fullHash not cleared!
}
```

File: `components/cdc_core/src/PinManager.cpp` (lines 243-274)

```cpp
bool PinManager::computeKdfHash(const char* pin, const uint8_t* salt, uint8_t* hashOut) {
    if (!pin || !salt || !hashOut) return false;

    // OpenPGP Iterated+Salted S2K (RFC 4880)
    size_t pinLen = strlen(pin);
    size_t combined = SALT_SIZE + pinLen;

    size_t totalBytes = iterations_;

    uint8_t buffer[64];  // Salt + PIN (max 16)
    memcpy(buffer, salt, SALT_SIZE);
    memcpy(buffer + SALT_SIZE, pin, pinLen);  // <-- PIN in buffer!

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);

    size_t processed = 0;
    while (processed < totalBytes) {
        size_t chunk = (totalBytes - processed < combined) ? (totalBytes - processed) : combined;
        mbedtls_sha256_update(&ctx, buffer, chunk);
        processed += chunk;
    }

    mbedtls_sha256_finish(&ctx, hashOut);
    mbedtls_sha256_free(&ctx);

    return true;  // <-- MISSING: buffer not cleared!
}
```

Compare with the GPG module which properly clears secrets (line 318):
```cpp
mbedtls_platform_zeroize(enc_key, sizeof(enc_key));
mbedtls_platform_zeroize(&storage, sizeof(storage));
```

## Recommended Fix

Add `mbedtls_platform_zeroize()` calls to clear the sensitive buffers before returning:

```cpp
bool PinManager::computeBadgeHash(const char* pin, uint8_t* hashOut) {
    if (!pin || !hashOut) return false;

    uint8_t fullHash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, (const uint8_t*)pin, strlen(pin));
    mbedtls_sha256_finish(&ctx, fullHash);
    mbedtls_sha256_free(&ctx);

    memcpy(hashOut, fullHash, BADGE_HASH_SIZE);
    
    // Clear the hash buffer
    mbedtls_platform_zeroize(fullHash, sizeof(fullHash));
    
    return true;
}
```

```cpp
bool PinManager::computeKdfHash(const char* pin, const uint8_t* salt, uint8_t* hashOut) {
    if (!pin || !salt || !hashOut) return false;

    size_t pinLen = strlen(pin);
    size_t combined = SALT_SIZE + pinLen;
    size_t totalBytes = iterations_;

    uint8_t buffer[64];
    memcpy(buffer, salt, SALT_SIZE);
    memcpy(buffer + SALT_SIZE, pin, pinLen);

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);

    size_t processed = 0;
    while (processed < totalBytes) {
        size_t chunk = (totalBytes - processed < combined) ? (totalBytes - processed) : combined;
        mbedtls_sha256_update(&ctx, buffer, chunk);
        processed += chunk;
    }

    mbedtls_sha256_finish(&ctx, hashOut);
    mbedtls_sha256_free(&ctx);

    // Clear the salt+PIN buffer
    mbedtls_platform_zeroize(buffer, sizeof(buffer));
    
    return true;
}
```

Alternatively, add a cleanup label for consistent clearing at all exit points:
```cpp
cleanup:
    mbedtls_platform_zeroize(buffer, sizeof(buffer));
    return result;
```

## References

- [Mbed TLS Platform Util Documentation](https://tls.mbed.org/api/platform__util_8h.html)
- [NIST SP 800-131A Rev 2 - Cryptographic Key Management](https://csrc.nist.gov/publications/detail/sp/800-131a/rev-2/final)
- [OpenPGP Iterated+Salted S2K (RFC 4880)](https://tools.ietf.org/html/rfc4880#section-3.7.1.1)
- [Secure Memory Zeroing Best Practices](https://cheatsheetseries.owasp.org/cheatsheets/Cryptographic_Storage_Cheat_Sheet.html#secure-memory-zeroing)

</content>