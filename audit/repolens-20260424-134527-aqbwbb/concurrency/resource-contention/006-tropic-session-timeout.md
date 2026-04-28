---
title: "[LOW] TROPIC01 session management lacks timeout for long operations"
severity: LOW
domain: resource-contention
lens: concurrency
labels:
  - "audit:concurrency/resource-contention"
---

## Summary
The TROPIC01 secure element uses a session-based locking mechanism where `sessionStart()` acquires a session and `sessionEnd()` releases it. However, if an operation takes too long (e.g., during a full display refresh), the session may timeout on the TROPIC01 chip side while the ESP32 still holds the lock, causing subsequent operations to fail.

**Locations:**
- `components/cdc_hal/src/Tropic01Element.cpp:192-230` - sessionStart()
- `components/cdc_hal/src/Tropic01Element.cpp:232-246` - sessionEnd()
- `components/cdc_hal/src/Tropic01Element.cpp:271-290` - ensureSession()

## Impact
1. **Session Timeout**: If the ESP32 holds the session lock while waiting for other operations (e.g., display refresh), the TROPIC01 chip may timeout the session.
2. **Stale Lock State**: The ESP32 mutex is released, but the TROPIC01 session may be in an inconsistent state.
3. **Operation Failures**: Subsequent operations may fail with `LT_L2_NO_SESSION` errors.

**Evidence:**
```cpp
// Tropic01Element.cpp:sessionStart (lines 192-230)
bool Tropic01Element::sessionStart() {
    lock();  // ESP32 mutex
    
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
        return false;
    }
    
    sessionActive_ = true;
    eccCacheValid_ = false;
    
    // Enable chip auto-sleep mode
    uint32_t sleepCfg = 0;
    if (lt_r_config_read(&handle_, TR01_CFG_SLEEP_MODE_ADDR, &sleepCfg) == LT_OK) {
        if (!(sleepCfg & 0x01)) {
            sleepCfg |= 0x01;
            if (lt_r_config_write(&handle_, TR01_CFG_SLEEP_MODE_ADDR, sleepCfg) == LT_OK) {
                LOG_I(TAG, "Auto-sleep enabled");
            }
        }
    }
    
    LOG_I(TAG, "Secure session active");
    unlock();  // ESP32 mutex released
    return true;
}

// Tropic01Element.cpp:sessionEnd (lines 232-246)
void Tropic01Element::sessionEnd() {
    lock();  // ESP32 mutex
    
    if (!sessionActive_) {
        unlock();
        return;
    }
    
    lt_session_abort(&handle_);  // Abort session
    sessionActive_ = false;
    LOG_I(TAG, "Session ended");
    unlock();  // ESP32 mutex released
}

// Tropic01Element.cpp:ensureSession (lines 271-290)
bool Tropic01Element::ensureSession(const char* op) {
    if (sessionActive_) return true;
    LOG_W(TAG, "Session inactive for %s - restarting", op);
    return sessionStart();
}
```

**Race condition sequence:**
```
Thread A (Display):               Thread B (TROPIC01):
    flushSync(PARTIAL)                sessionStart()
        s_epd_display->updateWindow()     lock()
            // Display refresh takes 8-15s       lt_verify_chip_and_start_secure_session()
            // TROPIC01 session times out         sessionActive_ = true
                                                    unlock()
    
    // 10s later
    verifyBadgePin()
        sessionStart()
            lock()
            sessionActive_ = true (still true from before!)
            unlock()
            return true
    
        verifyBadgePin() → calls rmemRead()
            lock()
            ensureSession("rmemRead")
                sessionActive_ is true → returns true
            lt_r_mem_data_read()
                // TROPIC01 session expired!
                ret = LT_L2_NO_SESSION
            handleSessionError(ret)
                sessionActive_ = false
            unlock()
```

The issue is that `sessionActive_` is a local state variable. The TROPIC01 chip may have timed out the session, but the ESP32 doesn't know until it tries an operation.

**Another scenario - nested operations:**
```
Thread A (TROPIC01):
    sessionStart()
        lock()
        lt_verify_chip_and_start_secure_session()
        sessionActive_ = true
        unlock()
    
    rmemRead()
        lock()
        ensureSession("rmemRead") → sessionActive_ is true
        lt_r_mem_data_read()
        unlock()
    
    // Thread A is blocked waiting for user input
    // TROPIC01 session times out after ~60s (configurable)
    
Thread B (TROPIC01):
    sessionStart()
        lock()
        sessionActive_ is true → return true (no new session started!)
        unlock()
    
    eccGenerate()
        lock()
        ensureSession("eccGenerate") → sessionActive_ is true
        lt_ecc_key_generate()
            // TROPIC01 session expired!
            ret = LT_L2_NO_SESSION
        handleSessionError(ret)
            sessionActive_ = false
        unlock()
```

## Recommended Fix
Add session timeout checking and lazy session refresh:

1. **Track session start time:**
   ```cpp
   class Tropic01Element {
       bool sessionActive_ = false;
       uint32_t sessionStartMs_ = 0;
       static constexpr uint32_t SESSION_TIMEOUT_MS = 55000;  // 55s (TROPIC01 default is 60s)
   };
   ```

2. **Add session timeout check:**
   ```cpp
   bool Tropic01Element::ensureSession(const char* op) {
       if (sessionActive_) {
           // Check if session might have timed out
           uint32_t elapsed = esp_timer_get_time() / 1000 - sessionStartMs_;
           if (elapsed > SESSION_TIMEOUT_MS) {
               LOG_W(TAG, "Session likely timed out (%lu ms elapsed)", elapsed);
               sessionActive_ = false;
           }
       }
       
       if (sessionActive_) return true;
       LOG_W(TAG, "Session inactive for %s - restarting", op);
       return sessionStart();
   }
   
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
           return false;
       }
       
       sessionActive_ = true;
       sessionStartMs_ = esp_timer_get_time() / 1000;  // Record start time
       eccCacheValid_ = false;
       
       unlock();
       return true;
   }
   ```

3. **Add session refresh method:**
   ```cpp
   /**
    * \brief Refresh session if near timeout.
    * \return `true` if session is active or refreshed.
    */
   bool Tropic01Element::refreshSession() {
       if (!sessionActive_) {
           return sessionStart();
       }
       
       uint32_t elapsed = esp_timer_get_time() / 1000 - sessionStartMs_;
       if (elapsed > SESSION_TIMEOUT_MS - 5000) {  // Refresh if < 5s left
           LOG_D(TAG, "Refreshing session (%lu ms elapsed)", elapsed);
           sessionEnd();
           return sessionStart();
       }
       
       return true;
   }
   ```

4. **Use refreshSession() in long operations:**
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
       
       // For long operations, check session periodically
       // (eccGenerate takes ~200ms, so not strictly necessary)
       
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
- [TROPIC01 Session Timeout](https://www.tropic.network/docs/tropic01/#session-management) - Default 60s timeout
- [ESP32 Timer API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/esp_timer.html) - esp_timer_get_time()
- [LibTropic Session Management](https://github.com/libTropic/libtropic/blob/master/src/libtropic_l3.c#L100) - lt_verify_chip_and_start_secure_session()
