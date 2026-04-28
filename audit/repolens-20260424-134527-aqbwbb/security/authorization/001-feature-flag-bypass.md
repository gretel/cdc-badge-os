---
title: "[MEDIUM] FEATURE_SECURE_SERIAL can be disabled via build flag allowing unauthenticated serial access"
severity: MEDIUM
domain: authorization
lens: auth-serial
labels:
  - "secure-serial"
---

## Summary

The `FEATURE_SECURE_SERIAL` feature flag (defined in `components/cdc_core/include/cdc_core/feature_flags.h:14-22`) controls whether the serial command interface requires PIN authentication. When `FEATURE_SECURE_SERIAL` is set to `0` (or not defined, defaulting to `0`), **all serial commands become accessible without any authentication**.

The feature flag is read from Kconfig `CONFIG_SECURE_SERIAL` but defaults to `0` if not explicitly set:
```cpp
#ifdef CONFIG_SECURE_SERIAL
#define FEATURE_SECURE_SERIAL 1
#else
#ifndef FEATURE_SECURE_SERIAL
#define FEATURE_SECURE_SERIAL 0
#endif
#endif
```

When `FEATURE_SECURE_SERIAL` is disabled, the authorization check in `components/serial_cmd/src/CommandRegistry.cpp:114-138` is completely bypassed:
```cpp
#if FEATURE_SECURE_SERIAL
// Check if PIN is blocked (lockout or retries exhausted)
// When blocked, only PING is allowed
auto& pm = cdc::core::PinManager::instance();
if (pm.isBadgeBlocked()) {
    if (pm.isLockoutActive()) {
        // ...
    }
    // ...
} else {
    // When secure serial is enabled, block ALL commands except PING and AUTH
    // when not authenticated
    bool isAllowedWithoutAuth = (strcasecmp(cmdBuf, "PING") == 0 ||
                                  strcasecmp(cmdBuf, "AUTH") == 0);
    if (!isAllowedWithoutAuth && authCheck_ && !authCheck_()) {
        Console::printf("ERROR: Not authenticated. Use AUTH <pin> to login.\r\n");
        return true;  // Command blocked
    }
}
#endif  // FEATURE_SECURE_SERIAL
```

## Impact

When `FEATURE_SECURE_SERIAL` is disabled:
1. **All serial commands execute without authentication** - including critical commands like:
   - `TR01_WIPE` - Factory reset of secure element
   - `NVS_CLEAR` - Erase all NVS data
   - `TR01_ECC_DEL` / `TR01_RMEM_DEL` - Delete secure element slots
   - `PASSWORD_GET` - Read password entries
   - `TOTP_GET` - Generate TOTP codes
   - `GPG_GENERATE` / `GPG_RESET` - Manage GPG keys

2. **No lockout protection** - PIN retry counters and lockout timers are only checked when `FEATURE_SECURE_SERIAL` is enabled

3. **Build-time bypass** - An attacker with build access can simply compile with `-DFEATURE_SECURE_SERIAL=0` to get full access

## Evidence

**File: `components/cdc_core/include/cdc_core/feature_flags.h`**
- Lines 14-22: Feature flag definition with default of `0`

**File: `components/serial_cmd/src/CommandRegistry.cpp`**
- Lines 114-138: Authorization check wrapped in `#if FEATURE_SECURE_SERIAL`
- Lines 144-148: Per-command auth check also conditional

**File: `components/serial_cmd/src/SerialCmd.cpp`**
- Lines 1200-1204: Auth provider only registered when `FEATURE_SECURE_SERIAL` is enabled
- Lines 1448-1452: AUTH/LOGOUT commands only registered when `FEATURE_SECURE_SERIAL` is enabled

## Recommended Fix

1. **Make `FEATURE_SECURE_SERIAL` default to `1`** instead of `0` in `feature_flags.h`:
   ```cpp
   #ifndef FEATURE_SECURE_SERIAL
   #define FEATURE_SECURE_SERIAL 1  // Default to enabled for security
   #endif
   ```

2. **Add a compile-time assertion** to ensure the feature is enabled:
   ```cpp
   #if FEATURE_SECURE_SERIAL == 0
   #warning "FEATURE_SECURE_SERIAL is disabled - serial commands will be accessible without PIN!"
   #endif
   ```

3. **Consider moving the authorization check outside the preprocessor conditional** and make it runtime-configurable instead of compile-time:
   ```cpp
   // Always check authorization, but allow runtime disable for debugging
   #if FEATURE_SECURE_SERIAL
   static bool s_authRequired = true;  // Can be toggled via debug command
   #else
   static bool s_authRequired = false;
   #endif
   
   if (s_authRequired && authCheck_ && !authCheck_()) {
       Console::printf("ERROR: Not authenticated. Use AUTH <pin> to login.\r\n");
       return true;
   }
   ```

## References

- [OWASP Authentication Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Authentication_Cheat_Sheet.html)
- [FIDO2 Specification - Client PIN Protocol](https://fidoalliance.org/specs/fido-v2.1-rd-20210309/fido-client-to-authenticator-protocol-v2.1-rd-20210309.html#client-pin)
- [OpenPGP Card Specification 3.4.1 - PIN Verification](https://g10code.com/docs/openpgp-card-3.4.pdf)
