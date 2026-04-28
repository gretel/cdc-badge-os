---
title: "[LOW] Verbose Cryptographic Debug Logging in DEBUG_MODE"
severity: LOW
domain: api-security
lens: session-zap-api
labels:
  - audit:toolgate/session-zap-api
---

## Summary

The `DEBUG_MODE` feature flag (default: 1) enables verbose logging of cryptographic operations in the FIDO2 module. When enabled, sensitive cryptographic material (ECDH keys, HKDF outputs, AES keys) is logged to the serial console.

**Affected File:** `components/mod_fido2/src/ctap2.cpp`
**Lines:** Multiple (see Evidence)

## Impact

When `DEBUG_MODE` is enabled (default):
- **Key material exposure** - ECDH shared secrets, AES keys logged to serial
- **Side-channel information** - Full cryptographic state visible in logs
- **Production risk** - Developers may ship firmware with debug logging enabled

## Evidence

```cpp
// ECDH Z value (shared secret)
#if DEBUG_MODE
    LOG_I("PIN", "=== ECDH DEBUG (full 32-byte values) ===");
    LOG_I("PIN", "Z (ECDH x-coord):");
    // ... full 32-byte Z value logged
#endif

// HKDF PRK (pseudo-random key)
#if DEBUG_MODE
    LOG_I("PIN", "HKDF PRK (HMAC-SHA256(salt=0, IKM=Z)):");
    LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          prk[0], prk[1], prk[2], prk[3], prk[4], prk[5], prk[6], prk[7], ...);
#endif

// AES session key
#if DEBUG_MODE
    LOG_I("PIN", "AES key (shared secret):");
    LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          shared_secret[0], shared_secret[1], ...);
#endif

// ECDH public keys
#if DEBUG_MODE
    LOG_I("PIN", "=== Our ECDH public key ===");
    LOG_I("PIN", "X: %02X%02X%02X%02X %02X%02X%02X%02X...", pub_x[0], ...);
#endif
```

## Recommended Fix

1. **Change default to 0** (disabled) for production-ready firmware
2. **Use separate DEBUG_CRYPTO flag** for verbose crypto logging
3. **Add build warning** when DEBUG_MODE is enabled

**Option 1 - Separate crypto debug flag:**
```cpp
// Debug Mode (disables lockouts, useful for development)
#ifndef DEBUG_MODE
#define DEBUG_MODE 1
#endif

// Verbose crypto logging (disable for production!)
#ifndef DEBUG_CRYPTO
#define DEBUG_CRYPTO 0
#endif
```

**Option 2 - Add compile warning:**
```cpp
#if DEBUG_MODE
#warning "DEBUG_MODE is enabled - cryptographic keys will be logged!"
#endif
```

**Option 3 - Runtime warning on boot:**
```cpp
#if DEBUG_MODE
    Console::printf("WARNING: DEBUG_MODE enabled - Crypto logs will show key material!\r\n");
#endif
```

## References

- CWE-532: Insertion of sensitive information into log file
- CWE-312: Secure storage of sensitive data
- FIDO2 CTAP2 specification
