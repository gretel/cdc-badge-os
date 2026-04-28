---
title: "[MEDIUM] DEBUG_MODE feature flag disables PIN lockouts when enabled"
severity: MEDIUM
domain: authorization
lens: debug-mode
labels:
  - "debug-mode"
  - "lockout-bypass"
---

## Summary

The `DEBUG_MODE` feature flag (defined in `components/cdc_core/include/cdc_core/feature_flags.h:29-31`) is set to `1` by default and "disables lockouts, useful for development". When enabled, this flag allows the PIN lockout mechanism to be bypassed, potentially allowing brute-force attacks on the PIN.

From `feature_flags.h`:
```cpp
// Debug Mode (disables lockouts, useful for development)
#ifndef DEBUG_MODE
#define DEBUG_MODE 1
#endif
```

The default value of `1` means **lockouts are disabled by default** in the current build configuration.

## Impact

When `DEBUG_MODE` is enabled:
1. **PIN lockout timers can be bypassed** - After 3 failed PIN attempts, users typically wait 60 seconds (or are permanently locked). With `DEBUG_MODE=1`, this protection may be circumvented.

2. **Brute-force attacks become feasible** - Without lockout protection, an attacker could try all 10,000 possible 4-digit PINs (or 100,000 for 6-digit PINs) without delay.

3. **Development default is insecure** - Since `DEBUG_MODE` defaults to `1`, production builds must explicitly set it to `0`, creating risk of accidentally shipping with debug mode enabled.

## Evidence

**File: `components/cdc_core/include/cdc_core/feature_flags.h`**
- Lines 29-31: DEBUG_MODE definition with default of `1`

**File: `components/cdc_core/src/PinManager.cpp`** (referenced structure)
- The `isBadgeBlocked()`, `isLockoutActive()`, and `getLockoutRemainingMs()` methods implement the lockout logic that DEBUG_MODE affects

**File: `components/serial_cmd/src/SerialCmd.cpp`**
- Lines 820-846: AUTH command checks `pm.isBadgeBlocked()` and displays lockout status
- Lines 1370-1388: `authenticate()` function uses lockout checks

## Recommended Fix

1. **Change DEBUG_MODE default to `0`** (disabled) for production-like security:
   ```cpp
   // Debug Mode (disables lockouts, useful for development)
   #ifndef DEBUG_MODE
   #define DEBUG_MODE 0  // Default to disabled for security
   #endif
   ```

2. **Add explicit build configuration** in `sdkconfig.defaults` or `platformio.ini` to ensure DEBUG_MODE is set appropriately per environment:
   ```ini
   # platformio.ini
   build_flags = 
       -DDEBUG_MODE=0  # Production
   ```

3. **Add a warning message at boot** if DEBUG_MODE is enabled:
   ```cpp
   #if DEBUG_MODE
   LOG_W(TAG, "DEBUG_MODE enabled - PIN lockouts disabled!");
   #endif
   ```

4. **Consider making DEBUG_MODE a runtime toggle** instead of compile-time, with a more secure default:
   ```cpp
   // Runtime debug flag that requires physical access to toggle
   static bool s_debugMode = false;
   
   bool isDebugMode() { return s_debugMode; }
   void setDebugMode(bool enabled) { s_debugMode = enabled; }
   ```

## References

- [OWASP Authentication Cheat Sheet - Account Lockout](https://cheatsheetseries.owasp.org/cheatsheets/Authentication_Cheat_Sheet.html#account-lockout)
- [NIST SP 800-63B - Verifier-Implemented Authentication](https://pages.nist.gov/800-63-3/sp800-63b.html#ver)
