---
title: "[LOW] Multiple saveToStorage() calls in PIN change workflow cause redundant writes"
severity: LOW
domain: database/transaction-safety
lens: transaction-safety
labels:
  - "audit:database/transaction-safety"
---

## Summary

In `components/cdc_core/src/PinManager.cpp`, the PIN change workflow calls `saveToStorage()` multiple times for related updates, causing redundant NVS writes and increasing the window for partial-commit failures.

**Affected locations:**
- `changeBadgePin()` (lines 331-335): Calls `verifyBadgePin()` which calls `saveToStorage()`, then `setBadgePin()` which calls `saveToStorage()` again
- `setBadgePin()` (lines 341-377): Updates hash, retries, and `badgePinIsSet_` in one save
- `resetBadgeRetries()` (lines 374-380): Separate save just for retry counter

## Impact

**Performance and wear:**
- Each `saveToStorage()` triggers NVS commit, which can take 10-50ms
- Multiple commits increase total operation time
- Flash wear: NVS has limited write cycles (typically 100K-1M per page)

**Transaction scope issues:**
- First save: Badge PIN hash and retries updated
- Second save: `badgePinIsSet_` flag updated
- If power fails between saves, `badgePinIsSet_` may not reflect actual hash state

## Evidence

**changeBadgePin()** (lines 331-335):
```cpp
bool PinManager::changeBadgePin(const char* currentPin, const char* newPin) {
    if (!verifyBadgePin(currentPin)) return false;  // Calls saveToStorage() for retries
    return setBadgePin(newPin);  // Calls saveToStorage() for new PIN
}
```

**verifyBadgePin()** (lines 296-328):
```cpp
bool PinManager::verifyBadgePin(const char* pin) {
    // ... verify ...
    if (compareHash(badgeHash_, inputHash, BADGE_HASH_SIZE)) {
        resetBadgeRetries();  // Calls saveToStorage()
        lockoutActive_ = false;
        LOG_I(TAG, "Badge PIN verified");
        return true;
    }

    badgeRetries_--;
    saveToStorage();  // Save for wrong PIN
    // ...
}
```

**setBadgePin()** (lines 341-377):
```cpp
bool PinManager::setBadgePin(const char* newPin) {
    // ... validation ...

    computeBadgeHash(newPin, badgeHash_);
    badgeRetries_ = MAX_RETRIES;

    uint8_t defaultHash[BADGE_HASH_SIZE];
    computeBadgeHash(DEFAULT_BADGE_PIN, defaultHash);
    badgePinIsSet_ = !compareHash(badgeHash_, defaultHash);

    saveToStorage();  // Second save in changeBadgePin() workflow
    LOG_I(TAG, "Badge PIN changed");
    return true;
}
```

**resetBadgeRetries()** (lines 374-380):
```cpp
void PinManager::resetBadgeRetries() {
    if (badgeRetries_ < MAX_RETRIES) {
        badgeRetries_ = MAX_RETRIES;
        saveToStorage();  // Third save possible!
    }
}
```

## Recommended Fix

**Batch updates with single save:**
```cpp
bool PinManager::changeBadgePin(const char* currentPin, const char* newPin) {
    if (!verifyBadgePinNoSave(currentPin)) return false;
    
    // Batch all updates into one save
    computeBadgeHash(newPin, badgeHash_);
    badgeRetries_ = MAX_RETRIES;
    
    uint8_t defaultHash[BADGE_HASH_SIZE];
    computeBadgeHash(DEFAULT_BADGE_PIN, defaultHash);
    badgePinIsSet_ = !compareHash(badgeHash_, defaultHash);
    
    saveToStorage();  // Single save
    LOG_I(TAG, "Badge PIN changed");
    return true;
}

// New helper: verify without saving
bool PinManager::verifyBadgePinNoSave(const char* pin) {
    if (!pin) return false;
    if (!pinLoaded_) init();

    uint8_t inputHash[BADGE_HASH_SIZE];
    if (!computeBadgeHash(pin, inputHash)) return false;

    return compareHash(badgeHash_, inputHash, BADGE_HASH_SIZE);
}

// Reset with optional save
void PinManager::resetBadgeRetries(bool save) {
    if (badgeRetries_ < MAX_RETRIES) {
        badgeRetries_ = MAX_RETRIES;
        if (save) saveToStorage();
    }
}
```

## References

- NVS write performance characteristics
- Flash wear leveling and endurance
- Batch transaction patterns