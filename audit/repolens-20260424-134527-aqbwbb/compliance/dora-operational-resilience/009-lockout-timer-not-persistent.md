---
title: "[MEDIUM] PIN Lockout Timer Not Persistent - Bypassable with Power Cycle"
severity: MEDIUM
domain: Incident Detection
lens: dora-incident-detection
labels:
  - "audit:compliance/dora-operational-resilience"
---

## Summary
The PIN lockout timer in `PinManager` is stored in RAM only and resets on power cycle. An attacker can bypass the 60-second lockout by simply power-cycling the device.

Key files:
- `components/cdc_core/include/cdc_core/PinManager.h:129-130` - lockoutStartMs_ and lockoutActive_ are member variables
- `components/cdc_core/src/PinManager.cpp:618-621` - startLockout() stores to RAM only

From `PinManager.h:128-130`:
```cpp
// Lockout timer (RAM only, resets on power cycle)
uint32_t lockoutStartMs_ = 0;
bool lockoutActive_ = false;
```

## Impact
For financial entities:
- Brute-force attacks can be performed by cycling power after each failed attempt
- The 3-attempt limit becomes "3 attempts per power cycle" instead of "3 total attempts"
- Weakens incident detection and response capability
- Does not meet expected resilience for security-critical authentication

From `README.md` line 71:
> | **PIN Protection** | 4-8 digit PIN with 3 attempt lockout |

This implies a persistent lockout, but the implementation only provides temporary lockout.

## Evidence
From `components/cdc_core/include/cdc_core/PinManager.h:128-130`:
```cpp
// Lockout timer (RAM only, resets on power cycle)
uint32_t lockoutStartMs_ = 0;
bool lockoutActive_ = false;
```

From `components/cdc_core/src/PinManager.cpp:618-621`:
```cpp
void PinManager::startLockout() {
    lockoutStartMs_ = esp_timer_get_time() / 1000;  // RAM only
    lockoutActive_ = true;
    // No storage update!
}
```

From `PinManager.cpp:54-55` (loadFromStorage):
```cpp
// Loads from TROPIC01 R-Memory Slot 0
// Does NOT include lockout state
```

The lockout timer is never persisted to TROPIC01 R-Memory.

## Recommended Fix
Persist lockout state to TROPIC01 R-Memory:

1. **Update PIN storage format in `PinManager.h`**:
   Add lockout timestamp field:
   ```cpp
   static constexpr uint8_t STORAGE_SIZE = 116;  // +4 bytes for lockout timestamp
   
   uint32_t lockoutStartMs_ = 0;  // Persisted to storage
   ```

2. **Update `loadFromStorage()` and `saveToStorage()`**:
   - Write lockoutStartMs_ to storage on lockout
   - Read lockoutStartMs_ on init
   - Clear lockout when duration expires

3. **Update `startLockout()` to persist immediately**:
   ```cpp
   void PinManager::startLockout() {
       lockoutStartMs_ = esp_timer_get_time() / 1000;
       lockoutActive_ = true;
       saveToStorage();  // Persist immediately
   }
   ```

4. **Update `isLockoutActive()` to check persistence**:
   ```cpp
   bool PinManager::isLockoutActive() const {
       // Check if lockout expired
       if (lockoutStartMs_ > 0) {
           uint32_t now = esp_timer_get_time() / 1000;
           if (now - lockoutStartMs_ >= LOCKOUT_DURATION_MS) {
               // Expired - clear persisted state
               lockoutStartMs_ = 0;
               lockoutActive_ = false;
               saveToStorage();
               return false;
           }
       }
       return lockoutActive_;
   }
   ```

5. **Consider extending lockout duration** - DORA recommends progressive lockouts:
   - 1st lockout: 60 seconds
   - 2nd lockout: 5 minutes
   - 3rd lockout: 30 minutes (or device reset)

## References
- DORA Regulation (EU) 2022/2554, Article 6(1)(f) - Incident detection
- EBA Guidelines on internal governance (EBA-GL-2021-06), Section 8.3 - Authentication resilience
- NIST SP 800-63B (Digital Identity Guidelines) - Authentication and linkability
- FIDO2 CTAP2 spec - PIN protocol (for reference on retry handling)
