---
title: "[MEDIUM] No key rotation policy for PINs (default PIN can remain unchanged indefinitely)"
severity: MEDIUM
domain: access-control
lens: nis2
labels:
  - "audit:compliance/nis2"
---

## Summary
The device has a default PIN (`123456`) but no mechanism to enforce periodic PIN rotation. NIS2 Article 20 requires "access control and access granting" with regular reviews. Without a rotation policy, the default PIN or an old PIN can remain unchanged indefinitely, increasing the risk of compromise.

## Impact
**NIS2 Art. 20 Access Control Gap**:
- Default PIN `123456` is documented in README.md
- No enforcement to change default PIN after initial setup
- No periodic rotation requirement (e.g., every 90 days)
- No PIN age tracking or expiration warning
- PINs can be shared without detection

## Evidence

1. **Default PIN documented in README**:
   - File: `README.md:190-191`
   ```
   1. **Change the default PIN** (Settings -> Change PIN)
      - Default PIN: `123456`
   ```
   - Suggests changing but doesn't enforce it

2. **PinManager has no expiration tracking**:
   - File: `components/cdc_core/include/cdc_core/PinManager.h:61-70`
   ```cpp
   bool verifyBadgePin(const char* pin);
   bool changeBadgePin(const char* currentPin, const char* newPin);
   bool setBadgePin(const char* newPin);
   ```
   - No `getPinAge()`, `setPinExpiry()`, or `forceRotation()` methods

3. **No PIN age storage in R-Memory**:
   - File: `components/cdc_core/src/PinManager.cpp`
   - PIN verification logic exists but no timestamp tracking
   - R-Memory slot 0 stores PINs but not metadata (change date, age)

4. **No PIN rotation command**:
   - File: `docs/SERIAL_COMMANDS.md`
   - Available: `PIN_CHANGE`, `PIN_STATUS`
   - Missing: `PIN_FORCE_ROTATE`, `PIN_EXPIRE`, `PIN_ROTATE_POLICY`

5. **DEBUG_MODE disables security features**:
   - File: `components/cdc_core/include/cdc_core/feature_flags.h:27-29`
   ```cpp
   #ifndef DEBUG_MODE
   #define DEBUG_MODE 1
   #endif
   ```
   - Default DEBUG_MODE=1 means lockouts are disabled
   - No reminder to switch to production mode

## Recommended Fix

1. **Add PIN metadata structure**:
   ```cpp
   // components/cdc_core/include/cdc_core/PinManager.h
   struct PinInfo {
       uint8_t pin[8];           // Hashed PIN
       uint32_t changedAt;       // Unix timestamp
       uint32_t expiresAt;       // 0 = no expiration
       uint8_t attempts;         // Failed attempts counter
       uint8_t lockoutUntil;     // Lockout timestamp
   };
   ```

2. **Implement PIN age tracking**:
   ```cpp
   // Store timestamp when PIN is changed
   bool PinManager::setBadgePin(const char* newPin) {
       // ... validate and hash PIN
       pinInfo.changedAt = esp_timer_get_time() / 1000;
       pinInfo.expiresAt = pinInfo.changedAt + PIN_ROTATION_DEFAULT_DAYS * 86400;
       savePinInfo();
   }
   ```

3. **Add PIN rotation policy**:
   ```cpp
   // Configuration
   #define PIN_ROTATION_DEFAULT_DAYS 90
   #define PIN_ROTATION_WARNING_DAYS 7
   
   // Check expiration
   bool PinManager::isPinExpired() {
       uint32_t now = esp_timer_get_time() / 1000;
       return now > pinInfo.expiresAt;
   }
   
   uint32_t PinManager::getPinDaysRemaining() {
       uint32_t now = esp_timer_get_time() / 1000;
       if (pinInfo.expiresAt == 0) return UINT32_MAX;
       return (pinInfo.expiresAt - now) / 86400;
   }
   ```

4. **Add serial commands**:
   ```bash
   # Show PIN age and expiration
   PIN_STATUS
   
   # Force PIN rotation
   PIN_FORCE_ROTATE <type>
   
   # Set rotation policy (days, 0 = no expiry)
   PIN_ROTATION_POLICY <type> <days>
   ```

5. **Add UI warning for expiring PINs**:
   - Show warning when PIN is within 7 days of expiration
   - Force PIN change on next unlock if expired
   - Display PIN age in Settings menu

6. **Enforce default PIN change**:
   - On first boot, detect default PIN
   - Require change before enabling FIDO2
   - Show "Default PIN active" warning in status

## References
- [NIS2 Directive Art. 20 - Access control](https://eur-lex.europa.eu/eli/dir/2022/2555/oj)
- [NIST SP 800-53 Rev. 5 IA-5 Authenticator Management](https://csrc.nist.gov/publications/detail/sp/800-53/rev-5/final)
- [PIN Manager Implementation](components/cdc_core/src/PinManager.cpp)

</content>