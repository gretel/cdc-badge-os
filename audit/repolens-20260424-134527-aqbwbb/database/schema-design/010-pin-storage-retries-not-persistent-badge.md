---
title: "[MEDIUM] Badge PIN lockout timer is RAM-only, not persistent across resets"
severity: MEDIUM
domain: database/schema-design
lens: state-persistence
labels:
  - "storage"
  - "pin-manager"
  - "security"
---

## Summary
The badge PIN lockout timer (`PinManager.h:124-125`) is stored only in RAM and resets on power cycle. This means an attacker could brute-force the badge PIN by power-cycling between attempts. The retries counter IS persisted, but the time-based lockout is not.

**Evidence:**
- `PinManager.h:124-125`:
  ```cpp
  // Lockout timer (RAM only, resets on power cycle)
  uint32_t lockoutStartMs_ = 0;
  bool lockoutActive_ = false;
  ```
- `PinManager.h:68`: `void startLockout();` - starts timer but doesn't persist
- `PinManager.h:70`: `uint32_t getLockoutRemainingMs() const;` - calculated from RAM state

## Impact
1. **Security bypass**: Attacker can brute-force by power-cycling between attempts
2. **Inconsistent security**: PW1/PW3 have persistent retries but no time lockout
3. **Design inconsistency**: Some state is persisted, some is not, with unclear rationale

## Evidence
From `PinManager.h:124-130`:
```cpp
// Lockout timer (RAM only, resets on power cycle)
uint32_t lockoutStartMs_ = 0;
bool lockoutActive_ = false;

bool loadFromStorage();
bool saveToStorage();
// ...
```

From `PinManager.cpp:617-622`:
```cpp
void PinManager::startLockout() {
    lockoutStartMs_ = esp_timer_get_time() / 1000;  // RAM only!
    lockoutActive_ = true;
    LOG_W(TAG, "Lockout started for %lu ms", LOCKOUT_DURATION_MS);
}
```

From `PinManager.cpp:147`:
```cpp
// loadFromStorage() does NOT load lockout state
```

## Recommended Fix
1. **Persist lockout state** to R-Memory:
   ```cpp
   struct PinStorage {
       uint8_t magic;
       uint8_t badgeHash[16];
       uint8_t badgeRetries;
       // ... existing fields ...
       uint8_t lockoutActive;      // Persisted flag
       uint32_t lockoutStartMs;    // Persisted timestamp
   };
   ```

2. **Update load/save** to include lockout state:
   ```cpp
   bool loadFromStorage() {
       // ... existing code ...
       lockoutActive_ = data[pos++];
       lockoutStartMs_ = (data[pos] << 24) | ...;
   }
   ```

3. **On boot**, check if lockout should still be active based on elapsed time:
   ```cpp
   uint32_t elapsed = esp_timer_get_time() / 1000 - lockoutStartMs_;
   if (elapsed >= LOCKOUT_DURATION_MS) {
       lockoutActive_ = false;
       badgeRetries_ = MAX_RETRIES;
       saveToStorage();
   }
   ```

## References
- Secure element PIN lockout patterns
- Time-based security state persistence
