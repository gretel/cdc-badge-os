---
title: "[MEDIUM] DEBUG_MODE=1 (default) disables PIN lockouts for all authentication"
severity: MEDIUM
domain: rate-abuse
lens: rate-abuse-debug
labels:
  - "audit:security/rate-abuse"
---

## Summary
The `DEBUG_MODE` feature flag defaults to `1` (enabled) in `components/cdc_core/include/cdc_core/feature_flags.h`. When enabled, it disables PIN lockouts for FIDO2 authentication, allowing unlimited brute-force attempts.

**Location:** `components/cdc_core/include/cdc_core/feature_flags.h:30-32`

```c
// Debug Mode (disables lockouts, useful for development)
#ifndef DEBUG_MODE
#define DEBUG_MODE 1  // Default is ENABLED!
#endif
```

**Location:** `components/mod_fido2/src/ctap2.cpp:1559, 2066, 2100, 2115, 2279, 2463, 2485, 2545`

DEBUG_MODE controls verbose logging and affects lockout behavior in multiple places throughout the FIDO2 module.

## Impact
With `DEBUG_MODE=1` (the default):
- FIDO2 PIN lockouts may be bypassed or have reduced timeout periods
- Verbose debug logging leaks internal state (PIN hashes, ECDH secrets)
- An attacker with serial/USB access can enumerate PIN attempts without lockout
- Production builds may accidentally ship with DEBUG_MODE=1 if not explicitly set to 0

The README explicitly warns: "**Set to 0 for production!**" but the default is 1, creating a risk of accidental deployment with weakened security.

## Evidence
- **File:** `components/cdc_core/include/cdc_core/feature_flags.h`
- **Line 31:** `#define DEBUG_MODE 1` (default enabled)
- **File:** `components/mod_fido2/src/ctap2.cpp:1559`
- **Lines 2066-2076, 2100-2109, 2115-2128:** Debug logging of ECDH shared secrets
- **Lines 2463-2475, 2485-2509, 2545-2569:** Debug logging of PIN hashes
- **File:** `README.md:87, 169` - Documentation warning about DEBUG_MODE

From `README.md`:
```
**Note:** `DEBUG_MODE=1` disables PIN lockouts for development. Set to 0 for production!
```

## Recommended Fix
**Option 1: Change default to DEBUG_MODE=0**
- Update `feature_flags.h` to default to `#define DEBUG_MODE 0`
- Developers explicitly enable debug mode when needed
- Production builds are safer by default

**Option 2: Separate DEBUG_LOG and DEBUG_LOCKOUT flags**
- Split into two flags: `DEBUG_LOG` (verbose logging) and `DEBUG_NO_LOCKOUT` (disable lockouts)
- Default: `DEBUG_LOG=1` for debugging, `DEBUG_NO_LOCKOUT=0` for security
- More granular control over development vs. production behavior

**Implementation steps (Option 1):**
1. Edit `components/cdc_core/include/cdc_core/feature_flags.h`
2. Change line 31 from `#define DEBUG_MODE 1` to `#define DEBUG_MODE 0`
3. Update README to reflect new default behavior
4. Add CI check to warn if DEBUG_MODE=1 in release builds

## References
- OWASP: [Security by Default](https://cheatsheetseries.owasp.org/cheatsheets/Default_Passwords_Cheat_Sheet.html)
- CWE-1339: Insufficient Configuration Validation
