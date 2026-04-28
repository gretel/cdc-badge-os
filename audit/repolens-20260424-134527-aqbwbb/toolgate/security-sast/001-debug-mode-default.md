---
title: "[HIGH] DEBUG_MODE enabled by default disables security lockouts"
severity: HIGH
domain: security
lens: sast
labels:
  - "security"
  - "configuration"
  - "debug-mode"
---

## Summary
The `DEBUG_MODE` feature flag is set to `1` (enabled) by default in `components/cdc_core/include/cdc_core/feature_flags.h:31`. When enabled, this flag disables security lockouts and makes the device easier to test but less secure for production use.

## Impact
- **Security Risk**: Debug mode disables lockout mechanisms that protect against brute-force attacks on PINs
- **Production Risk**: If firmware is built without explicitly disabling DEBUG_MODE, production devices may ship with reduced security
- **Attack Surface**: Developers may forget to disable DEBUG_MODE when building release firmware

## Evidence
File: `components/cdc_core/include/cdc_core/feature_flags.h:30-32`
```cpp
// Debug Mode (disables lockouts, useful for development)
#ifndef DEBUG_MODE
#define DEBUG_MODE 1
#endif
```

Usage in FIDO2 module (`components/mod_fido2/src/ctap2.cpp:1559`):
```cpp
#if DEBUG_MODE
// Debug code that disables certain checks
```

## Recommended Fix
Change the default value from `1` to `0` so that debug features are **disabled by default** and must be explicitly enabled for development:

```cpp
// Debug Mode (disables lockouts, useful for development)
#ifndef DEBUG_MODE
#define DEBUG_MODE 0  // Changed from 1 to 0
#endif
```

This follows the security principle of **"secure by default"** - production builds should have all security features enabled unless explicitly overridden.

## References
- CWE-483: Incorrect Default Permissions
- OWASP: "Define configuration defaults to be secure"
- ESP-IDF Feature Flag best practices
