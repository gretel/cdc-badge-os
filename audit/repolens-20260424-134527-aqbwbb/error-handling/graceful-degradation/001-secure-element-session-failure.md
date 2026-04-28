---
title: "[MEDIUM] TROPIC01 Secure Element - No Fallback for Session Loss During Operations"
severity: MEDIUM
domain: hardware-abstraction
lens: graceful-degradation
labels:
  - "audit:error-handling/graceful-degradation"
---

## Summary
In `components/cdc_hal/src/Tropic01Element.cpp`, operations that require a secure session (e.g., `eccGenerate`, `ecdsaSign`, `rmemRead`) use `ensureSession()` to restart a lost session. However, when `sessionStart()` fails (line 210 returns `false`), the operation returns `SeResult::SESSION_REQUIRED` with no retry logic or fallback. This causes a hard failure for all secure element operations if the session cannot be restarted (e.g., due to hardware alarm, pairing key mismatch, or chip in sleep mode).

Lines of interest:
- `Tropic01Element.cpp:206-215` - `sessionStart()` returns `false` on failure
- `Tropic01Element.cpp:279-283` - `ensureSession()` returns `false` if session restart fails
- `Tropic01Element.cpp:334-338` - `eccGenerate()` returns `SeResult::SESSION_REQUIRED` without retry

## Impact
When the secure element session fails to restart:
1. All ECC operations (key generation, signing) fail immediately
2. All R-Memory operations (reading/writing metadata) fail
3. Modules depending on the secure element (GPG, FIDO2, CA) become unusable
4. No degraded mode is possible (e.g., cached signatures, software fallback for non-critical ops)

The system treats session loss as a terminal error rather than a recoverable condition.

## Evidence
```cpp
// Tropic01Element.cpp:206-215
bool Tropic01Element::sessionStart() {
    // ...
    lt_ret_t ret = lt_verify_chip_and_start_secure_session(...);
    if (ret != LT_OK) {
        LOG_E(TAG, "Secure session failed (%s)", lt_ret_verbose(ret));
        sessionActive_ = false;
        unlock();
        return false;  // Hard failure - no retry
    }
    // ...
}

// Tropic01Element.cpp:279-283
bool Tropic01Element::ensureSession(const char* op) {
    if (sessionActive_) return true;
    LOG_W(TAG, "Session inactive for %s - restarting", op);
    return sessionStart();  // Returns false on failure
}

// Tropic01Element.cpp:334-338
SeResult Tropic01Element::eccGenerate(uint8_t slot, EccCurve curve) {
    // ...
    if (!ensureSession("eccGenerate")) {
        unlock();
        return SeResult::SESSION_REQUIRED;  // No fallback
    }
    // ...
}
```

## Recommended Fix
Implement graceful degradation for session failures:

1. **Add retry logic with exponential backoff** in `ensureSession()`:
   ```cpp
   bool Tropic01Element::ensureSession(const char* op, uint8_t maxRetries = 3) {
       if (sessionActive_) return true;
       for (uint8_t i = 0; i < maxRetries; i++) {
           if (sessionStart()) return true;
           vTaskDelay(pdMS_TO_TICKS(10 * (1 << i)));  // 10ms, 20ms, 40ms
       }
       LOG_W(TAG, "Session restart failed after %u retries", maxRetries);
       return false;
   }
   ```

2. **Distinguish recoverable vs. unrecoverable session errors** in `handleSessionError()`:
   ```cpp
   enum class SessionError {
       RECOVERABLE,    // e.g., session timeout, auto-sleep
       UNRECOVERABLE   // e.g., alarm mode, pairing key mismatch
   };
   
   SessionError classifySessionError(lt_ret_t ret) {
       switch (ret) {
           case LT_L2_NO_SESSION:
           case LT_L2_TAG_ERR:
               return SessionError::RECOVERABLE;
           case LT_L1_CHIP_ALARM_MODE:
           case LT_L2_HSK_ERR:
               return SessionError::UNRECOVERABLE;
           default:
               return SessionError::RECOVERABLE;
       }
   }
   ```

3. **Return partial data where possible** (e.g., cached metadata from R-Memory if session temporarily lost).

## References
- [TROPIC01 Datasheet - Session Management](https://www.microchip.com/en-us/products/security-ic/tropic01)
- [libtropic Library Documentation](https://github.com/tropic-hal/libtropic)
- [Embedded Systems Graceful Degradation Patterns](https://www.embedded.com/design/prototyping-and-development/4024616/Graceful-degradation-in-embedded-systems)
