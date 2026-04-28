---
title: "[MEDIUM] TROPIC01 session restart failures handled silently"
severity: MEDIUM
domain: error-path-tests
lens: secure-element
labels:
  - "tropic01"
  - "session-management"
  - "secure-element"
---

## Summary
In `components/cdc_hal/src/Tropic01Element.cpp` (lines 277-298 and 337-340), when `ensureSession()` fails to restart a TROPIC01 secure element session, some code paths return the error code but others proceed as if the session is valid. This can lead to operations on an invalid session returning confusing errors.

**Files:**
- `components/cdc_hal/src/Tropic01Element.cpp:277-298`
- `components/cdc_hal/src/Tropic01Element.cpp:337-340`
- `components/cdc_hal/src/Tropic01Element.cpp:380-385`

## Impact
1. **Silent Session Loss**: Operations may succeed on first try but fail silently on retry
2. **Data Corruption**: ECDSA key operations on invalid session produce wrong signatures
3. **Authentication Failures**: FIDO2 credentials may fail to sign without clear error
4. **Debug Difficulty**: Session errors masked as generic "operation failed"
5. **Security**: Stale session might use wrong key slot

## Evidence
From `components/cdc_hal/src/Tropic01Element.cpp`:

```cpp
// Lines 277-298: handleSessionError() - maps errors but doesn't guarantee recovery
void handleSessionError(lt_ret_t error) {
    switch (error) {
        case LT_L2_SESSION_ERROR:
        case LT_L2_SESSION_TERMINATED:
            LOG_W("TROPIC", "Session invalid, will retry");
            state = SE_STATE_SESSION_REQUIRED;
            break;
        case LT_L2_ALARM_MODE:
            LOG_E("TROPIC", "Chip in alarm mode");
            state = SE_STATE_ALARM;
            break;
        // ...
    }
}

// Lines 337-340: ensureSession() - can return SESSION_REQUIRED
SeResult ensureSession() {
    if (state == SE_STATE_SESSION_REQUIRED) {
        if (startSession() != LT_RET_OK) {
            return SeResult::SESSION_REQUIRED;  // ❌ Caller must handle
        }
    }
    return SeResult::OK;
}

// Lines 380-385: generateKey() - handles error but caller might not check
SeResult generateKey(uint8_t slot) {
    SeResult sessionResult = ensureSession();
    if (sessionResult != SeResult::OK) {
        return sessionResult;  // Returns error
    }
    
    lt_ret_t result = lt_ecc_key_generate(slot);
    if (result != LT_RET_OK) {
        handleSessionError(result);  // Sets state but returns mapped result
        return mapResult(result);
    }
    // ...
}

// Usage example - caller might ignore return value:
// In Fido2Module.cpp or similar
tropicElement->generateKey(slot);  // ❌ No error check!
```

**Problem:**
- `ensureSession()` returns error but some callers ignore it
- `handleSessionError()` sets state but doesn't block operations
- Session restart can fail silently in nested calls
- No timeout/retry limit on session recovery

## Recommended Fix
Ensure session errors are always checked and handled:

1. **Add retry limit to session recovery**:
   ```cpp
   #define TROPIC_SESSION_MAX_RETRIES 3
   
   SeResult Tropic01Element::ensureSession() {
       if (state == SE_STATE_SESSION_REQUIRED) {
           for (int i = 0; i < TROPIC_SESSION_MAX_RETRIES; i++) {
               if (startSession() == LT_RET_OK) {
                   return SeResult::OK;
               }
               LOG_W("TROPIC", "Session start attempt %d failed", i + 1);
               vTaskDelay(pdMS_TO_TICKS(10));
           }
           LOG_E("TROPIC", "Session start failed after %d retries", 
                 TROPIC_SESSION_MAX_RETRIES);
           return SeResult::SESSION_REQUIRED;
       }
       return SeResult::OK;
   }
   ```

2. **Add wrapper with automatic retry**:
   ```cpp
   template<typename Func>
   SeResult withSessionRetry(Func operation) {
       for (int i = 0; i < TROPIC_SESSION_MAX_RETRIES; i++) {
           SeResult sessionResult = ensureSession();
           if (sessionResult != SeResult::OK) {
               return sessionResult;
           }
           
           SeResult opResult = operation();
           if (opResult != SeResult::OK && opResult != SeResult::SESSION_REQUIRED) {
               return opResult;
           }
           // Session may have expired, retry
           vTaskDelay(pdMS_TO_TICKS(5));
       }
       return SeResult::SESSION_REQUIRED;
   }
   
   // Usage:
   SeResult generateKey(uint8_t slot) {
       return withSessionRetry([this, slot]() {
           lt_ret_t result = lt_ecc_key_generate(slot);
           if (result != LT_RET_OK) {
               handleSessionError(result);
               return SeResult::SESSION_REQUIRED;
           }
           return SeResult::OK;
       });
   }
   ```

3. **Add assertion for return value checking**:
   ```cpp
   // In DEBUG mode, check all TROPIC calls
   #ifdef DEBUG_MODE
   #define TROPIC_CHECK(call) \
       do { \
           SeResult _r = call; \
           if (_r != SeResult::OK) { \
               LOG_D("TROPIC", "Check: %s returned %d", #call, _r); \
           } \
       } while(0)
   #endif
   ```

4. **Add test cases**:
   - Mock `startSession()` to fail 2 times, then succeed
   - Verify retry logic works
   - Mock `startSession()` to fail 4 times
   - Verify max retry limit is respected
   - Test session expiration during operation

## References
- TROPIC01 Datasheet: https://www.microchip.com/en-us/product/tropic01
- LibTropic Documentation: https://github.com/LibTropic/libtropic
- Secure Element Session Management: https://www.microchip.com/content/dam/mchp/documents/TROPIC/ProductDocuments/DataSheets/TROPIC01-DS-00000889.pdf

</content>