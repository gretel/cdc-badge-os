---
title: "[INFO] Review Random Number Generation Usage Across All Modules"
severity: INFO
domain: cryptographic-implementation
lens: cdc-badge-os
labels:
  - rng
  - entropy
  - best-practice
---

## Summary

The codebase uses good random number generation practices with TROPIC01 TRNG and ESP32 fallback. This is an informational finding to verify all modules follow the same pattern and use cryptographically secure random for all security-sensitive values (nonces, salts, keys, tokens).

**Location:** Multiple files across the codebase

## Impact

- **Positive finding**: Good practices observed
- **Verification needed**: Ensure all modules use the same pattern
- **Consistency**: All modules should use the same RNG interface

Evidence of good practices found:

File: `components/cdc_hal/src/Tropic01Element.cpp:791-803`
```cpp
lt_ret_t ret = lt_random_value_get(&handle_, buffer, size);
if (ret != LT_OK) {
    esp_fill_random(buffer, size);  // Fallback
}
```

File: `components/mod_fido2/src/ctap2.cpp:143-149`
```cpp
static void secure_random_fill(uint8_t *buf, size_t len) {
    lt_ret_t ret = lt_random_value_get(&handle_, buf, len);
    if (ret != LT_OK) {
        esp_fill_random(buf, len);  // Fallback
    }
}
```

File: `components/mod_gpg/src/GpgStorage.cpp:270-271`
```cpp
uint8_t nonce[16];
esp_fill_random(nonce, sizeof(nonce));  // For GCM encryption
```

## Recommended Fix

1. **Create a centralized RNG interface** in `cdc_hal`:
   ```cpp
   // components/cdc_hal/include/cdc_hal/IRandom.h
   class IRandom {
   public:
       virtual ~IRandom() = default;
       virtual void fillRandom(uint8_t *buf, size_t len) = 0;
   };
   ```

2. **Ensure all modules use the centralized interface** instead of calling `esp_fill_random` directly

3. **Add documentation** on proper RNG usage:
   ```cpp
   /**
    * \brief Fill buffer with cryptographically secure random bytes.
    * \param buf Buffer to fill.
    * \param len Number of bytes to generate.
    * \note Uses TROPIC01 TRNG with ESP32 fallback.
    */
   ```

4. **Audit for any use of weak RNG** (e.g., `rand()`, `srand()`, `mbedtls_rand()` without proper seeding)

## References

- [NIST SP 800-90A - DRBG](https://csrc.nist.gov/publications/detail/sp/800-90a/rev-1/final)
- [Mbed TLS RNG Documentation](https://tls.mbed.org/api/random_8h.html)
- [ESP32 Random Number Generator](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/random.html)
