---
title: "[MEDIUM] Lack of secure memory clearing for PIN/hash buffers"
severity: MEDIUM
domain: security
lens: toolgate/session-zap
labels:
  - "audit:toolgate/session-zap"
---

## Summary

PIN values, hash buffers, and intermediate crypto data are not cleared after use, leaving sensitive data in stack memory until overwritten.

**Locations:**
- `components/cdc_core/src/PinManager.cpp:224` - `fullHash[32]` not cleared after badge hash computation
- `components/cdc_core/src/PinManager.cpp:255` - `buffer[64]` not cleared after KDF computation
- `components/mod_fido2/src/ctap2.cpp` - Multiple PIN/hash buffers throughout

## Impact

**Data that remains in memory:**
1. **PIN values**: Raw PIN strings used for verification remain on stack
2. **Hash values**: Computed PIN hashes remain in local buffers
3. **Salt + PIN combinations**: KDF input buffers contain salt+PIN concatenation
4. **Intermediate crypto state**: mbedtls contexts may contain sensitive data

**Example from PinManager.cpp:**

Line 221-234:
```cpp
bool PinManager::computeBadgeHash(const char* pin, uint8_t* hashOut) {
    if (!pin || !hashOut) return false;

    uint8_t fullHash[32];        // <-- Not cleared after use
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, (const uint8_t*)pin, strlen(pin));
    mbedtls_sha256_finish(&ctx, fullHash);
    mbedtls_sha256_free(&ctx);

    memcpy(hashOut, fullHash, BADGE_HASH_SIZE);
    return true;  // fullHash still contains PIN hash!
}
```

Line 243-270:
```cpp
bool PinManager::computeKdfHash(const char* pin, const uint8_t* salt, uint8_t* hashOut) {
    // ...
    uint8_t buffer[64];  // Salt + PIN (max 16)
    memcpy(buffer, salt, SALT_SIZE);
    memcpy(buffer + SALT_SIZE, pin, pinLen);  // <-- PIN in buffer
    
    // ... KDF computation ...
    
    return true;  // buffer still contains salt+PIN!
}
```

**Risk scenarios:**
1. **Stack inspection**: After a function returns, stack memory may be preserved until overwritten
2. **Core dumps**: ESP32 core dumps could capture stack with PIN material
3. **Cold boot attacks**: RAM contents could be read after device power-off
4. **Interrupt context**: An interrupt during function execution could capture stack state

## Evidence

**File: `components/cdc_core/src/PinManager.cpp`**

Line 221-234: `computeBadgeHash()` - `fullHash[32]` not cleared

Line 243-270: `computeKdfHash()` - `buffer[64]` not cleared

**File: `components/mod_fido2/src/ctap2.cpp`**

Line 2514: `uint8_t decrypted_pin_hash[16];` - PIN hash decrypted but not cleared

Line 2535: `uint8_t decrypted[64];` - Decrypted PIN data not cleared

Line 2593: `uint8_t encrypted_token[PIN_TOKEN_SIZE + 16];` - pinToken not cleared

## Recommended Fix

Use `memset` or `secure_memzero()` to clear sensitive data after use:

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
    
    // Clear sensitive data
    memset(fullHash, 0, sizeof(fullHash));
    
    return true;
}

bool PinManager::computeKdfHash(const char* pin, const uint8_t* salt, uint8_t* hashOut) {
    // ...
    uint8_t buffer[64];
    memcpy(buffer, salt, SALT_SIZE);
    memcpy(buffer + SALT_SIZE, pin, pinLen);
    
    // ... KDF computation ...
    
    // Clear sensitive data
    memset(buffer, 0, sizeof(buffer));
    
    return true;
}
```

For mbedtls contexts, use `mbedtls_ecp_keypair_free()` or equivalent to clear crypto state.

**Alternative**: Use ESP32's `secure_memzero()` if available:
```cpp
#include "esp_system.h"
secure_memzero(fullHash, sizeof(fullHash));
```

## References

- CWE-459: Incomplete Cleanup
- [OWASP: Memory Clearing](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html#erase-passwords-from-memory)
- [ESP32 secure_memzero](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html#secure-memzero)
