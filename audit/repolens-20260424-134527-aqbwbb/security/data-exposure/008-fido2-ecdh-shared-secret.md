---
title: "[MEDIUM] FIDO2 ECDH shared secret exposed in DEBUG_MODE logs"
severity: MEDIUM
domain: mod_fido2
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
In `components/mod_fido2/src/ctap2.cpp`, when `DEBUG_MODE` is enabled, the full 32-byte ECDH shared secret (used as AES key for PIN encryption) is logged at lines 2066-2128. This includes the raw Z coordinate and the derived shared secret.

## Impact
- **AES Key Exposure**: The shared secret is the AES key used to encrypt/decrypt the PIN hash; exposure allows full PIN recovery
- **ECDH Z Coordinate**: The raw ECDH shared point (Z) is logged, enabling attackers to derive the same shared secret if they capture the platform public key
- **Platform Key Leakage**: Full platform public key coordinates (X and Y) are logged, aiding replay attacks
- **Default DEBUG_MODE**: `DEBUG_MODE` defaults to 1 in `feature_flags.h:31`, making this exposure common in development builds

## Evidence
File: `components/mod_fido2/src/ctap2.cpp:2066-2128`

Lines 2066-2076 (ECDH Z coordinate):
```cpp
#if DEBUG_MODE
    // Full debug output for manual verification
    LOG_I("PIN", "=== ECDH DEBUG (full 32-byte values) ===");
    LOG_I("PIN", "Z (ECDH x-coord):");
    LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          ecdh_z[0], ecdh_z[1], ...);
    LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          ecdh_z[16], ecdh_z[17], ...);
#endif // DEBUG_MODE
```

Lines 2115-2128 (Derived AES key):
```cpp
#if DEBUG_MODE
    LOG_I("PIN", "AES key (shared secret):");
    LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          shared_secret[0], shared_secret[1], ...);
    LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          shared_secret[16], shared_secret[17], ...);
    LOG_I("PIN", "=== END ECDH DEBUG ===");
#endif
```

Feature flag at `components/cdc_core/include/cdc_core/feature_flags.h:30-31`:
```cpp
#ifndef DEBUG_MODE
#define DEBUG_MODE 1
#endif
```

## Recommended Fix
1. **Remove Z coordinate logging** entirely or log only a hash of Z (e.g., first 4 bytes)
2. **Remove AES key logging** - the shared secret should never appear in logs
3. **Add stricter flag** like `ECDH_DEBUG` separate from general `DEBUG_MODE`
4. **Consider HKDF PRK logging** - also exposed at lines 2100-2109

Example fix:
```cpp
#if DEBUG_MODE
    // Log only first 4 bytes of Z for debugging
    LOG_I("PIN", "ECDH Z (first 4 bytes): %02X%02X%02X%02X...",
          ecdh_z[0], ecdh_z[1], ecdh_z[2], ecdh_z[3]);
    // Full AES key removed for security
#endif
```

## References
- FIDO2 CTAP2 specification: Client PIN protocol
- OWASP: [Logging Cheat Sheet - Sensitive Data](https://cheatsheetseries.owasp.org/cheatsheets/Logging_Cheet_Sheet.html#do-not-log-sensitive-information)
- CTAP2 AES key derivation: HKDF-SHA256 with Z as input key material

</content>