---
title: "[LOW] OpenPGP VERIFY command lacks rate limiting for brute-force protection"
severity: LOW
domain: authorization
lens: openpgp-pin
labels:
  - "openpgp"
  - "pin-verification"
---

## Summary

The OpenPGP VERIFY command (for PIN verification) in `components/mod_gpg/src/openpgp/openpgp.cpp` checks PIN retries but lacks **rate limiting** or **delay** between attempts. This allows rapid brute-force attacks on the PW1 (User PIN) and PW3 (Admin PIN).

The VERIFY command handler (around line 1075-1100):
```cpp
case INS_VERIFY:  // VERIFY PIN
    uint8_t p1 = apdu->p1;  // 0x00 = PW1, 0x80 = PW3
    const char* pin = (const char*)apdu->data;
    size_t pinLen = apdu->lc;
    
    bool verified = false;
    if (p1 == 0x00) {  // PW1
        verified = pin_storage_openpgp_verify_pw1(pin);
        if (verified) pw1_verified = true;
    } else if (p1 == 0x80) {  // PW3
        verified = pin_storage_openpgp_verify_pw3(pin);
        if (verified) pw3_verified = true;
    }
    // ...
```

The PIN verification uses `PinManager::verifyPW1()` and `verifyPW3()` which decrement retry counters, but there's no **time-based delay** between attempts.

## Impact

1. **Rapid brute-force attacks** - An attacker connected via USB CCID can try all 100,000 possible 6-digit PW1 PINs in seconds (limited only by USB round-trip time).

2. **No progressive delay** - Unlike the Badge PIN which has a 60-second lockout after 3 attempts, the OpenPGP PINs only have a retry counter with no time-based component.

3. **Easy enumeration** - With ~10ms per attempt, all 100,000 6-digit combinations can be tested in ~17 minutes.

## Evidence

**File: `components/mod_gpg/src/openpgp/openpgp.cpp`**
- Lines 1075-1100: VERIFY PIN command handler
- Line 1084: `pw1_verified = true` on success
- Line 1095: `pw3_verified = true` on success

**File: `components/mod_gpg/src/pin_storage.cpp`**
- Lines 8-10: `pin_storage_openpgp_verify_pw1()` calls `PinManager::verifyPW1()`
- Lines 12-14: `pin_storage_openpgp_verify_pw3()` calls `PinManager::verifyPW3()`

**File: `components/cdc_core/src/PinManager.cpp`**
- The `verifyPW1()` and `verifyPW3()` methods decrement retry counters but have no delay logic

## Recommended Fix

1. **Add progressive delay** - Implement increasing delay between failed attempts:
   ```cpp
   static void addPinDelay(int failedAttempts) {
       uint32_t delayMs = 100;  // Base delay
       for (int i = 0; i < failedAttempts; i++) {
           delayMs *= 2;  // Exponential backoff
       }
       vTaskDelay(pdMS_TO_TICKS(delayMs));
   }
   
   case INS_VERIFY:
       // ...
       if (!verified) {
           int retries = (p1 == 0x00) ? pin_storage_openpgp_pw1_retries() 
                                      : pin_storage_openpgp_pw3_retries();
           int failed = 3 - retries;  // Calculate failed attempts
           addPinDelay(failed);
       }
   ```

2. **Add global delay counter** - Track total failed attempts across all sessions:
   ```cpp
   static uint32_t s_totalFailedAttempts = 0;
   
   // Increment on failure
   if (!verified) {
       s_totalFailedAttempts++;
       uint32_t delayMs = 100 * s_totalFailedAttempts;
       vTaskDelay(pdMS_TO_TICKS(delayMs));
   }
   ```

3. **Consider time-based lockout** similar to Badge PIN:
   ```cpp
   static uint32_t s_lastPinAttemptMs = 0;
   static constexpr uint32_t PIN_DELAY_MS = 1000;  // 1 second minimum
   
   if (s_lastPinAttemptMs > 0) {
       uint32_t elapsed = esp_timer_get_time() / 1000 - s_lastPinAttemptMs;
       if (elapsed < PIN_DELAY_MS) {
           vTaskDelay(pdMS_TO_TICKS(PIN_DELAY_MS - elapsed));
       }
   }
   s_lastPinAttemptMs = esp_timer_get_time() / 1000;
   ```

## References

- [OWASP Authentication Cheat Sheet - Password Brute Force](https://cheatsheetseries.owasp.org/cheatsheets/Authentication_Cheat_Sheet.html#brute-force)
- [NIST SP 800-63B - Impersonation Attacks](https://pages.nist.gov/800-63-3/sp800-63b.html#impersonation)
- [OpenPGP Card Specification 3.4.1 - PIN Management](https://g10code.com/docs/openpgp-card-3.4.pdf)
