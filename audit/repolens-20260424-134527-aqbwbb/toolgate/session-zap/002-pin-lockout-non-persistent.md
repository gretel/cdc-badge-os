---
title: "[MEDIUM] PIN Lockout Timer Stored in RAM (Non-Persistent)"
severity: MEDIUM
domain: cdc-badge-os
lens: toolgate/session-zap
labels:
  - "audit:toolgate/session-zap"
---

## Summary

The PIN lockout timer (`lockoutStartMs_` and `lockoutActive_`) is stored only in RAM in the `PinManager` class, not persisted to the secure element. This allows an attacker to bypass the 60-second lockout by simply power-cycling the device.

**Location:** `components/cdc_core/include/cdc_core/PinManager.h:129-130`

```cpp
// Lockout timer (RAM only, resets on power cycle)
uint32_t lockoutStartMs_ = 0;
bool lockoutActive_ = false;
```

**Storage format (same file, line 24-28):**
```cpp
// Storage Format (106 bytes):
// [Magic 0xDD]           (1)  - Format identifier
// [Badge/FIDO2 Hash]     (16) - LEFT(SHA256(PIN), 16)
// [Badge Retries]        (1)  - Remaining attempts for Badge PIN
// ... (KDF params, salts, hashes)
// [PW1 Retries]          (1)  - Remaining attempts
// [PW3 Retries]          (1)  - Remaining attempts
```

Notice that the lockout state is NOT included in the storage format.

## Impact

**Security Impact:**
- **Lockout timer resets on power cycle**, allowing unlimited retry attempts
- An attacker can exhaust all 3 PIN retries, then power-cycle the device to reset
- The 60-second lockout becomes a minor inconvenience rather than a real deterrent

**Current behavior:**
1. Attacker tries 3 wrong PINs → lockout starts (60s wait)
2. Instead of waiting, attacker power-cycles device
3. On boot, `lockoutActive_` is `false` (default value)
4. `badgeRetries_` is reloaded from storage as `0`
5. `isLockoutActive()` returns `false` (lockout timer is 0)
6. Lockout expires immediately, retries reset to MAX_RETRIES

**From `PinManager.cpp:646-658`:**
```cpp
bool PinManager::isLockoutActive() const {
    if (!lockoutActive_) {  // Always true after power cycle!
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

## Evidence

1. **File:** `components/cdc_core/include/cdc_core/PinManager.h:128-130`
   - Shows lockout variables are instance members, not persisted
   - Comment explicitly says "RAM only, resets on power cycle"

2. **File:** `components/cdc_core/src/PinManager.cpp:96-150` (`loadFromStorage()`)
   - Loads badge hash, retries, KDF params, salts, hashes
   - **Does not load lockout state**

3. **File:** `components/cdc_core/src/PinManager.cpp:154-210` (`saveToStorage()`)
   - Saves badge hash, retries, KDF params, salts, hashes
   - **Does not save lockout state**

4. **File:** `components/cdc_core/src/PinManager.cpp:618-621` (`startLockout()`)
   ```cpp
   void PinManager::startLockout() {
       lockoutStartMs_ = esp_timer_get_time() / 1000;
       lockoutActive_ = true;
       LOG_W(TAG, "Lockout for %lu ms", LOCKOUT_DURATION_MS);
   }
   ```
   - Only sets RAM variables, no call to `saveToStorage()`

## Recommended Fix

**Add lockout state to persistent storage:**

1. **Update storage format** (add lockout timestamp):
```cpp
// Storage Format (110 bytes):
// [Magic 0xDD]           (1)  - Format identifier
// [Badge/FIDO2 Hash]     (16) - LEFT(SHA256(PIN), 16)
// [Badge Retries]        (1)  - Remaining attempts for Badge PIN
// [Lockout Timestamp]    (4)  - Unix timestamp when lockout started (0 = no lockout)
// ... (rest of format)
```

2. **Update `saveToStorage()`** to save lockout timestamp:
```cpp
bool PinManager::saveToStorage() {
    // ... existing code ...
    
    // Badge retries
    data[pos++] = badgeRetries_;
    
    // Lockout timestamp (4 bytes, big-endian)
    uint32_t lockoutTs = lockoutActive_ ? lockoutStartMs_ : 0;
    data[pos++] = (lockoutTs >> 24) & 0xFF;
    data[pos++] = (lockoutTs >> 16) & 0xFF;
    data[pos++] = (lockoutTs >> 8) & 0xFF;
    data[pos++] = lockoutTs & 0xFF;
    
    // ... rest of storage ...
}
```

3. **Update `loadFromStorage()`** to load lockout timestamp:
```cpp
bool PinManager::loadFromStorage() {
    // ... existing code ...
    
    // Badge retries
    badgeRetries_ = data[pos++];
    
    // Lockout timestamp
    uint32_t lockoutTs = (data[pos] << 24) | (data[pos+1] << 16) | 
                         (data[pos+2] << 8) | data[pos+3];
    pos += 4;
    
    if (lockoutTs > 0) {
        lockoutStartMs_ = lockoutTs;
        lockoutActive_ = true;
    }
    
    // ... rest of loading ...
}
```

4. **Update `startLockout()`** to trigger save:
```cpp
void PinManager::startLockout() {
    lockoutStartMs_ = esp_timer_get_time() / 1000;
    lockoutActive_ = true;
    saveToStorage();  // Persist lockout state
    LOG_W(TAG, "Lockout for %lu ms", LOCKOUT_DURATION_MS);
}
```

**Alternative (simpler but less precise):**
Store just a flag and reload time on boot (loses exact remaining time but maintains security):
```cpp
// Save
data[pos++] = lockoutActive_ ? 1 : 0;

// Load  
lockoutActive_ = (data[pos++] == 1);
if (lockoutActive_) {
    lockoutStartMs_ = esp_timer_get_time() / 1000;  // Fresh timestamp
}
```

## References

- [FIDO2 Security Guidelines - Retry Limits](https://fidoalliance.org/specs/fido-v2.0-rd-20180130/fido-client-to-protocol-v2.0-rd-20180130.html#retry-limits)
- NIST SP 800-63B: "The authenticator SHALL enforce a delay after each failed authentication"
- Common security practice: Security state should survive power cycles
