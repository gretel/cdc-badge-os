---
title: "[LOW] TRNG fallback is silent - TROPIC01 random failures not detected"
severity: LOW
domain: error-path-tests
lens: random-number
labels:
  - "trng"
  - "tropic01"
  - "random"
---

## Summary
In `components/cdc_hal/src/Tropic01Element.cpp` (lines 776-806), when TROPIC01 TRNG fails, the code silently falls back to ESP32 TRNG with only a warning log. This means TROPIC01 random number generation failures go unnoticed, potentially affecting security-critical operations.

**Files:**
- `components/cdc_hal/src/Tropic01Element.cpp:776-806`
- `components/mod_gpg/src/GpgStorage.cpp:270-271`
- `components/mod_fido2/src/ctap2.cpp:145-148`

## Impact
1. **Silent Degradation**: TROPIC01 TRNG failure goes unnoticed
2. **Security**: ESP32 TRNG may be less secure than TROPIC01 TRNG
3. **Debug Difficulty**: Hard to detect when TROPIC01 TRNG is failing
4. **No Monitoring**: No way to track TRNG failure rate

## Evidence
From `components/cdc_hal/src/Tropic01Element.cpp`:

```cpp
// Lines 776-806: getRandom() - silent fallback
SeResult Tropic01Element::getRandom(uint8_t* buffer, size_t length) {
    if (length == 0 || buffer == nullptr) {
        return SeResult::INVALID_PARAM;
    }
    
    // Try TROPIC01 TRNG first
    lt_ret_t result = lt_random_value_get(length, buffer);
    if (result == LT_RET_OK) {
        return SeResult::OK;
    }
    
    // ❌ Silent fallback to ESP32 TRNG!
    LOG_W("TROPIC", "TROPIC TRNG failed, using ESP32 TRNG");
    
    // ESP32 TRNG fallback
    esp_fill_random(buffer, length);  // No error check!
    
    // Always returns OK even if TROPIC failed
    return SeResult::OK;
}

// Usage in GpgStorage.cpp:270-271
tropicElement->getRandom(key, 32);  // ❌ No error check, always OK
```

**Problem:**
- TROPIC01 TRNG failure is logged as warning only
- ESP32 TRNG fallback always succeeds (no error check)
- Function always returns `SeResult::OK`
- No way for caller to detect TROPIC01 TRNG degradation

## Recommended Fix
Add proper error tracking for TRNG fallback:

1. **Add TRNG failure counter**:
   ```cpp
   class Tropic01Element {
   private:
       uint32_t trngFailures = 0;
       uint32_t trngAttempts = 0;
       
   public:
       SeResult getRandom(uint8_t* buffer, size_t length) {
           trngAttempts++;
           
           lt_ret_t result = lt_random_value_get(length, buffer);
           if (result == LT_RET_OK) {
               return SeResult::OK;
           }
           
           trngFailures++;
           LOG_W("TROPIC", "TROPIC TRNG failed (%lu/%lu), using ESP32 fallback", 
                 trngFailures, trngAttempts);
           
           // Check ESP32 TRNG too
           esp_fill_random(buffer, length);
           
           // Return warning code if we want to track
           return SeResult::OK;  // Still OK, but logged
       }
       
       float getTrngFailureRate() const {
           if (trngAttempts == 0) return 0.0f;
           return (float)trngFailures / trngAttempts;
       }
   };
   ```

2. **Add threshold for TRNG failure**:
   ```cpp
   #define TROPIC_TRNG_FAILURE_THRESHOLD 0.1f  // 10%
   
   SeResult Tropic01Element::getRandom(uint8_t* buffer, size_t length) {
       // ... existing code ...
       
       if (getTrngFailureRate() > TROPIC_TRNG_FAILURE_THRESHOLD) {
           LOG_E("TROPIC", "TROPIC TRNG failure rate > 10%%, consider hardware check");
           // Maybe trigger hardware self-test?
       }
       
       return SeResult::OK;
   }
   ```

3. **Add test cases**:
   - Mock `lt_random_value_get()` to fail
   - Verify ESP32 fallback is used
   - Verify warning is logged
   - Mock persistent failure, verify failure rate tracking
   - Verify threshold warning is triggered

## References
- ESP32 TRNG: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/random.html
- TROPIC01 TRNG: https://www.microchip.com/content/dam/mchp/documents/TROPIC/ProductDocuments/DataSheets/TROPIC01-DS-00000889.pdf

</content>