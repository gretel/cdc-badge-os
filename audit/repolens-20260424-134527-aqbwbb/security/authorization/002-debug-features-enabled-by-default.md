---
title: "[HIGH] DEBUG_MODE enabled by default bypasses security lockouts"
severity: HIGH
domain: Authorization & Access Control
lens: authorization
labels:
  - audit:security/authorization
---

## Summary
The `DEBUG_MODE` feature flag is set to `1` (enabled) by default in `feature_flags.h`. This debug mode disables security lockouts and exposes verbose logging for authentication flows, which can be left enabled in production builds.

**Location:** `components/cdc_core/include/cdc_core/feature_flags.h:30-31`

## Impact
- **Lockout bypass:** Debug mode may disable retry counters and lockout timers for PIN verification
- **Information disclosure:** Verbose logging exposes cryptographic material (ECDH shared secrets, AES keys) to serial output
- **Production deployment risk:** If not explicitly disabled, production firmware may ship with security controls weakened
- **Side-channel leakage:** Debug logs of HMAC values and key material can aid attackers in recovering secrets

## Evidence
Default feature flag definition at `components/cdc_core/include/cdc_core/feature_flags.h:30-31`:
```cpp
// Debug Mode (disables lockouts, useful for development)
#ifndef DEBUG_MODE
#define DEBUG_MODE 1
#endif
```

Verbose debug logging in FIDO2 PIN authentication at `components/mod_fido2/src/ctap2.cpp:2066-2128`:
```cpp
#if DEBUG_MODE
    // Full debug output for manual verification
    LOG_I("PIN", "=== ECDH DEBUG (full 32-byte values) ===");
    LOG_I("PIN", "Z (ECDH x-coord):");
    LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          ecdh_z[0], ecdh_z[1], ecdh_z[2], ecdh_z[3], ecdh_z[4], ecdh_z[5], ecdh_z[6], ecdh_z[7],
          ecdh_z[8], ecdh_z[9], ecdh_z[10], ecdh_z[11], ecdh_z[12], ecdh_z[13], ecdh_z[14], ecdh_z[15]);
    LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          ecdh_z[16], ecdh_z[17], ecdh_z[18], ecdh_z[19], ecdh_z[20], ecdh_z[21], ecdh_z[22], ecdh_z[23],
          ecdh_z[24], ecdh_z[25], ecdh_z[26], ecdh_z[27], ecdh_z[28], ecdh_z[29], ecdh_z[30], ecdh_z[31]);
#endif // DEBUG_MODE
```

This logs the full 32-byte ECDH shared secret (Z coordinate) and derived AES key to serial output when DEBUG_MODE is enabled.

## Recommended Fix
Change the default value of `DEBUG_MODE` to `0` (disabled) in `components/cdc_core/include/cdc_core/feature_flags.h`:

```cpp
// Change line 30-31 from:
#ifndef DEBUG_MODE
#define DEBUG_MODE 1
#endif

// To:
#ifndef DEBUG_MODE
#define DEBUG_MODE 0
#endif
```

This ensures:
1. Production builds have debug features disabled by default
2. Developers must explicitly enable `DEBUG_MODE` for development (e.g., via build flags)
3. Security lockouts remain active unless intentionally disabled for debugging

Additionally, consider:
- Adding a build configuration check that warns if `DEBUG_MODE=1` is set for production builds
- Using separate build profiles (debug vs. release) with different default values

## References
- OWASP Secure Coding Practices: https://owasp.org/www-project-secure-coding-practices-quick-reference-guide/
- NIST SP 800-53 Rev. 5 AC-17: Administrative Privileged Access
- FIDO2 CTAP2 specification section 5.10 (PIN Protocol): https://fidoalliance.org/specs/fido-v2.1-rd-20210309/fido-client-to-authenticator-protocol-v2.1-rd-20210309.html
