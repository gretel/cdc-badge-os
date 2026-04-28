---
title: "[MEDIUM] FEATURE_SECURE_SERIAL disabled by default allows unauthenticated serial access"
severity: MEDIUM
domain: cdc-badge-os
lens: library/cdc-badge-os
labels:
  - "audit:toolgate/session-nuclei"
---

## Summary
The `FEATURE_SECURE_SERIAL` flag is set to `0` (disabled) by default in `components/cdc_core/include/cdc_core/feature_flags.h:15-22`. When disabled, all serial commands execute without PIN authentication, allowing anyone with serial access to:
- Read NVS storage
- Modify system settings
- Delete secure element data
- Reboot the device

**Location**: `components/cdc_core/include/cdc_core/feature_flags.h:15-22`

## Impact
- **No Authentication**: All 30+ serial commands accessible without PIN
- **Data Exposure**: NVS commands reveal stored settings, potentially including keys
- **Data Corruption**: NVS and TR01 delete commands can wipe user data
- **Physical Access Required**: Serial console at 115200 baud (USB CDC)

## Evidence
```cpp
// components/cdc_core/include/cdc_core/feature_flags.h:15-22
// Secure Serial (require PIN for serial commands)
// Maps from Kconfig CONFIG_SECURE_SERIAL
#ifdef CONFIG_SECURE_SERIAL
#define FEATURE_SECURE_SERIAL 1
#else
#ifndef FEATURE_SECURE_SERIAL
#define FEATURE_SECURE_SERIAL 0
#endif
#endif
```

Command registration in `SerialCmd.cpp:1441-1488`:
```cpp
#if FEATURE_SECURE_SERIAL
    // Authentication commands
    reg.registerCommand({"AUTH", "Authenticate with PIN", cmdAuth, "auth", false});
    reg.registerCommand({"LOGOUT", "End authenticated session", cmdLogout, "auth", false});
#endif
```

When `FEATURE_SECURE_SERIAL=0`, the `AUTH` command is not even registered.

## Recommended Fix
Change the default to `1` for production-ready security:

```cpp
// components/cdc_core/include/cdc_core/feature_flags.h:15-22
// Secure Serial (require PIN for serial commands)
// Maps from Kconfig CONFIG_SECURE_SERIAL
#ifdef CONFIG_SECURE_SERIAL
#define FEATURE_SECURE_SERIAL 1
#else
#ifndef FEATURE_SECURE_SERIAL
#define FEATURE_SECURE_SERIAL 1  // Changed from 0 to 1
#endif
#endif
```

This ensures:
1. All serial commands require authentication by default
2. Only PING and AUTH commands work without login
3. Developers can disable with `-DFEATURE_SECURE_SERIAL=0` for debugging

## References
- `components/cdc_core/include/cdc_core/feature_flags.h` - Feature flag definitions
- `components/serial_cmd/src/SerialCmd.cpp:1441-1488` - Command registration
- `components/serial_cmd/src/CommandRegistry.cpp:114-139` - Auth check logic
