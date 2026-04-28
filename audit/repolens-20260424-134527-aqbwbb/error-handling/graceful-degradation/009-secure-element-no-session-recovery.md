---
title: "[MEDIUM] Secure element operations fail immediately instead of auto-recovering session"
severity: MEDIUM
domain: graceful-degradation
lens: error-handling
labels:
  - "secure-element"
  - "session-management"
  - "graceful-degradation"
---

## Summary
In `components/cdc_hal/src/Tropic01Element.cpp`, the `ensureSession()` method (line 270-277) attempts to restart a session when it's inactive, but only returns `true/false`. Callers then immediately return an error instead of retrying the operation after session recovery. This creates an all-or-nothing behavior where a transient session failure causes the entire operation to fail.

**File**: `components/cdc_hal/src/Tropic01Element.cpp`  
**Lines**: 270-277, and all operations that call `ensureSession()`

## Impact
- **User Experience**: Transient session timeouts (common after light sleep) require manual retry instead of being transparently recovered.
- **Reliability**: Operations that could succeed after a simple session restart fail permanently.
- **Graceful Degradation**: The system could auto-recover but instead treats session expiration as a hard failure.

## Evidence
```cpp
// components/cdc_hal/src/Tropic01Element.cpp:270-277
bool Tropic01Element::ensureSession(const char* op) {
    if (sessionActive_) return true;
    LOG_W(TAG, "Session inactive for %s - restarting", op);
    return sessionStart();  // Returns true/false but caller just propagates error
}

// Example usage in eccGenerate (line 326-344):
SeResult Tropic01Element::eccGenerate(uint8_t slot, EccCurve curve) {
    // ...
    lock();
    
    if (!ensureSession("eccGenerate")) {  // If session restart fails...
        unlock();
        return SeResult::SESSION_REQUIRED;  // ...immediate failure, no retry
    }
    
    // Operation continues only if session is active
    lt_ret_t ret = lt_ecc_key_generate(&handle_, static_cast<lt_ecc_slot_t>(slot), ltCurve);
    // ...
}
```

Similar pattern in all operations:
- `eccImport()` (line 357)
- `eccGetPublicKey()` (line 391)
- `eccDelete()` (line 425)
- `ecdsaSign()` (line 474)
- `eddsaSign()` (line 504)
- `rmemRead()` (line 533)
- `rmemWrite()` (line 567)
- `rmemErase()` (line 595)
- `getRandom()` (line 762) - **Has fallback TRNG but only on session failure, not other errors**
- `getChipId()` (line 794)
- `getFwVersion()` (line 826)

The `getRandom()` function is the exception with its fallback:
```cpp
// components/cdc_hal/src/Tropic01Element.cpp:762-780
bool Tropic01Element::getRandom(uint8_t* buffer, uint16_t size) {
    // ...
    lock();
    
    if (!ensureSession("getRandom")) {
        // Fallback to ESP32 TRNG
        LOG_W(TAG, "Using ESP32 TRNG as fallback");
        esp_fill_random(buffer, size);  // Graceful degradation!
        unlock();
        return true;
    }
    // ...
}
```

## Recommended Fix
Implement session auto-recovery with retry for all operations:

```cpp
SeResult Tropic01Element::eccGenerate(uint8_t slot, EccCurve curve) {
    if (slot >= ECC_SLOT_COUNT) {
        return SeResult::INVALID_PARAM;
    }

    lock();
    
    // Auto-recover session if needed (try once)
    if (!sessionActive_) {
        if (sessionStart()) {
            LOG_I(TAG, "Session auto-recovered for eccGenerate");
        } else {
            unlock();
            return SeResult::SESSION_REQUIRED;
        }
    }

    lt_ecc_curve_type_t ltCurve = (curve == EccCurve::ED25519) ?
                                    TR01_CURVE_ED25519 : TR01_CURVE_P256;

    lt_ret_t ret = lt_ecc_key_generate(&handle_, static_cast<lt_ecc_slot_t>(slot), ltCurve);
    if (ret == LT_OK) {
        eccSlotCache_ |= (1u << slot);
    }
    
    // If session error, try one more time after re-establishing
    if (ret == LT_L2_NO_SESSION || ret == LT_HOST_NO_SESSION) {
        unlock();
        // Quick retry after session recovery
        lock();
        if (sessionStart() && 
            (ret = lt_ecc_key_generate(&handle_, static_cast<lt_ecc_slot_t>(slot), ltCurve)) == LT_OK) {
            eccSlotCache_ |= (1u << slot);
        }
    }
    
    handleSessionError(ret);
    unlock();
    return mapResult(ret);
}
```

## References
- TROPIC01 session management: https://github.com/libtropic/libtropic
- Graceful degradation: Systems should continue operating at reduced capability when components fail
- Auto-recovery pattern: Transient failures should be retried transparently
