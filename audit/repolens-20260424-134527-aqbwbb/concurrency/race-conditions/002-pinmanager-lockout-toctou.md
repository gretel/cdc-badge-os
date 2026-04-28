---
title: "[MEDIUM] PinManager Lockout TOCTOU - Check-Then-Act Race on Lockout Timer"
severity: MEDIUM
domain: concurrency
lens: race-conditions
labels:
  - "audit:concurrency/race-conditions"
---

## Summary

The `PinManager` class in `components/cdc_core/src/PinManager.cpp` has a Time-of-Check-Time-of-Use (TOCTOU) race condition in the lockout logic. The `isLockoutActive()` method (line 648-663) checks if lockout is active and lazily clears expired lockout, but this check-and-update sequence is not atomic.

**Evidence** - `isLockoutActive()` (lines 648-663):
```cpp
bool PinManager::isLockoutActive() const {
    if (!lockoutActive_) {
        return false;
    }

    uint32_t remaining = getLockoutRemainingMs();
    if (remaining == 0) {
        // Lockout expired - reset retries (const_cast needed for lazy update)
        const_cast<PinManager*>(this)->lockoutActive_ = false;
        const_cast<PinManager*>(this)->badgeRetries_ = MAX_RETRIES;
        const_cast<PinManager*>(this)->saveToStorage();  // Async operation
        LOG_I(TAG, "Lockout expired, retries reset");
        return false;
    }
    return true;
}
```

**Evidence** - `isBadgeBlocked()` (lines 601-607):
```cpp
bool PinManager::isBadgeBlocked() const {
    if (badgeRetries_ == 0) {
        return isLockoutActive();  // TOCTOU: lockout can expire between checks
    }
    return false;
}
```

**Evidence** - `verifyBadgePin()` (lines 299-329):
```cpp
bool PinManager::verifyBadgePin(const char* pin) {
    // ...
    if (isBadgeBlocked()) {  // Check 1
        LOG_W(TAG, "Badge PIN blocked");
        return false;
    }
    // ...
    if (compareHash(badgeHash_, inputHash, BADGE_HASH_SIZE)) {
        resetBadgeRetries();
        lockoutActive_ = false;  // Check 2 - lockout state can change between checks
        LOG_I(TAG, "Badge PIN verified");
        return true;
    }
    // ...
}
```

The race occurs because:
1. Thread A calls `isBadgeBlocked()`, sees lockout is active
2. Thread B calls `isBadgeBlocked()`, lockout expires, clears lockout and saves to storage (async)
3. Thread A continues, sees lockout cleared, proceeds to verify PIN
4. Meanwhile, storage save completes with stale data

## Impact

**Security Bypass**: In a multi-core ESP32-S3 scenario:
1. User enters wrong PIN 3 times, lockout starts (60s)
2. On Core 0, lockout expires after 60s, `isLockoutActive()` clears it
3. On Core 1, another PIN entry happens almost simultaneously
4. The check-then-act window allows bypassing lockout if timing aligns

**Data Inconsistency**: The lazy update pattern with `const_cast` means:
- `saveToStorage()` is async and can take milliseconds
- Another thread can read stale `badgeRetries_` value
- NVS write can be interleaved with other operations

## Recommended Fix

Add a mutex to protect the lockout state and make the check-then-clear atomic:

```cpp
// In PinManager.h - add mutex
#include <mutex>

class PinManager {
private:
    mutable std::mutex lockoutMutex_;  // Add this
    bool lockoutActive_ = false;
    uint32_t lockoutStartMs_ = 0;
    // ...
public:
    bool isBadgeBlocked() const;
    bool isLockoutActive() const;
    void startLockout();
    uint32_t getLockoutRemainingMs() const;
};

// In PinManager.cpp - isBadgeBlocked()
bool PinManager::isBadgeBlocked() const {
    std::lock_guard<std::mutex> lock(lockoutMutex_);
    if (badgeRetries_ == 0) {
        // Check and update atomically
        if (lockoutActive_) {
            uint32_t remaining = getLockoutRemainingMs();
            if (remaining == 0) {
                lockoutActive_ = false;
                badgeRetries_ = MAX_RETRIES;
                saveToStorage();
                LOG_I(TAG, "Lockout expired, retries reset");
                return false;
            }
            return true;
        }
    }
    return false;
}

// In PinManager.cpp - isLockoutActive()
bool PinManager::isLockoutActive() const {
    std::lock_guard<std::mutex> lock(lockoutMutex_);
    if (!lockoutActive_) {
        return false;
    }

    uint32_t remaining = getLockoutRemainingMs();
    if (remaining == 0) {
        lockoutActive_ = false;
        const_cast<PinManager*>(this)->badgeRetries_ = MAX_RETRIES;
        const_cast<PinManager*>(this)->saveToStorage();
        LOG_I(TAG, "Lockout expired, retries reset");
        return false;
    }
    return true;
}
```

Alternatively, use FreeRTOS mutex for consistency with the codebase:
```cpp
static portMUX_TYPE lockoutMux_ = portMUX_INITIALIZER_UNLOCKED;
// Use portENTER_CRITICAL/portEXIT_CRITICAL instead
```

## References

- TOCTOU vulnerability pattern: https://cwe.mitre.org/data/definitions/367.html
- ESP32-S3 SMP considerations: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/freertos-smp.html
- Lockout security best practices: https://cheatsheetseries.owasp.org/cheatsheets/Authentication_Cheat_Sheet.html
