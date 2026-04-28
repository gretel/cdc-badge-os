---
title: "[LOW] Sensitive PIN data not cleared from stack after verification"
severity: LOW
domain: secrets
lens: secrets-credential-management
labels:
  - "audit:security/secrets"
---

## Summary
PIN values passed to verification functions in `PinManager` are not cleared from stack variables after use. The `verifyBadgePin()`, `verifyPW1()`, and `verifyPW3()` functions in `components/cdc_core/src/PinManager.cpp` receive PIN as a `const char*` parameter but don't clear the calling function's stack buffer.

Additionally, the `computeBadgeHash()` and `computeKdfHash()` functions don't clear the PIN data from local buffers after computing the hash.

## Impact
**Security Risk**: After PIN verification, the PIN string may remain in RAM:
- Stack variables may persist until overwritten
- Memory dumps could reveal PINs
- Swapped memory could expose PINs (though ESP32 doesn't swap)
- Cache lines may retain PIN data

This is a lower-severity issue for embedded systems but still follows best practices for crypto code.

## Evidence

**File: `components/cdc_core/src/PinManager.cpp`**
```cpp
// Lines 396-411 - verifyBadgePin
bool PinManager::verifyBadgePin(const char* pin) {
    if (!pin) return false;
    if (!pinLoaded_) init();
    if (badgeRetries_ == 0) {
        LOG_I(TAG, "Badge blocked");
        return false;
    }

    uint8_t inputHash[BADGE_HASH_SIZE];
    if (!computeBadgeHash(pin, inputHash)) return false;

    if (compareHash(badgeHash_, inputHash, BADGE_HASH_SIZE)) {
        resetBadgeRetries();
        lockoutActive_ = false;
        LOG_I(TAG, "Badge PIN verified");
        return true;
    }
    ...
}

// Lines 217-224 - computeBadgeHash
bool PinManager::computeBadgeHash(const char* pin, uint8_t* hashOut) {
    if (!pin || !hashOut) return false;

    uint8_t fullHash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, (const uint8_t*)pin, strlen(pin));  // PIN in RAM
    mbedtls_sha256_finish(&ctx, fullHash);
    mbedtls_sha256_free(&ctx);

    memcpy(hashOut, fullHash, BADGE_HASH_SIZE);
    return true;  // PIN data still in stack buffer!
}
```

**File: `components/mod_fido2/src/ctap2.cpp`**
```cpp
// Lines 2570-2590 - PIN decryption
// The decrypted PIN hash is used but not clearly cleared
uint8_t decrypted_pin_hash[32];
...
if (!aes_256_cbc_decrypt_iv(shared_secret, iv, ciphertext, 16, decrypted_pin_hash)) {
    LOG_E("PIN", "PIN decryption failed");
    return CTAP1_STATUS_OK;
}
// decrypted_pin_hash is used for comparison but not cleared
```

## Recommended Fix
1. **Use `mbedtls_platform_zeroize()`**: After PIN verification, clear stack buffers:
   ```cpp
   bool PinManager::verifyBadgePin(const char* pin) {
       ...
       uint8_t inputHash[BADGE_HASH_SIZE];
       if (!computeBadgeHash(pin, inputHash)) {
           mbedtls_platform_zeroize(inputHash, sizeof(inputHash));
           return false;
       }
       bool success = compareHash(badgeHash_, inputHash, BADGE_HASH_SIZE);
       mbedtls_platform_zeroize(inputHash, sizeof(inputHash));
       return success;
   }
   ```

2. **Clear PIN in calling functions**: Where PINs are read into buffers, clear them after use:
   ```cpp
   char pinBuf[16];
   readPinFromUser(pinBuf);
   bool ok = verifyBadgePin(pinBuf);
   mbedtls_platform_zeroize(pinBuf, sizeof(pinBuf));
   ```

3. **Use volatile for sensitive buffers**: Ensure compiler doesn't optimize clears:
   ```cpp
   volatile uint8_t buffer[32];
   ```

## References
- [Mbedtls Platform Zeroize](https://github.com/Mbed-TLS/mbedtls/blob/development/docs/architecture/secure-memory.md)
- [OWASP Secure Coding Practices](https://owasp.org/www-project-secure-coding-practices-quick-reference-guide/)
- [CWE-285: Improper clearing of sensitive data](https://cwe.mitre.org/data/definitions/285.html)
