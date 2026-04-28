---
title: "[LOW] Tropic01Element::getRandom() always returns true masking RNG failures"
severity: LOW
domain: cdc_hal
lens: error-handling
labels:
  - "audit:error-handling/error-swallowing"
---

## Summary
In `components/cdc_hal/src/Tropic01Element.cpp:781-807`, the `getRandom()` function always returns `true` even when TROPIC01 RNG fails. While it does fallback to ESP32 TRNG, callers cannot distinguish between "TROPIC01 RNG used" and "ESP32 fallback used".

**Location:** `components/cdc_hal/src/Tropic01Element.cpp:781-807`

```cpp
bool Tropic01Element::getRandom(uint8_t* buffer, uint16_t size) {
    if (!buffer || size == 0) {
        return false;
    }

    lock();

    if (!ensureSession("getRandom")) {
        // Fallback to ESP32 TRNG
        LOG_W(TAG, "Using ESP32 TRNG as fallback");
        esp_fill_random(buffer, size);
        unlock();
        return true;  // Returns true even on fallback
    }

    lt_ret_t ret = lt_random_value_get(&handle_, buffer, size);
    handleSessionError(ret);

    unlock();

    if (ret != LT_OK) {
        LOG_W(TAG, "TROPIC01 TRNG failed, using ESP32 TRNG");
        esp_fill_random(buffer, size);
    }

    return true;  // Always returns true!
}
```

## Impact
- Callers cannot detect when TROPIC01 RNG fails
- Quality of randomness may vary (TROPIC01 vs ESP32 TRNG)
- Hard to debug RNG source issues
- May mask underlying TROPIC01 communication problems

## Evidence
The function:
1. Logs warnings when falling back to ESP32 TRNG (lines 790, 802)
2. Always returns `true` regardless of which RNG source was used
3. No way for caller to know if TROPIC01 RNG worked or if fallback was used

For cryptographic applications (FIDO2, GPG, etc.), knowing the RNG source matters for security audits.

## Recommended Fix
Consider returning status or adding a method to query RNG source:

**Option 1: Return status with source info**
```cpp
enum class RngSource {
    TROPIC01,
    ESP32_FALLBACK
};

struct RngResult {
    bool success;
    RngSource source;
};

RngResult Tropic01Element::getRandom(uint8_t* buffer, uint16_t size);
```

**Option 2: Simple boolean for TROPIC01 success**
```cpp
/**
 * \brief Gets random bytes from TROPIC01 with ESP32 fallback.
 * \param buffer Destination buffer.
 * \param size Number of bytes to generate.
 * \return `true` if TROPIC01 RNG succeeded, `false` if fallback was used.
 */
bool Tropic01Element::getRandom(uint8_t* buffer, uint16_t size) {
    if (!buffer || size == 0) {
        return false;
    }

    lock();

    if (!ensureSession("getRandom")) {
        LOG_W(TAG, "Using ESP32 TRNG as fallback");
        esp_fill_random(buffer, size);
        unlock();
        return false;  // Indicate fallback
    }

    lt_ret_t ret = lt_random_value_get(&handle_, buffer, size);
    handleSessionError(ret);
    unlock();

    if (ret != LT_OK) {
        LOG_W(TAG, "TROPIC01 TRNG failed, using ESP32 TRNG");
        esp_fill_random(buffer, size);
        return false;  // Indicate fallback
    }

    return true;  // TROPIC01 RNG succeeded
}
```

## References
- NIST SP 800-90A: Random Number Generation
- ESP32 TRNG vs External RNG: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/random.html
