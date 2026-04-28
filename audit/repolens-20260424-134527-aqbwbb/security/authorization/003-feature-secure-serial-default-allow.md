---
title: "[MEDIUM] FEATURE_SECURE_SERIAL disabled by default allows unrestricted serial access"
severity: MEDIUM
domain: Authorization & Access Control
lens: authorization
labels:
  - audit:security/authorization
---

## Summary
The `FEATURE_SECURE_SERIAL` flag defaults to `0` (disabled) unless explicitly enabled via `CONFIG_SECURE_SERIAL`. When disabled, all serial commands execute without PIN authentication, allowing unrestricted access to device functions.

**Location:** `components/cdc_core/include/cdc_core/feature_flags.h:16-22`

## Impact
- **Unauthenticated access:** All serial commands (including `REBOOT`, `TR01_WIPE`, `NVS_CLEAR`) are accessible without PIN
- **Physical security bypass:** Anyone with physical access and a serial adapter can execute privileged commands
- **Inconsistent security posture:** Modules register commands with `requiresAuth = true`, but this is ignored when `FEATURE_SECURE_SERIAL = 0`
- **Default-allow pattern:** The system defaults to permissive rather than restrictive access control

## Evidence
Feature flag definition at `components/cdc_core/include/cdc_core/feature_flags.h:16-22`:
```cpp
// Secure Serial (require PIN for serial commands)
// Maps from Kconfig CONFIG_SECURE_SERIAL
#ifdef CONFIG_SECURE_SERIAL
#define FEATURE_SECURE_SERIAL 1
#else
#ifndef FEATURE_SECURE_SERIAL
#define FEATURE_SECURE_SERIAL 0  // Default: disabled
#endif
#endif
```

Authentication check in command processing at `components/serial_cmd/src/CommandRegistry.cpp:114-139`:
```cpp
#if FEATURE_SECURE_SERIAL
    // Check if PIN is blocked (lockout or retries exhausted)
    // When blocked, only PING is allowed (to check device is alive)
    auto& pm = cdc::core::PinManager::instance();
    if (pm.isBadgeBlocked()) {
        if (strcasecmp(cmdBuf, "PING") != 0) {
            // Show lockout message
            return true;  // Command blocked
        }
        // PING is allowed even when blocked
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

When `FEATURE_SECURE_SERIAL` is `0`, the entire block is skipped and commands execute without authentication.

Command registration at `components/serial_cmd/src/SerialCmd.cpp:1440-1485` shows commands marked with `requiresAuth`:
```cpp
reg.registerCommand({"REBOOT", "Restart the device", cmdReboot, "system", true});
reg.registerCommand({"NVS_DEL", "Delete NVS key/namespace", cmdNvsDel, "nvs", true});
reg.registerCommand({"NVS_CLEAR", "Erase entire NVS (NVS_CLEAR YES)", cmdNvsClear, "nvs", true});
reg.registerCommand({"TR01_WIPE", "Factory reset (TR01_WIPE CONFIRM)", cmdTr01Wipe, "tr01", true});
```

However, these flags are only respected when `FEATURE_SECURE_SERIAL` is enabled.

## Recommended Fix
Change the default value of `FEATURE_SECURE_SERIAL` to `1` (enabled) in `components/cdc_core/include/cdc_core/feature_flags.h`:

```cpp
// Change line 20 from:
#define FEATURE_SECURE_SERIAL 0

// To:
#define FEATURE_SECURE_SERIAL 1
```

Alternatively, update the Kconfig (`components/serial_cmd/Kconfig.projbuild`) to make it enabled by default:
```kconfig
config SECURE_SERIAL
    bool "Enable Secure Serial (require PIN authentication)"
    default y  # Already correct
```

This follows the security principle of **deny-by-default** - the system should require authentication unless explicitly configured otherwise.

## References
- OWASP Authentication Cheat Sheet: https://cheatsheetseries.owasp.org/cheatsheets/Authentication_Cheat_Sheet.html
- NIST SP 800-53 Rev. 5 IA-2: Identification and Authentication (Organizational Users)
- Security-by-default best practices: https://cheatsheetseries.owasp.org/cheatsheets/Default_Passwords_Cheat_Sheet.html
