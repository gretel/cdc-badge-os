---
title: "[HIGH] Default DEBUG_MODE=1 disables security lockouts for production builds"
severity: HIGH
domain: secure-sdlc
lens: compliance
labels:
  - "security"
  - "debug-mode"
---

## Summary
The `DEBUG_MODE` feature flag is set to `1` by default in `components/cdc_core/include/cdc_core/feature_flags.h:31`. This disables PIN lockout protection (3-attempt brute-force protection) and increases log verbosity. The README.md explicitly states `DEBUG_MODE=1` disables lockouts for development and should be set to `0` for production, but there is no enforcement or CI check to ensure this is changed before deployment.

## Impact
- **Security Risk**: With `DEBUG_MODE=1`, the 3-attempt PIN lockout is disabled, allowing unlimited brute-force attempts against the Badge PIN, FIDO2 PW1, and GPG PW3.
- **Production Deployment Risk**: Developers may inadvertently ship firmware with debug mode enabled if they forget to update the flag.
- **Information Leakage**: Increased log verbosity may expose sensitive information via serial output.

## Evidence
File: `components/cdc_core/include/cdc_core/feature_flags.h:30-31`
```cpp
// Debug Mode (disables lockouts, useful for development)
#ifndef DEBUG_MODE
#define DEBUG_MODE 1
#endif
```

File: `README.md:87`
```
**Note:** `DEBUG_MODE=1` disables lockouts for development. Set to 0 for production!
```

File: `README.md:187`
```
build_flags =
    -DDEBUG_MODE=0
```

## Recommended Fix
1. Change default value from `1` to `0` in `components/cdc_core/include/cdc_core/feature_flags.h:31`:
   ```cpp
   #ifndef DEBUG_MODE
   #define DEBUG_MODE 0
   #endif
   ```

2. Add a CI check in `build.yml` that fails the build if `DEBUG_MODE=1` is detected:
   ```yaml
   - name: Check DEBUG_MODE
     run: |
       if grep -q "DEBUG_MODE 1" components/cdc_core/include/cdc_core/feature_flags.h; then
         echo "ERROR: DEBUG_MODE must be set to 0 for release builds!"
         exit 1
       fi
   ```

3. Add a warning in the release process (release job) to remind developers to verify `DEBUG_MODE=0`.

## References
- CRA (Cyber-Resilience Act) Article 10 - Essential characteristics of relevant digital products
- ISO 27001 A.12.4 - Logging and monitoring
- OWASP Cheat Sheet: Debugging
