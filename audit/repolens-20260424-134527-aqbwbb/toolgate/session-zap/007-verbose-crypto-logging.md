---
title: "[LOW] Verbose PIN/crypto logging in DEBUG_MODE"
severity: LOW
domain: security
lens: toolgate/session-zap
labels:
  - "audit:toolgate/session-zap"
---

## Summary

The FIDO2 CTAP2 module logs PIN hashes, ECDH shared secrets, and encrypted pinTokens in DEBUG_MODE, which could expose sensitive cryptographic material to serial output.

**Locations:** `components/mod_fido2/src/ctap2.cpp` lines 2068-2127, 2465-2564

## Impact

**Sensitive data logged in DEBUG_MODE:**

1. **ECDH shared secret (Z value)** - Lines 2068-2073:
   ```cpp
   LOG_I("PIN", "Z (ECDH x-coord):");
   LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
         ...);
   ```

2. **HKDF PRK (derived key)** - Lines 2101-2105:
   ```cpp
   LOG_I("PIN", "HKDF PRK (HMAC-SHA256(salt=0, IKM=Z)):");
   LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
         ...);
   ```

3. **AES key (shared secret)** - Lines 2116-2122:
   ```cpp
   LOG_I("PIN", "AES key (shared secret):");
   LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
         ...);
   ```

4. **Decrypted PIN hash** - Lines 2546-2550:
   ```cpp
   LOG_D("PIN", "Decrypted PIN hash: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
         ...);
   ```

5. **Platform public keys** - Lines 2487-2504:
   ```cpp
   LOG_I("PIN", "Platform (Chrome) public key X:");
   LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
         ...);
   ```

**Risk scenarios:**
1. **Serial console access**: Anyone with USB serial access can see all logged data
2. **Log files**: If logs are redirected to files, sensitive data persists
3. **Core dumps**: Logs may be included in crash dumps
4. **Debug builds**: Default `DEBUG_MODE=1` means all production builds have verbose logging

## Evidence

**File: `components/mod_fido2/src/ctap2.cpp`**

Lines 2066-2127 (ECDH debug):
```cpp
#if DEBUG_MODE
    LOG_I("PIN", "=== ECDH DEBUG (full 32-byte values) ===");
    LOG_I("PIN", "Z (ECDH x-coord):");
    LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          ecdh_z[0], ecdh_z[1], ecdh_z[2], ecdh_z[3],
          ecdh_z[4], ecdh_z[5], ecdh_z[6], ecdh_z[7],
          ecdh_z[8], ecdh_z[9], ecdh_z[10], ecdh_z[11],
          ecdh_z[12], ecdh_z[13], ecdh_z[14], ecdh_z[15]);
    // ... more lines ...
    LOG_I("PIN", "=== END ECDH DEBUG ===");
#endif
```

Lines 2545-2569 (PIN hash debug):
```cpp
#if DEBUG_MODE
    LOG_D("PIN", "Decrypted PIN hash: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          decrypted_pin_hash[0], decrypted_pin_hash[1], ...);

    // Get stored hash for comparison
    uint8_t stored_hash[16];
    pin_storage_get_fido2_hash(stored_hash);
    LOG_D("PIN", "Stored FIDO2 hash:  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          ...);

    // Debug: compute expected hash for "0000"
    uint8_t test_full[32];
    sha256((const uint8_t*)"0000", 4, test_full);
    LOG_D("PIN", "Expected for 0000: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          ...);
#endif
```

## Recommended Fix

Remove or reduce logging of sensitive data:

**Option 1: Remove ECDH/crypto logging entirely**
```cpp
// Remove lines 2066-2127 (ECDH debug block)
// Keep only high-level status
LOG_I("PIN", "ECDH key pair generated");
LOG_I("PIN", "Shared secret computed");
```

**Option 2: Use higher log level for crypto data**
```cpp
// Change LOG_I to LOG_V (verbose) or LOG_D with more specific condition
#if defined(DEBUG_MODE) && defined(DEBUG_CRYPTO_DETAIL)
    LOG_I("PIN", "Z (ECDH x-coord):");
    LOG_I("PIN", "  %02X%02X%02X%02X...", ...);
#endif
```

**Option 3: Log only hashes, not raw values**
```cpp
// Instead of logging full ECDH Z value
uint8_t z_hash[32];
sha256(ecdh_z, 32, z_hash);
LOG_I("PIN", "Z hash: %02X%02X%02X%02X...", z_hash);
```

## References

- CWE-532: Information Exposure Through Log File
- [OWASP: Logging Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Logging_Cheat_Sheet.html)
- [FIDO2 CTAP2 Debug Logging](https://fidoalliance.org/specs/fido-v2.1-rd-20201209/fido-client-to-authenticator-protocol-v2.1-rd-20201209.html)
