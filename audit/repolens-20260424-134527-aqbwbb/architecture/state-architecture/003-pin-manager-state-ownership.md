---
title: "[HIGH] PinManager singleton state has unclear ownership and no state transition validation"
severity: HIGH
domain: State Management Architecture
lens: state-architecture
labels:
  - "audit:architecture/state-architecture"
---

## Summary
The `PinManager` singleton (`components/cdc_core/include/cdc_core/PinManager.h`) manages critical security state (badge PIN, PW1, PW3 hashes, retries, lockout timer) but has no clear state machine validation. Multiple methods can modify state without checking preconditions (e.g., `changeBadgePin()` can be called without verifying current PIN first).

### Evidence:
- `PinManager.h:65-69`: Public API allows `changeBadgePin(currentPin, newPin)` without requiring PIN verification first
- `PinManager.h:75-76`: `startLockout()` can be called multiple times, resetting lockout timer each time
- `PinManager.h:80`: `resetBadgeRetries()` has no guard to check if retries are actually 0
- `PinManager.h:108-130`: Internal state (`badgeHash_`, `badgeRetries_`, `pw1Retries_`, `lockoutActive_`, `lockoutStartMs_`) is mutable but no state transition diagram exists

### State Transition Issues:
1. `resetBadgeRetries()` can be called anytime, not just after lockout
2. `startLockout()` doesn't check if already locked out
3. `changeBadgePin()` accepts `currentPin` parameter but doesn't enforce it's verified first
4. No `isStateValid()` method to check if PIN system is in consistent state

## Impact
1. **Security risk**: PIN change logic can be bypassed if caller doesn't verify PIN first
2. **Lockout logic bugs**: Multiple `startLockout()` calls reset timer, extending lockout indefinitely
3. **Debugging difficulty**: No way to query full PIN state for debugging (e.g., "is PIN set?", "is lockout active?")
4. **State corruption**: `badgePinIsSet_` flag can be out of sync with `badgeHash_` data

## Recommended Fix
1. Add state machine validation:
   ```cpp
   enum class PinState {
       NOT_SET,
       VERIFIED,
       LOCKED_OUT,
       BLOCKED
   };
   
   PinState getPinState() const;
   bool canChangePin() const;  // Returns true if PIN verified or retries > 0
   ```

2. Add precondition checks:
   ```cpp
   bool changeBadgePin(const char* currentPin, const char* newPin) {
       if (!canChangePin()) return false;
       if (!verifyBadgePin(currentPin)) return false;
       // ... rest of logic
   }
   ```

3. Add state query API:
   ```cpp
   typedef struct {
       PinState state;
       uint8_t badgeRetries;
       uint8_t pw1Retries;
       uint8_t pw3Retries;
       uint32_t lockoutRemainingMs;
       bool pinIsSet;
   } PinManagerState;
   
   PinManagerState getPinManagerState() const;
   ```

4. Add lockout timer guard:
   ```cpp
   void startLockout() {
       if (!isLockoutActive()) {  // Only start if not already locked
           lockoutStartMs_ = millis();
           lockoutActive_ = true;
       }
   }
   ```

## References
- `components/cdc_core/include/cdc_core/PinManager.h:65-100` - Public API
- `components/cdc_core/include/cdc_core/PinManager.h:108-130` - Internal state
- `components/cdc_core/src/PinManager.cpp` - Implementation (check for missing guards)
