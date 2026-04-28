---
title: "[MEDIUM] No access control review or recertification process for PINs"
severity: MEDIUM
domain: access-control
lens: nis2
labels:
  - "audit:compliance/nis2"
---

## Summary
The device lacks a periodic access control review mechanism. While PINs are the primary access control, NIS2 requires "access control and access granting" with regular reviews (Art. 20). There is no process to review who has access (PIN holders), no recertification of PINs, and no audit trail of access events.

## Impact
**NIS2 Art. 20 Access Control Gap**:
- No periodic review of who knows the PINs
- No process for changing PINs when personnel changes
- No audit log of who accessed what and when
- No recertification of access rights
- Default PINs may remain unchanged

## Evidence

1. **No access audit log**:
   - File: `components/cdc_core/src/PinManager.cpp`
   - PIN verification is logged but not stored persistently
   - No `AccessLog` class or NVS storage for access events
   - Lockout events logged but not tracked for review

2. **No PIN expiration or rotation**:
   - File: `components/cdc_core/include/cdc_core/PinManager.h:61-70`
   ```cpp
   // Badge/FIDO2
   bool verifyBadgePin(const char* pin);
   bool changeBadgePin(const char* currentPin, const char* newPin);
   bool setBadgePin(const char* newPin);
   ```
   - No `setPinExpiry()` or `getPinAge()` methods
   - PINs never expire by default

3. **No access review command**:
   - File: `docs/SERIAL_COMMANDS.md`
   - Available PIN commands: `PIN_CHANGE`, `PIN_STATUS`
   - Missing: `ACCESS_LOG`, `ACCESS_REVIEW`, `PIN_ROTATE`

4. **No multi-user access tracking**:
   - Multiple PINs exist (Badge, PW1, PW3) but no owner tracking
   - File: `components/cdc_core/include/cdc_core/PinManager.h:20-28`
   - PIN storage doesn't record who set each PIN or when

5. **No session tracking**:
   - After PIN verification, no session timeout
   - File: `components/cdc_os_ui/src/LockScreen.cpp`
   - Device locks after idle time but no access duration limit

6. **No access recertification workflow**:
   - No command to force PIN re-verification after N days
   - No "PIN expiry warning" system
   - No process for bulk PIN change

## Recommended Fix

1. **Implement Access Audit Log**:
   ```cpp
   // components/cdc_core/include/cdc_core/AccessLog.h
   struct AccessEntry {
       uint32_t timestamp;
       uint8_t pinType;        // BADGE, PW1, PW3
       uint8_t result;         // SUCCESS, FAILURE
       uint16_t module;        // Which module was accessed
   };
   ```
   - Store last 100 entries in NVS
   - Command: `ACCESS_LOG` to view entries
   - Command: `ACCESS_LOG CLEAR` to reset

2. **Add PIN Age Tracking**:
   - Store PIN change timestamp in R-Memory slot 0
   - Add `getPinAge()` method to PinManager
   - Display PIN age in `PIN_STATUS` output

3. **Implement PIN Rotation Policy**:
   ```cpp
   // Configuration
   #define PIN_ROTATION_DAYS_DEFAULT 90  // Rotate every 90 days
   ```
   - Warn when PIN is near expiration
   - Command: `PIN_FORCE_ROTATE <type>`

4. **Add Access Review Command**:
   ```bash
   # Show access summary
   ACCESS_REVIEW
   
   # Show last 50 access events
   ACCESS_LOG 50
   
   # Export access log (for external review)
   ACCESS_LOG EXPORT
   ```

5. **Implement Session Timeout**:
   - Add session duration limit (e.g., 10 minutes)
   - Require re-authentication after timeout
   - Configurable via `SET_SESSION_TIMEOUT <seconds>`

6. **Document Access Review Process**:
   - Create `docs/ACCESS_REVIEW.md`
   - Define who should review access (device owner)
   - Define review frequency (e.g., monthly)
   - Define what to check (PINs still needed, rotation status)

## References
- [NIS2 Directive Art. 20 - Access control](https://eur-lex.europa.eu/eli/dir/2022/2555/oj)
- [NIST SP 800-53 Rev. 5 AC-2 Account Management](https://csrc.nist.gov/publications/detail/sp/800-53/rev-5/final)
- [NIST SP 800-53 Rev. 5 AU-2 Audit Events](https://csrc.nist.gov/publications/detail/sp/800-53/rev-5/final)

</content>