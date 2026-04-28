---
title: "[MEDIUM] PIN retry counter updates have race condition between read and write operations"
severity: MEDIUM
domain: transaction-concurrency
lens: transaction-concurrency
labels:
  - audit:concurrency/transaction-concurrency
---

## Summary

In `components/cdc_core/src/PinManager.cpp`, the PIN retry counter is updated using a read-modify-write pattern that spans multiple function calls. When `verifyBadgePin()`, `verifyPW1()`, or `verifyPW3()` are called concurrently (e.g., from different UI tasks or serial command handlers), the retry counters can be corrupted, leading to incorrect lockout behavior or lost retry decrements.

**Location:** `components/cdc_core/src/PinManager.cpp:300-330, 410-430, 510-530`

The pattern appears in three verification methods:
1. `verifyBadgePin()` - Line 300-330
2. `verifyPW1()` - Line 410-430
3. `verifyPW3()` - Line 510-530

## Impact

**Race Condition on PIN Retries:**

Consider two concurrent badge PIN verification attempts:
1. Task A calls `verifyBadgePin("wrong1")` - reads `badgeRetries_ = 3`
2. Task B calls `verifyBadgePin("wrong2")` - reads `badgeRetries_ = 3`
3. Task A decrements to 2, writes to NVS
4. Task B decrements to 2, writes to NVS (overwrites A's update)
5. Result: User has 2 retries left instead of 1

**Consequences:**
- **Security impact**: More verification attempts than intended before lockout
- **User experience**: Confusing "wrong PIN" behavior when retries seem to not decrease
- **Lockout timing**: Lockout may trigger at wrong time, or not at all

**Additional concern in `isLockoutActive()` (lines 645-663):**

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
        const_cast<PinManager*>(this)->saveToStorage();  // Line 662
        LOG_I(TAG, "Lockout expired, retries reset");
        return false;
    }
    return true;
}
```

This method modifies state (`lockoutActive_`, `badgeRetries_`) and writes to storage, all without synchronization.

## Evidence

**verifyBadgePin() - Non-atomic retry update:**
```cpp
// Line 300-330: components/cdc_core/src/PinManager.cpp
bool PinManager::verifyBadgePin(const char* pin) {
    if (!pin) return false;
    if (!pinLoaded_) init();

    // Check if blocked (retries=0 or lockout active)
    if (isBadgeBlocked()) {
        LOG_W(TAG, "Badge PIN blocked");
        return false;
    }

    uint8_t inputHash[BADGE_HASH_SIZE];
    if (!computeBadgeHash(pin, inputHash)) return false;

    if (compareHash(badgeHash_, inputHash, BADGE_HASH_SIZE)) {
        resetBadgeRetries();
        lockoutActive_ = false;  // Clear lockout on success
        LOG_I(TAG, "Badge PIN verified");
        return true;
    }

    badgeRetries_--;
    saveToStorage();  // Line 327 - Non-atomic decrement and write
    LOG_W(TAG, "Wrong badge PIN, %d retries left", badgeRetries_);

    // Start lockout timer when retries exhausted
    if (badgeRetries_ == 0) {
        startLockout();
    }
    return false;
}
```

**saveToStorage() - Writes entire PIN state to NVS:**
```cpp
// Line 155-214: components/cdc_core/src/PinManager.cpp
bool PinManager::saveToStorage() {
    hal::ISecureElement* se = hal::getSecureElementInstance();
    if (!se || !se->isSessionActive()) {
        LOG_E(TAG, "SE session not active");
        return false;
    }

    uint8_t data[STORAGE_SIZE];
    size_t pos = 0;

    data[pos++] = MAGIC_V3;
    data[pos++] = badgeRetries_;  // Writes badgeRetries_
    // ... writes all PIN state ...
    
    se->rmemErase(RMEM_SLOT_PIN);
    hal::SeResult result = se->rmemWrite(RMEM_SLOT_PIN, data, STORAGE_SIZE);  // Line 211
    // ...
}
```

**Multiple callers can invoke simultaneously:**
- UI task (PinEntryView)
- Serial command handler
- BLE command handler
- Background sync task

## Recommended Fix

**Add a mutex to protect PIN state modifications:**

```cpp
// In PinManager.h (add to private section):
#include "freertos/semphr.h"

private:
    // ... existing members ...
    mutable SemaphoreHandle_t mutex_ = nullptr;  // Add this

    // In init() method:
    bool PinManager::init() {
        if (pinLoaded_) return true;
        
        // Create mutex if not exists
        if (!mutex_) {
            mutex_ = xSemaphoreCreateRecursiveMutex();
        }
        
        if (!loadFromStorage()) {
            LOG_W(TAG, "No PINs stored, using defaults");
            loadDefaults();
        }
        pinLoaded_ = true;
        return true;
    }

    // Wrap verifyBadgePin with mutex:
    bool PinManager::verifyBadgePin(const char* pin) {
        if (!mutex_) return false;
        
        xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);
        
        bool result = verifyBadgePinInternal(pin);
        
        xSemaphoreGiveRecursive(mutex_);
        return result;
    }
    
    // Rename existing implementation to verifyBadgePinInternal:
    bool PinManager::verifyBadgePinInternal(const char* pin) {
        // Existing implementation without mutex
        // ...
    }

    // Same pattern for verifyPW1(), verifyPW3(), isLockoutActive()
```

**Alternative: Use atomic operations for simple counters**

For just the retry counters, consider using ESP32's atomic operations:

```cpp
#include "esp_atomic.h"

// Change member to atomic
volatile uint8_t badgeRetries_ = MAX_RETRIES;

// Atomic decrement
bool PinManager::decrementBadgeRetries() {
    uint8_t oldVal = badgeRetries_;
    uint8_t newVal;
    do {
        if (oldVal == 0) return false;  // Already at 0
        newVal = oldVal - 1;
    } while (!atomic_compare_exchange(&badgeRetries_, &oldVal, newVal));
    
    saveToStorage();
    return true;
}
```

## References

1. **ESP-IDF Atomics** - [atomic_compare_exchange](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/atomics.html)
2. **FreeRTOS Mutex** - [Recursive mutexes](https://www.freertos.org/a00127.html)
3. **PIN Security Best Practices** - NIST SP 800-63B Section 5.1.1.1 on authentication events
4. **Time-of-check to time-of-use (TOCTOU)** - Classic concurrency bug pattern
