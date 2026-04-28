---
title: "[MEDIUM] PinManager::isLockoutActive() modifies state via const_cast without synchronization"
severity: MEDIUM
domain: concurrency
lens: resource-contention
labels:
  - "audit:concurrency/resource-contention"
---

## Summary
The `PinManager::isLockoutActive()` method (line 646-661 in `components/cdc_core/src/PinManager.cpp`) modifies mutable state (`lockoutActive_`, `badgeRetries_`) using `const_cast` while being declared as a `const` function. This lazy-update pattern creates a specific race condition: when lockout expires, multiple concurrent callers can all detect expiration and each call `saveToStorage()`, causing redundant NVS writes.

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
        const_cast<PinManager*>(this)->saveToStorage();  // Slow NVS write!
        LOG_I(TAG, "Lockout expired, retries reset");
        return false;
    }
    return true;
}
```

## Impact
1. **Redundant NVS writes**: Multiple concurrent callers detecting expiration each trigger a full NVS commit (10-50ms each).
2. **NVS wear**: Unnecessary writes contribute to flash wear leveling overhead.
3. **Blocking**: Each `saveToStorage()` blocks the calling task; multiple concurrent calls serialize on NVS.
4. **Const correctness violation**: `isLockoutActive()` is a getter but has side effects (lazy state update + I/O).

## Evidence
- File: `components/cdc_core/src/PinManager.cpp:646-661` (isLockoutActive implementation)
- File: `components/cdc_core/src/PinManager.cpp:154-214` (saveToStorage with NVS commit)
- Called from `isBadgeBlocked()` (line 607) which is also `const`
- NVS `nvs_commit()` is blocking and can take 10-50ms

## Recommended Fix
Make `isLockoutActive()` non-const and add mutex protection (complements the broader PinManager mutex fix in issue 006):

```cpp
// In PinManager.h - make non-const
bool isLockoutActive();  // Remove const

// In PinManager.cpp - protect with mutex
bool PinManager::isLockoutActive() {
    if (mutex_) xSemaphoreTake(mutex_, portMAX_DELAY);
    
    bool result = false;
    if (lockoutActive_) {
        uint32_t remaining = getLockoutRemainingMs();
        if (remaining == 0) {
            lockoutActive_ = false;
            badgeRetries_ = MAX_RETRIES;
            saveToStorage();  // Now serialized by mutex
            result = false;
        } else {
            result = true;
        }
    }
    
    if (mutex_) xSemaphoreGive(mutex_);
    return result;
}
```

## References
- Const correctness: https://isocpp.org/wiki/faq/const-correctness
- NVS performance: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
