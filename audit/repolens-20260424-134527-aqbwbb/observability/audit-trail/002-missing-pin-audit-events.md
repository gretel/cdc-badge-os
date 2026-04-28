---
title: "[HIGH] Missing audit events for PIN verification and lockout"
severity: HIGH
domain: observability
lens: audit-trail
labels:
  - "audit:observability/audit-trail"
---

## Summary

PIN verification events (success/failure), PIN changes, and lockout timer events are logged via `LOG_W`/`LOG_I` but lack structured audit records. This makes it impossible to track brute-force attempts, lockout patterns, and PIN lifecycle changes in a queryable format.

**Files affected:**
- `components/cdc_core/src/PinManager.cpp:301-330` - `verifyBadgePin()`
- `components/cdc_core/src/PinManager.cpp:404-430` - `verifyPW1()`
- `components/cdc_core/src/PinManager.cpp:501-528` - `verifyPW3()`
- `components/cdc_core/src/PinManager.cpp:616-623` - `startLockout()`

## Impact

1. **Security Analysis**: Cannot detect brute-force PIN attacks from audit logs.
2. **Lockout Tracking**: No record of when lockouts started/ended for forensic analysis.
3. **PIN Lifecycle**: No audit trail for PIN changes (who/when/which PIN type).
4. **Compliance**: FIDO2 and Common Criteria require audit of authentication events.

## Evidence

### PIN verification lacks audit structure

In `PinManager.cpp:301-330`:
```cpp
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
    saveToStorage();  // Persist retry count
    LOG_W(TAG, "Wrong badge PIN, %d retries left", badgeRetries_);

    // Start lockout timer when retries exhausted
    if (badgeRetries_ == 0) {
        startLockout();
    }
    return false;
}
```

**Missing audit data:**
- No unique event ID for correlating related events
- No source context (USB, BLE, keypad?)
- No timestamp in structured format
- No actor/session identification
- Log output is volatile (not persisted)

### Lockout events not audited

In `PinManager.cpp:616-623`:
```cpp
void PinManager::startLockout() {
    lockoutStartMs_ = esp_timer_get_time() / 1000;  // Convert to ms
    lockoutActive_ = true;
    LOG_W(TAG, "Lockout started for %lu ms", LOCKOUT_DURATION_MS);
}
```

Lockout start is logged but:
- No record of lockout duration
- No record of lockout expiry
- No correlation to failed PIN attempts that triggered it

### PIN change lacks before/after tracking

In `PinManager.cpp:332-336`:
```cpp
bool PinManager::changeBadgePin(const char* currentPin, const char* newPin) {
    if (!verifyBadgePin(currentPin)) return false;
    return setBadgePin(newPin);
}
```

No audit record of:
- Which PIN type changed (Badge/PW1/PW3)
- Timestamp of change
- Whether it was user-initiated or default reset

## Recommended Fix

### Step 1: Add audit calls to PIN verification (20 min)

Update `verifyBadgePin()`:
```cpp
bool PinManager::verifyBadgePin(const char* pin) {
    if (!pin) {
        core::audit_record(core::AuditEventType::PIN_VERIFY_FAILURE,
                          0, 0, 2, getPinContext(), 0);
        return false;
    }

    // ... existing code ...

    if (compareHash(badgeHash_, inputHash, BADGE_HASH_SIZE)) {
        resetBadgeRetries();
        lockoutActive_ = false;
        core::audit_record(core::AuditEventType::PIN_VERIFY_SUCCESS,
                          0, 0, 0, getPinContext(), badgeRetries_);
        LOG_I(TAG, "Badge PIN verified");
        return true;
    }

    badgeRetries_--;
    saveToStorage();
    core::audit_record(core::AuditEventType::PIN_VERIFY_FAILURE,
                      0, 0, 1, getPinContext(), badgeRetries_);
    LOG_W(TAG, "Wrong badge PIN, %d retries left", badgeRetries_);

    if (badgeRetries_ == 0) {
        startLockout();
        core::audit_record(core::AuditEventType::PIN_VERIFY_FAILURE,
                          0, 0, 1, getPinContext(), 0x100);  // 0x100 = lockout flag
    }
    return false;
}
```

### Step 2: Add lockout audit (10 min)

Update `startLockout()`:
```cpp
void PinManager::startLockout() {
    lockoutStartMs_ = esp_timer_get_time() / 1000;
    lockoutActive_ = true;
    core::audit_record(core::AuditEventType::SYSTEM_LOCK,
                      0, 0, 0, getPinContext(), LOCKOUT_DURATION_MS);
    LOG_W(TAG, "Lockout started for %lu ms", LOCKOUT_DURATION_MS);
}
```

### Step 3: Add PIN change audit (10 min)

Update `setBadgePin()`:
```cpp
bool PinManager::setBadgePin(const char* newPin) {
    // ... existing validation ...

    computeBadgeHash(newPin, badgeHash_);
    badgeRetries_ = MAX_RETRIES;
    saveToStorage();

    core::audit_record(core::AuditEventType::PIN_CHANGE,
                      0, 0, 0, getPinContext(), 0);  // 0 = Badge PIN
    LOG_I(TAG, "Badge PIN changed");
    return true;
}
```

## References

- FIDO2 CTAP2 Spec: Authentication events must be trackable
- NIST SP 800-63B: Authentication event logging requirements
- Common Criteria Part 2: FAU_GEN.1 (Audit data generation)
