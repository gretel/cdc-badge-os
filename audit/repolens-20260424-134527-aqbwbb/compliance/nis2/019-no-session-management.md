---
title: "[LOW] No session management (no session timeout, no session tracking)"
severity: LOW
domain: access-control
lens: nis2
labels:
  - "audit:compliance/nis2"
---

## Summary
After PIN verification, the device maintains an unlocked state indefinitely (until lock screen or deep sleep). NIS2 Article 20 requires "access control and access granting" with session management. No session timeout means an unlocked device remains accessible to anyone with physical access.

## Impact
**NIS2 Art. 20 Session Management Gap**:
- No session duration limit after unlock
- Device stays unlocked until manually locked or idle timeout
- No way to configure session timeout
- No session tracking (who unlocked, when)

## Evidence

1. **LockScreen uses idle timeout only**:
   - File: `components/cdc_os_ui/src/LockScreen.cpp`
   - Locks after idle time (e.g., 5 minutes)
   - No maximum session duration

2. **PinManager has no session tracking**:
   - File: `components/cdc_core/include/cdc_core/PinManager.h`
   ```cpp
   bool verifyBadgePin(const char* pin);
   bool isPinVerified();
   ```
   - No `getSessionStartTime()`, `getSessionDuration()`

3. **No session timeout configuration**:
   - File: `docs/SERIAL_COMMANDS.md`
   - No command to set session timeout
   - No `SESSION_TIMEOUT` setting

4. **No session limit in feature flags**:
   - File: `components/cdc_core/include/cdc_core/feature_flags.h`
   - No `SESSION_TIMEOUT_DEFAULT` or similar

5. **Lock screen idle time hardcoded**:
   - Likely in `LockScreen.cpp` or `Settings.cpp`
   - No configurable session duration limit

## Recommended Fix

1. **Add session tracking to PinManager**:
   ```cpp
   // components/cdc_core/include/cdc_core/PinManager.h
   class PinManager {
       uint32_t sessionStart_;      // Unix timestamp
       uint32_t sessionTimeout_;    // Max session duration (seconds)
       
   public:
       bool verifyBadgePin(const char* pin);
       uint32_t getSessionDuration() const;
       bool isSessionExpired() const;
       void resetSession();
   };
   ```

2. **Add session timeout configuration**:
   ```cpp
   // Configuration
   #define SESSION_TIMEOUT_DEFAULT 600  // 10 minutes
   
   // Settings
   SET_SESSION_TIMEOUT <seconds>  // 0 = no limit
   GET_SESSION_TIMEOUT
   ```

3. **Auto-lock on session expiry**:
   ```cpp
   // In main loop or timer
   void checkSessionTimeout() {
       if (pinManager.isSessionExpired()) {
           LOG_W("SESSION", "Session expired, locking device");
           lockScreen.lock();
       }
   }
   ```

4. **Add session status command**:
   ```bash
   SESSION_STATUS
   # Output:
   # Session active since: 2024-01-15 10:30:00
   # Duration: 5m 23s
   # Remaining: 4m 37s
   ```

5. **Add UI indicator**:
   - Show session time on status screen
   - Warning at 80% of timeout
   - Countdown display when near expiry

## References
- [NIS2 Directive Art. 20 - Access control](https://eur-lex.europa.eu/eli/dir/2022/2555/oj)
- [NIST SP 800-63B - Session Management](https://csrc.nist.gov/publications/detail/sp/800-63b/final)
- [LockScreen Implementation](components/cdc_os_ui/src/LockScreen.cpp)

</content>