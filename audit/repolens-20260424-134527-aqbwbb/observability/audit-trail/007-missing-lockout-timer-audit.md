---
title: "[MEDIUM] Missing audit trail for PIN lockout timer events"
severity: MEDIUM
domain: security-authentication
lens: audit-trail
labels:
  - audit:observability/audit-trail
---

## Summary
PIN lockout timer events in `components/cdc_core/src/PinManager.cpp` are not audited. When a PIN is locked out (retries exhausted), when the lockout starts, and when it expires, no audit records are generated.

**Missing audit events:**
1. Lockout start (when retries are exhausted)
2. Lockout status check (is lockout active?)
3. Lockout expiration (when timer completes)
4. Lockout duration (how long was user locked out?)

## Impact
- **Security monitoring**: Cannot detect brute-force attacks from repeated lockouts
- **Troubleshooting**: Users cannot determine why PIN is blocked or when it will unlock
- **Forensics**: No record of how many times lockout was triggered
- **Accountability**: Cannot correlate lockout events with specific authentication attempts

## Evidence
```cpp
// PinManager.cpp:324 - Lockout starts silently
bool PinManager::verifyBadgePin(const char* pin) {
    // ...
    if (compareHash(badgeHash_, inputHash, BADGE_HASH_SIZE)) {
        resetBadgeRetries();
        lockoutActive_ = false;
        LOG_I(TAG, "Badge PIN verified");
        return true;
    }

    badgeRetries_--;
    saveToStorage();
    LOG_W(TAG, "Wrong badge PIN, %d retries left", badgeRetries_);

    // Start lockout timer when retries exhausted
    if (badgeRetries_ == 0) {
        startLockout();  // Lockout starts, but no audit event!
    }
    return false;
}

// PinManager.cpp:617 - Lockout timer starts
void PinManager::startLockout() {
    lockoutStartMs_ = esp_timer_get_time() / 1000;
    lockoutActive_ = true;
    LOG_W(TAG, "Lockout started for %lu ms", LOCKOUT_DURATION_MS);
    // Basic log only, no structured audit!
}

// PinManager.cpp:652 - Lockout expires silently
bool PinManager::isLockoutActive() const {
    // ...
    if (remaining == 0) {
        // Lockout expired - reset retries
        const_cast<PinManager*>(this)->lockoutActive_ = false;
        const_cast<PinManager*>(this)->badgeRetries_ = MAX_RETRIES;
        const_cast<PinManager*>(this)->saveToStorage();
        LOG_I(TAG, "Lockout expired, retries reset");
        // No audit event for expiration!
        return false;
    }
    return true;
}
```

## Recommended Fix
1. Add audit functions in `components/cdc_core/include/cdc_core/PinAudit.h`:
   ```cpp
   void pin_audit_lockout_started(const char* pin_type);
   void pin_audit_lockout_expired(const char* pin_type, uint32_t duration_ms);
   void pin_audit_lockout_check(const char* pin_type, bool is_active);
   ```

2. Add calls to PinManager:
   - In `startLockout()`: `pin_audit_lockout_started("BADGE_PIN")`
   - In `isLockoutActive()`: when lockout expires, `pin_audit_lockout_expired("BADGE_PIN", duration)`
   - In `verifyBadgePin()`: when checking if blocked, `pin_audit_lockout_check()`

3. Include in audit:
   - PIN type (BADGE, PW1, PW3)
   - Event type (LOCKOUT_START/LOCKOUT_EXPIRE/LOCKOUT_CHECK)
   - Duration (for expiration)
   - Timestamp

## References
- NIST SP 800-63B: Section 5.1.1.2 (Memorized Secret Verifiers)
- FIDO2 Server Implementation: Section 6.3 (Audit Logging)
- OWASP Authentication Cheat Sheet (Section 4.5)
