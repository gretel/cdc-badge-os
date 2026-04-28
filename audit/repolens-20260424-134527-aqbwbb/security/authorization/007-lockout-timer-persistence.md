---
title: "[LOW] PIN lockout timer is RAM-only and resets on power cycle"
severity: LOW
domain: Authorization & Access Control
lens: authorization
labels:
  - audit:security/authorization
---

## Summary
The PIN lockout timer (`lockoutStartMs_`) is stored in RAM only and is not persisted to the secure element. When the device powers off or resets during a lockout period, the lockout is cleared immediately, allowing an attacker to bypass the 60-second wait time by cycling power.

**Location:** `components/cdc_core/include/cdc_core/PinManager.h:128-129`

## Impact
- **Lockout bypass:** Power cycling clears the lockout timer, allowing immediate retry
- **Brute-force acceleration:** An attacker can iterate through PIN attempts faster by power cycling between attempts
- **Weak time-based protection:** The 60-second lockout provides minimal security against automated attacks

## Evidence
Lockout timer storage at `components/cdc_core/include/cdc_core/PinManager.h:128-129`:
```cpp
// Lockout timer (RAM only, resets on power cycle)
uint32_t lockoutStartMs_ = 0;
bool lockoutActive_ = false;
```

The comment explicitly states "RAM only, resets on power cycle".

Lockout start at `components/cdc_core/src/PinManager.cpp:621-626`:
```cpp
void PinManager::startLockout() {
    lockoutStartMs_ = esp_timer_get_time() / 1000;  // Convert to ms
    lockoutActive_ = true;
    LOG_W(TAG, "Lockout started for %lu ms", LOCKOUT_DURATION_MS);
}
```

Lockout check at `components/cdc_core/src/PinManager.cpp:652-663`:
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
        const_cast<PinManager*>(this)->saveToStorage();
        LOG_I(TAG, "Lockout expired, retries reset");
        return false;
    }
    return true;
}
```

The lockout state is checked in `isBadgeBlocked()` at `components/cdc_core/src/PinManager.cpp:600-606`:
```cpp
bool PinManager::isBadgeBlocked() const {
    // Blocked if retries exhausted AND lockout still active
    if (badgeRetries_ == 0) {
        return isLockoutActive();
    }
    return false;
}
```

When power cycles, `lockoutActive_` is reset to `false` (default initialization), so `isLockoutActive()` returns `false` immediately.

## Recommended Fix
Persist the lockout state to the secure element's R-Memory. Update the storage format in `components/cdc_core/include/cdc_core/PinManager.h`:

1. **Add lockout timestamp to storage:**
```cpp
// In PinManager.h, add:
static constexpr uint32_t LOCKOUT_DURATION_MS = 60000;  // 60 seconds
uint32_t lockoutStartMs_;  // Persisted to R-Memory

// Update storage format to include lockout timestamp
static constexpr uint8_t STORAGE_SIZE = 110;  // Increased from 106
```

2. **Update save/load functions:**
```cpp
// In saveToStorage():
// ... existing code ...
// Add lockout timestamp
data[pos++] = (lockoutStartMs_ >> 24) & 0xFF;
data[pos++] = (lockoutStartMs_ >> 16) & 0xFF;
data[pos++] = (lockoutStartMs_ >> 8) & 0xFF;
data[pos++] = lockoutStartMs_ & 0xFF;

// In loadFromStorage():
// ... existing code ...
// Load lockout timestamp
lockoutStartMs_ = (data[pos] << 24) | (data[pos+1] << 16) | (data[pos+2] << 8) | data[pos+3];
pos += 4;
```

3. **Update lockout check to consider power cycle:**
```cpp
bool PinManager::isLockoutActive() const {
    if (badgeRetries_ == 0 && lockoutStartMs_ > 0) {
        uint32_t nowMs = esp_timer_get_time() / 1000;
        uint32_t elapsed = nowMs - lockoutStartMs_;
        
        if (elapsed < LOCKOUT_DURATION_MS) {
            return true;  // Still locked
        }
        // Lockout expired
        lockoutActive_ = false;
        badgeRetries_ = MAX_RETRIES;
        saveToStorage();
    }
    return false;
}
```

This ensures the lockout timer survives power cycles and provides consistent brute-force protection.

## References
- OWASP Authentication Cheat Sheet: https://cheatsheetseries.owasp.org/cheatsheets/Authentication_Cheat_Sheet.html
- NIST SP 800-63B Section 5.1.1.2 (Memory-Dependent Authentication): https://pages.nist.gov/800-63-3/sp800-63b.html#memdep
- FIDO2 security considerations: https://fidoalliance.org/specs/fido2/
