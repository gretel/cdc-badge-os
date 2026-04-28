---
title: "[HIGH] Missing audit log for PIN change operations"
severity: HIGH
domain: security-authentication
lens: audit-trail
labels:
  - audit:observability/audit-trail
---

## Summary
PIN change operations (Badge PIN, OpenPGP PW1, PW3) are performed in `components/cdc_core/src/PinManager.cpp` but produce no audit trail. When a PIN is changed via `setBadgePin()`, `setPW1()`, or `setPW3()`, there is no structured log entry recording:
- What type of PIN was changed
- When the change occurred (timestamp)
- Whether it was a success or failure
- The previous and new state (e.g., "PIN changed from default to custom")

**Evidence:**
- `PinManager.cpp:368` - `setBadgePin()` logs only `LOG_I(TAG, "Badge PIN changed")` without context
- `PinManager.cpp:467` - `setPW1()` logs only `LOG_I(TAG, "PW1 changed")`
- `PinManager.cpp:561` - `setPW3()` logs only `LOG_I(TAG, "PW3 changed")`
- `PinChangeView.cpp:214` - PIN change wizard calls `setBadgePin()` but no audit event is generated

## Impact
- **Security investigations**: Cannot determine when PINs were last changed or identify suspicious PIN changes
- **Compliance**: FIDO2 and OpenPGP specifications recommend audit trails for security-critical operations
- **Accountability**: No way to distinguish between initial setup PIN changes vs. user-initiated changes
- **Forensics**: If a device is compromised, investigators cannot trace PIN modification history

## Evidence
```cpp
// PinManager.cpp:368
bool PinManager::setBadgePin(const char* newPin) {
    // ... validation ...
    saveToStorage();
    LOG_I(TAG, "Badge PIN changed");  // Basic log only, no audit structure
    return true;
}

// PinChangeView.cpp:214
bool changed = onChange_
    ? onChange_(currentPin_, newPin_)
    : core::PinManager::instance().setBadgePin(newPin_);
if (changed) {
    pinChanged_ = true;
    showMessage(tr(StringId::PIN_CHANGED));
    LOG_I(TAG, "PIN changed successfully");  // No audit event
}
```

## Recommended Fix
1. Create an audit log structure in `components/cdc_core/include/cdc_core/AuditLog.h`:
   ```cpp
   struct AuditEntry {
       uint32_t timestamp_ms;
       const char* event_type;  // e.g., "PIN_CHANGED", "PIN_VERIFY_FAIL"
       const char* target;      // e.g., "BADGE_PIN", "PW1", "PW3"
       const char* result;      // "SUCCESS", "FAILURE"
       const char* details;     // Optional context
   };
   ```

2. Add audit functions to `PinManager`:
   ```cpp
   void auditPinChange(const char* pinType, bool success);
   void auditPinVerify(const char* pinType, bool success);
   ```

3. Call audit functions at key points:
   - After `setBadgePin()`, `setPW1()`, `setPW3()` succeed/fail
   - After `verifyBadgePin()`, `verifyPW1()`, `verifyPW3()` succeed/fail
   - When lockout starts/ends

4. Store audit entries in NVS (append-only ring buffer, e.g., last 50 entries)

## References
- FIDO2 Server Implementation Best Practices (Section 6.3 Audit Logging)
- NIST SP 800-53 Rev 5: AU-2 Audit Events
- OpenPGP Card Specification v3.3 (Section 5.2 PIN management)
