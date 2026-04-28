---
title: "[MEDIUM] PinManager Concurrent Access Without Synchronization"
severity: MEDIUM
domain: resource-contention
lens: concurrency
labels:
  - audit:concurrency/resource-contention
---

## Summary

The `PinManager` singleton stores PIN retry counts and lockout state in member variables that are accessed from multiple contexts (UI task, serial commands, module callbacks) without any mutex protection. Concurrent PIN verification can corrupt retry counts.

**Location**: `components/cdc_core/src/PinManager.cpp:24-663`

## Impact

**Resource Contention Risk**: When multiple tasks verify PINs concurrently (e.g., UI keypad + serial command), the retry counters can be corrupted:

1. **Race condition**: Two threads read `badgeRetries_ = 3`, both decrement to 2, result is 2 instead of 1
2. **Lockout bypass**: `isLockoutActive()` check followed by `verifyBadgePin()` can be interleaved
3. **Data inconsistency**: `saveToStorage()` called with partial updates

**Evidence**:
- `PinManager.cpp:24-28`: Singleton pattern with no thread-safety
- `PinManager.cpp:300-329`: `verifyBadgePin()` reads/writes shared state
- `PinManager.cpp:412-435`: `verifyPW1()` same pattern
- Multiple code paths can call these simultaneously

## Evidence

**Singleton Pattern** (`components/cdc_core/src/PinManager.cpp:24-28`):
```cpp
PinManager& PinManager::instance() {
    static PinManager instance;
    return instance;
}
```

**Badge PIN Verification** (`components/cdc_core/src/PinManager.cpp:300-329`):
```cpp
bool PinManager::verifyBadgePin(const char* pin) {
    if (!pin) return false;
    if (!pinLoaded_) init();

    // Check if blocked (retries=0 or lockout active)
    if (isBadgeBlocked()) {  // <-- READ
        LOG_W(TAG, "Badge PIN blocked");
        return false;
    }

    uint8_t inputHash[BADGE_HASH_SIZE];
    if (!computeBadgeHash(pin, inputHash)) return false;

    if (compareHash(badgeHash_, inputHash, BADGE_HASH_SIZE)) {
        resetBadgeRetries();  // <-- WRITE
        lockoutActive_ = false;
        LOG_I(TAG, "Badge PIN verified");
        return true;
    }

    badgeRetries_--;  // <-- WRITE (not atomic!)
    saveToStorage();  // <-- I/O operation

    if (badgeRetries_ == 0) {  // <-- READ
        startLockout();
    }
    return false;
}
```

**Lockout Check** (`components/cdc_core/src/PinManager.cpp:612-619`):
```cpp
bool PinManager::isBadgeBlocked() const {
    // Blocked if retries exhausted AND lockout still active
    if (badgeRetries_ == 0) {
        return isLockoutActive();  // <-- Another READ
    }
    return false;
}
```

**Lockout Active Check** (`components/cdc_core/src/PinManager.cpp:648-663`):
```cpp
bool PinManager::isLockoutActive() const {
    if (!lockoutActive_) {
        return false;
    }

    uint32_t remaining = getLockoutRemainingMs();
    if (remaining == 0) {
        // Lockout expired - reset retries (const_cast needed for lazy update)
        const_cast<PinManager*>(this)->lockoutActive_ = false;  // <-- WRITE
        const_cast<PinManager*>(this)->badgeRetries_ = MAX_RETRIES;  // <-- WRITE
        const_cast<PinManager*>(this)->saveToStorage();  // <-- I/O
        LOG_I(TAG, "Lockout expired, retries reset");
        return false;
    }
    return true;
}
```

**Concurrent Access Scenarios**:
1. **UI + Serial**: User enters PIN on keypad while serial command verifies PIN
2. **Multiple modules**: FIDO2 and GPG modules both check badge PIN
3. **ISR + Task**: Event-driven PIN check from USB interrupt context

## Recommended Fix

1. **Add mutex protection**:
```cpp
// In PinManager.h
class PinManager {
private:
    // ... existing members ...
    SemaphoreHandle_t mutex_;

public:
    bool verifyBadgePin(const char* pin);
    // ... other methods ...
};

// In PinManager.cpp
bool PinManager::init() {
    if (pinLoaded_) return true;

    // Create mutex
    mutex_ = xSemaphoreCreateMutex();

    if (!loadFromStorage()) {
        loadDefaults();
    }

    pinLoaded_ = true;
    return true;
}

bool PinManager::verifyBadgePin(const char* pin) {
    if (!pin) return false;
    if (!pinLoaded_) init();

    xSemaphoreTake(mutex_, portMAX_DELAY);

    bool blocked = isBadgeBlocked();
    bool result = false;

    if (!blocked) {
        uint8_t inputHash[BADGE_HASH_SIZE];
        if (!computeBadgeHash(pin, inputHash)) {
            xSemaphoreGive(mutex_);
            return false;
        }

        if (compareHash(badgeHash_, inputHash, BADGE_HASH_SIZE)) {
            resetBadgeRetriesLocked();  // New method (assumes lock)
            lockoutActive_ = false;
            result = true;
        } else {
            badgeRetries_--;
            saveToStorage();

            if (badgeRetries_ == 0) {
                startLockoutLocked();  // New method
            }
        }
    }

    xSemaphoreGive(mutex_);
    return result;
}

// Helper methods that assume lock is held
void PinManager::resetBadgeRetriesLocked() {
    badgeRetries_ = MAX_RETRIES;
    saveToStorage();
}

void PinManager::startLockoutLocked() {
    lockoutStartMs_ = esp_timer_get_time() / 1000;
    lockoutActive_ = true;
}
```

2. **Add timeout support**:
```cpp
bool PinManager::verifyBadgePin(const char* pin, uint32_t timeoutMs = 1000) {
    if (!pin) return false;
    if (!pinLoaded_) init();

    TickType_t start = xTaskGetTickCount();
    TickType_t timeout = pdMS_TO_TICKS(timeoutMs);

    while (xSemaphoreTake(mutex_, timeout) == pdFALSE) {
        if (xTaskGetTickCount() - start > timeout) {
            LOG_W(TAG, "PIN manager mutex timeout");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // ... same logic ...
    xSemaphoreGive(mutex_);
    return result;
}
```

3. **Consider atomic operations** for simple counters:
```cpp
#include <atomic>

class PinManager {
private:
    std::atomic<uint8_t> badgeRetries_{MAX_RETRIES};
    std::atomic<bool> lockoutActive_{false};
    // ...
};
```

## References

- FreeRTOS: Mutex for protecting shared data
- ESP-IDF: Task synchronization patterns
- Concurrency: Critical sections for PIN verification
