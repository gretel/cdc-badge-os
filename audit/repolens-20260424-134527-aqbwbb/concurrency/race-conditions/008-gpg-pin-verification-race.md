---
title: "[MEDIUM] GPG PIN Verification Read-Modify-Write Race Condition"
severity: MEDIUM
domain: concurrency
lens: race-conditions
labels:
  - "audit:concurrency/race-conditions"
---

## Summary

The GPG module's PIN verification workflow (`components/mod_gpg/src/openpgp/openpgp.cpp`) performs read-modify-write operations on PIN retry counters without atomic protection. Multiple concurrent PIN verification attempts can lead to incorrect retry counts or race conditions where the same PIN attempt is counted multiple times.

**Location**: `components/mod_gpg/src/openpgp/openpgp.cpp` (lines 1053-1100) and `components/mod_gpg/src/pin_storage.cpp`

## Impact

**Security Risk**: If two PIN verification requests arrive concurrently (e.g., from USB and BLE simultaneously, or from different FIDO2 operations), the retry counter could be decremented incorrectly, potentially:
1. Allowing more attempts than intended before lockout
2. Locking out the user prematurely
3. Causing inconsistent state between memory and storage

**User Experience**: Users may experience unpredictable lockout behavior where they lose attempts without knowing why.

## Evidence

### Pattern 1: Non-Atomic Read-Modify-Write in Verification

`components/mod_gpg/src/openpgp/openpgp.cpp` line 1082:
```cpp
// PW1 (User PIN) verification
verified = pin_storage_openpgp_verify_pw1(pin_str);
...
retries = pin_storage_openpgp_pw1_retries();
```

`components/mod_gpg/src/pin_storage.cpp` line 8:
```cpp
bool pin_storage_openpgp_verify_pw1(const char *pin) {
    return cdc::core::PinManager::instance().verifyPW1(pin);
}
```

`components/cdc_core/src/PinManager.cpp` line 411:
```cpp
bool PinManager::verifyPW1(const char* pin) {
    if (!pin) return false;
    if (!pinLoaded_) init();
    if (pw1Retries_ == 0) {
        LOG_W(TAG, "PW1 blocked");
        return false;
    }

    uint8_t inputHash[KDF_HASH_SIZE];
    if (!computeKdfHash(pin, pw1Salt_, inputHash)) return false;

    if (compareHash(pw1Hash_, inputHash, KDF_HASH_SIZE)) {
        resetPW1Retries();
        LOG_I(TAG, "PW1 verified");
        return true;
    }

    pw1Retries_--;
    saveToStorage();  // Persist retry count
    LOG_W(TAG, "Wrong PW1, %d retries left", pw1Retries_);
    return false;
}
```

### Pattern 2: Separate Read Operations

`components/mod_gpg/src/openpgp/openpgp.cpp` line 1053-1061:
```cpp
if (pin_storage_openpgp_pw1_blocked()) {
    // ...
    retries = pin_storage_openpgp_pw1_retries();
}
...
if (pin_storage_openpgp_pw3_blocked()) {
    // ...
    retries = pin_storage_openpgp_pw3_retries();
}
```

The `pin_storage_openpgp_pw1_retries()` function reads the current value:
```cpp
uint8_t pin_storage_openpgp_pw1_retries(void) {
    return cdc::core::PinManager::instance().getPW1Retries();
}
```

### Race Condition Window

1. Thread A calls `verifyPW1("123456")` - checks retries > 0
2. Thread B calls `verifyPW1("123456")` - checks retries > 0 (same value, still > 0)
3. Thread A computes hash, fails, decrements retries to 2, saves
4. Thread B computes hash, fails, decrements retries to 1, saves (should be 1, but both threads thought it was 2)
5. Result: 2 attempts consumed but user only made 1 physical attempt

## Recommended Fix

### Option 1: Add Mutex Protection to PinManager

Add a mutex to `PinManager` to protect retry operations:

`components/cdc_core/include/cdc_core/PinManager.h`:
```cpp
#include "freertos/semphr.h"

class PinManager {
private:
    // ... existing members ...
    static portMUX_TYPE s_mux;  // For atomic retry updates
};
```

`components/cdc_core/src/PinManager.cpp`:
```cpp
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

bool PinManager::verifyPW1(const char* pin) {
    if (!pin) return false;
    if (!pinLoaded_) init();

    // Atomic check-and-decrement
    portENTER_CRITICAL(&s_mux);
    if (pw1Retries_ == 0) {
        portEXIT_CRITICAL(&s_mux);
        LOG_W(TAG, "PW1 blocked");
        return false;
    }

    portEXIT_CRITICAL(&s_mux);

    uint8_t inputHash[KDF_HASH_SIZE];
    if (!computeKdfHash(pin, pw1Salt_, inputHash)) return false;

    if (compareHash(pw1Hash_, inputHash, KDF_HASH_SIZE)) {
        portENTER_CRITICAL(&s_mux);
        resetPW1Retries();
        portEXIT_CRITICAL(&s_mux);
        LOG_I(TAG, "PW1 verified");
        return true;
    }

    portENTER_CRITICAL(&s_mux);
    pw1Retries_--;
    uint8_t currentRetries = pw1Retries_;
    portEXIT_CRITICAL(&s_mux);

    saveToStorage();
    LOG_W(TAG, "Wrong PW1, %d retries left", currentRetries);
    return false;
}
```

### Option 2: Use Atomic Operations

Replace `uint8_t` retry counters with `std::atomic<uint8_t>`:

```cpp
#include <atomic>

class PinManager {
private:
    std::atomic<uint8_t> pw1Retries_{MAX_RETRIES};
    std::atomic<uint8_t> pw3Retries_{MAX_RETRIES};
    std::atomic<uint8_t> badgeRetries_{MAX_RETRIES};
};
```

Then update verification to use atomic operations:
```cpp
bool PinManager::verifyPW1(const char* pin) {
    if (!pin) return false;
    if (!pinLoaded_) init();

    // Atomic check
    uint8_t current = pw1Retries_.load();
    if (current == 0) {
        LOG_W(TAG, "PW1 blocked");
        return false;
    }

    // Atomic decrement (returns previous value)
    uint8_t old = pw1Retries_.fetch_sub(1);
    if (old == 1) {  // Was 1, now 0 - just exhausted
        startLockout();
    }

    // ... rest of verification ...
}
```

### Option 3: Combine Read-Modify-Write in Single Call

Modify the API to return both result and current retry count:
```cpp
struct PinVerifyResult {
    bool verified;
    uint8_t retriesLeft;
    bool wasBlocked;
};

PinVerifyResult verifyPW1(const char* pin);
```

This ensures callers get a consistent snapshot.

## References

- ESP-IDF Critical Sections: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/critical_section.html
- FreeRTOS portMUX: https://www.freertos.org/a00111.html
- C++11 std::atomic: https://en.cppreference.com/w/cpp/atomic/atomic
- Time-of-check-time-of-use (TOCTOU): https://cwe.mitre.org/data/definitions/367.html
