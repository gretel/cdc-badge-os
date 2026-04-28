---
title: "[MEDIUM] DEBUG_MODE defaults to enabled, disabling PIN lockouts"
severity: MEDIUM
domain: security
lens: session-nuclei
labels:
  - audit:toolgate/session-nuclei
---

## Summary

In `components/cdc_core/include/cdc_core/feature_flags.h:30-31`, the `DEBUG_MODE` feature flag defaults to `1` (enabled):

```cpp
// Debug Mode (disables lockouts, useful for development)
#ifndef DEBUG_MODE
#define DEBUG_MODE 1
#endif
```

This means that unless explicitly overridden at build time, the firmware will be built with debug mode enabled, which disables PIN lockout functionality.

## Impact

- **Security Risk**: PIN lockouts are critical for brute-force protection. When `DEBUG_MODE=1`, the 3-attempt lockout with 60-second delay is bypassed.
- **Production Deployment Risk**: If a developer builds and flashes the firmware without explicitly setting `DEBUG_MODE=0`, the device will ship with weakened security.
- **Consistency**: The README.md explicitly states `DEBUG_MODE=1` disables lockouts and should be set to `0` for production, but the default is the opposite.

## Evidence

**File**: `components/cdc_core/include/cdc_core/feature_flags.h` (lines 29-32)

```cpp
// Debug Mode (disables lockouts, useful for development)
#ifndef DEBUG_MODE
#define DEBUG_MODE 1
#endif
```

**File**: `README.md` (line 184)

```
**Note:** `DEBUG_MODE=1` disables lockouts for development. Set to 0 for production!
```

The README warns users to set it to 0, but the code defaults to 1.

## Recommended Fix

Change the default value of `DEBUG_MODE` from `1` to `0` so that production-ready firmware has lockouts enabled by default:

```cpp
// Debug Mode (disables lockouts, useful for development)
#ifndef DEBUG_MODE
#define DEBUG_MODE 0
#endif
```

Developers can still override this via:
- `platformio.ini` build flags: `-DDEBUG_MODE=1`
- Kconfig menuconfig
- SDK config

This follows the security principle of "secure by default" - production features should be enabled by default, with debug features opt-in.

## References

- README.md: https://github.com/riatlabs/cdc-badge-os/blob/main/README.md
- Feature Flags: `components/cdc_core/include/cdc_core/feature_flags.h`
- Security.md should document this requirement explicitly
