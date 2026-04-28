---
title: "[INFO] FIDO2 ECDH Shared Secret Logged Under DEBUG_MODE"
severity: LOW
domain: gdpr-dsgvo
lens: compliance/gdpr-dsgvo
labels:
  - "audit:compliance/gdpr-dsgvo"
---

## Summary
The FIDO2 module (`components/mod_fido2/src/ctap2.cpp`) logs the full ECDH shared secret (32-byte AES key used for FIDO2 Client PIN protocol) when `DEBUG_MODE` is enabled. This is a cryptographic key that protects the FIDO2 PIN authentication flow.

**Affected logging statements:**
- Lines 2115-2128: `#if DEBUG_MODE` block that logs the complete 32-byte shared secret

## Impact
**Security Risk**: When `DEBUG_MODE` is enabled, the ECDH shared secret (which is used as the AES-256 key for encrypting/decrypting the FIDO2 PIN) is printed to serial output. This means:
1. Anyone with serial access can see the shared secret
2. The shared secret can be used to decrypt FIDO2 PIN values
3. An attacker could potentially intercept and decrypt FIDO2 PIN authentication

**Context**: This is lower risk because:
1. Wrapped in `#if DEBUG_MODE` preprocessor directive
2. Only appears in debug builds (typically not production)
3. Shared secret is ephemeral (per-connection)

**Data Controller Responsibility**: Under Art. 32 DSGVO (security of processing), cryptographic keys should be handled securely. Even debug logging of keys should be considered carefully.

**Evidence**:
```cpp
// components/mod_fido2/src/ctap2.cpp:2115-2128
#if DEBUG_MODE
    LOG_I("PIN", "AES key (shared secret):");
    LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          shared_secret[0], shared_secret[1], shared_secret[2], shared_secret[3],
          shared_secret[4], shared_secret[5], shared_secret[6], shared_secret[7],
          shared_secret[8], shared_secret[9], shared_secret[10], shared_secret[11],
          shared_secret[12], shared_secret[13], shared_secret[14], shared_secret[15]);
    LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          shared_secret[16], shared_secret[17], shared_secret[18], shared_secret[19],
          shared_secret[20], shared_secret[21], shared_secret[22], shared_secret[23],
          shared_secret[24], shared_secret[25], shared_secret[26], shared_secret[27],
          shared_secret[28], shared_secret[29], shared_secret[30], shared_secret[31]);
    LOG_I("PIN", "=== END ECDH DEBUG ===");
#endif
```

## Recommended Fix

### Option 1: Remove Shared Secret Logging (~15 min)
Simply remove or comment out the logging block:

```cpp
// Remove lines 2115-2128 or change to:
#if DEBUG_MODE
    LOG_D("PIN", "ECDH shared secret computed (32 bytes)");
#endif
```

### Option 2: Log Only Hash of Shared Secret (~20 min)
If the logging is useful for debugging, log a hash instead:

```cpp
#if DEBUG_MODE
    uint8_t hash[32];
    mbedtls_sha256(shared_secret, 32, hash, 0);
    LOG_D("PIN", "ECDH shared secret hash: %02X%02X%02X%02X...",
          hash[0], hash[1], hash[2], hash[3]);
#endif
```

### Option 3: Add Separate DEBUG_KEYS Flag (~30 min)
Create a more granular debug flag:

```cpp
// In feature_flags.h
#define DEBUG_KEYS 0  // Default: disabled

// In ctap2.cpp:
#if DEBUG_MODE && DEBUG_KEYS
    // Full shared secret logging
#endif
```

**Recommended**: Option 1 (remove the logging) is simplest and most secure. The shared secret doesn't need to be printed for normal debugging.

## References
- **Art. 32 DSGVO** - Security of processing (cryptographic keys)
- **NIST SP 800-57** - Key management best practices
- **OWASP Cryptographic Storage Cheat Sheet** - https://cheatsheetseries.owasp.org/cheatsheets/Cryptographic_Storage_Cheat_Sheet.html

</content>