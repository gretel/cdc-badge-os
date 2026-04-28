---
title: "[LOW] Lockout state not persisted across reboots"
severity: LOW
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
The PIN lockout timer (`lockoutStartMs_` and `lockoutActive_`) is stored in RAM, not persisted to storage. After a reboot, the lockout state is lost and the user can immediately try PIN again, even if they were locked out before the reboot.

**File**: `components/cdc_core/src/PinManager.cpp:618-625`
```cpp
void PinManager::startLockout() {
    lockoutStartMs_ = esp_timer_get_time() / 1000;  // Convert to ms
    lockoutActive_ = true;
    LOG_W(TAG, "Lockout started for %lu ms", LOCKOUT_DURATION_MS);
}
```

**File**: `components/cdc_core/include/cdc_core/PinManager.h:128-130`
```cpp
// Lockout timer (RAM only, resets on power cycle)
uint32_t lockoutStartMs_ = 0;
bool lockoutActive_ = false;
```

**File**: `components/cdc_core/src/PinManager.cpp:319-328`
```cpp
badgeRetries_--;
saveToStorage();  // Persist retry count
LOG_W(TAG, "Wrong badge PIN, %d retries left", badgeRetries_);

// Start lockout timer when retries exhausted
if (badgeRetries_ == 0) {
    startLockout();  // Only stores in RAM
}
```

The retry count is persisted, but the lockout timer is not.

## Impact
- **Lockout Bypass**: An attacker can reboot the device to reset the lockout timer
- **Brute-force Facilitation**: After exhausting retries, an attacker can reboot and try again
- **Reduced Security**: The 60-second lockout provides minimal protection if it can be bypassed with a reboot

## Evidence
**File**: `components/cdc_core/include/cdc_core/PinManager.h:128-130`
```cpp
// Lockout timer (RAM only, resets on power cycle)
uint32_t lockoutStartMs_ = 0;
bool lockoutActive_ = false;
```

The comment explicitly states that lockout is RAM-only.

**File**: `components/cdc_core/src/PinManager.cpp:648-663`
```cpp
bool PinManager::isLockoutActive() const {
    if (!lockoutActive_) {
        return false;
    }

    uint32_t remaining = getLockoutRemainingMs();
    if (remaining == 0) {
        // Lockout expired - reset retries
        const_cast<PinManager*>(this)->lockoutActive_ = false;
        const_cast<PinManager*>(this)->badgeRetries_ = MAX_RETRIES;
        const_cast<PinManager*>(this)->saveToStorage();
        LOG_I(TAG, "Lockout expired, retries reset");
        return false;
    }
    return true;
}
```

The lockout state is checked but not persisted.

## Recommended Fix
Persist the lockout start time to storage:

```cpp
// Add to PinManager.h
uint32_t lockoutStartMs_ = 0;
bool lockoutActive_ = false;

// Add to loadFromStorage()
// After loading badgeRetries_
if (badgeRetries_ == 0) {
    // Check if lockout is still active based on stored time
    uint32_t nowMs = esp_timer_get_time() / 1000;
    uint32_t elapsed = nowMs - lockoutStartMs_;
    if (elapsed < LOCKOUT_DURATION_MS) {
        lockoutActive_ = true;
    } else {
        // Lockout expired, reset retries
        badgeRetries_ = MAX_RETRIES;
        lockoutActive_ = false;
    }
}

// Add to saveToStorage()
// After saving badgeRetries_
if (lockoutActive_) {
    // Write lockout start time (add to storage format)
    // pos += 1;  // badgeRetries_
    // data[pos++] = lockoutStartMs_ & 0xFF;
    // data[pos++] = (lockoutStartMs_ >> 8) & 0xFF;
    // data[pos++] = (lockoutStartMs_ >> 16) & 0xFF;
    // data[pos++] = (lockoutStartMs_ >> 24) & 0xFF;
}
```

Update storage format (add 4 bytes for lockout start time):

```cpp
// Storage Format (110 bytes, was 106):
// [Magic 0xDD]           (1)
// [Badge/FIDO2 Hash]     (16)
// [Badge Retries]        (1)
// [Lockout Start Time]   (4)  // NEW: Persisted lockout start time
// [KDF Algorithm]        (1)
// ... rest unchanged ...
```

Update `loadDefaults()` to initialize lockout:

```cpp
void PinManager::loadDefaults() {
    computeBadgeHash(DEFAULT_BADGE_PIN, badgeHash_);
    badgeRetries_ = MAX_RETRIES;
    lockoutActive_ = false;
    lockoutStartMs_ = 0;  // Initialize
    // ... rest of defaults ...
}
```

## References
- NIST SP 800-63B - Authentication and Lockout Policy
- OWASP Authentication Cheat Sheet - Lockout
- CWE-307: Improper Restriction of Excessive Authentication Attempts

</content>