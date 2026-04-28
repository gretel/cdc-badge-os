---
title: "[MEDIUM] No error boundary around TROPIC01 secure session operations"
severity: MEDIUM
domain: error-handling
lens: error-boundaries
labels:
  - "error-handling"
  - "secure-element"
---

## Summary
The `Tropic01Element` class in `components/cdc_hal/src/Tropic01Element.cpp` performs secure session operations without comprehensive error boundaries. Session failures during ECC or R-memory operations can leave the module in an inconsistent state.

**Evidence:**
- `components/cdc_hal/src/Tropic01Element.cpp:331-365` (eccGenerate):
```cpp
SeResult Tropic01Element::eccGenerate(uint8_t slot, EccCurve curve) {
    if (slot >= ECC_SLOT_COUNT) {
        return SeResult::INVALID_PARAM;
    }

    lock();

    if (!ensureSession("eccGenerate")) {
        unlock();
        return SeResult::SESSION_REQUIRED;
    }

    lt_ecc_curve_type_t ltCurve = (curve == EccCurve::ED25519) ?
                                   TR01_CURVE_ED25519 : TR01_CURVE_P256;

    lt_ret_t ret = lt_ecc_key_generate(&handle_, static_cast<lt_ecc_slot_t>(slot), ltCurve);
    if (ret == LT_OK) {
        eccSlotCache_ |= (1u << slot);
    }
    handleSessionError(ret);

    unlock();
    return mapResult(ret);  // Error mapped but no boundary for callers
}
```

- `components/cdc_hal/src/Tropic01Element.cpp:271-281` (ensureSession):
```cpp
bool Tropic01Element::ensureSession(const char* op) {
    if (sessionActive_) return true;
    LOG_W(TAG, "Session inactive for %s - restarting", op);
    return sessionStart();  // Can fail, but no retry logic
}
```

- `components/cdc_hal/src/Tropic01Element.cpp:191-230` (sessionStart):
```cpp
bool Tropic01Element::sessionStart() {
    lock();

    if (sessionActive_) {
        unlock();
        return true;
    }

    LOG_I(TAG, "Starting secure session...");

    lt_ret_t ret = lt_verify_chip_and_start_secure_session(
        &handle_, PAIRING_KEY_PRIV, PAIRING_KEY_PUB, PAIRING_KEY_SLOT);

    if (ret != LT_OK) {
        LOG_E(TAG, "Secure session failed (%s)", lt_ret_verbose(ret));
        sessionActive_ = false;
        unlock();
        return false;  // Silent failure - no error broadcast
    }

    // ... success path
    unlock();
    return true;
}
```

## Impact
- **Silent session failures**: Session restart can fail without notification
- **Inconsistent state**: Cache may be stale after session error
- **No retry mechanism**: Transient secure element errors not recovered
- **Caller burden**: Each caller must handle session errors individually

## Recommended Fix
Add error boundaries and retry logic to critical operations:

```cpp
bool Tropic01Element::sessionStart() {
    lock();

    if (sessionActive_) {
        unlock();
        return true;
    }

    LOG_I(TAG, "Starting secure session...");

    // Retry session start with exponential backoff
    uint8_t retries = 3;
    for (uint8_t i = 0; i < retries; i++) {
        lt_ret_t ret = lt_verify_chip_and_start_secure_session(
            &handle_, PAIRING_KEY_PRIV, PAIRING_KEY_PUB, PAIRING_KEY_SLOT);

        if (ret == LT_OK) {
            sessionActive_ = true;
            eccCacheValid_ = false;
            unlock();
            return true;
        }

        handleSessionError(ret);
        unlock();

        if (i < retries - 1) {
            LOG_W(TAG, "Session start failed, retry %d/%d", i + 1, retries);
            vTaskDelay(pdMS_TO_TICKS(50 * (1 << i)));  // 50ms, 100ms, 200ms
        }
    }

    LOG_E(TAG, "Secure session failed after %d retries", retries);
    // Optionally publish error event
    return false;
}

SeResult Tropic01Element::eccGenerate(uint8_t slot, EccCurve curve) {
    if (slot >= ECC_SLOT_COUNT) {
        return SeResult::INVALID_PARAM;
    }

    lock();

    // Ensure session with retry
    uint8_t retries = 2;
    for (uint8_t i = 0; i < retries; i++) {
        if (ensureSession("eccGenerate")) break;
        if (i < retries - 1) vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (!sessionActive_) {
        unlock();
        return SeResult::SESSION_REQUIRED;
    }

    lt_ecc_curve_type_t ltCurve = (curve == EccCurve::ED25519) ?
                                   TR01_CURVE_ED25519 : TR01_CURVE_P256;

    lt_ret_t ret = lt_ecc_key_generate(&handle_, static_cast<lt_ecc_slot_t>(slot), ltCurve);
    if (ret == LT_OK) {
        eccSlotCache_ |= (1u << slot);
    }
    handleSessionError(ret);

    unlock();
    return mapResult(ret);
}
```

## References
- [Secure Element Error Handling](https://www.trustonic.com/technology/tropic01/)
- [Retry Pattern](https://learn.microsoft.com/en-us/azure/architecture/best-practices/retry-service-specific)
