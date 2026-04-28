---
title: "[MEDIUM] Non-constant-time PIN hash comparison in FIDO2"
severity: MEDIUM
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
The FIDO2 PIN verification uses a simple hash comparison that may not be constant-time, potentially allowing timing attacks to leak the correct PIN hash:

**File**: `components/mod_fido2/src/ctap2.cpp:2572`
```cpp
// Verify PIN hash
if (!pin_storage_verify_fido2_hash(decrypted_pin_hash)) {
    g_client_pin.pin_retries--;
    LOG_W("PIN", "Invalid PIN, retries left: %d", g_client_pin.pin_retries);
    ...
}
```

**File**: `components/cdc_core/src/PinManager.cpp:280-288`
```cpp
bool PinManager::compareHash(const uint8_t* h1, const uint8_t* h2, size_t len) const {
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) {
        diff |= h1[i] ^ h2[i];
    }
    return diff == 0;
}
```

While `compareHash()` uses a simple XOR-based comparison, it **does not guarantee constant-time execution** because:
1. The compiler may optimize the loop
2. Modern CPUs with branch prediction may introduce timing variations
3. Cache timing differences could leak information

## Impact
- **Timing Attack**: An attacker with precise timing measurements could potentially determine the correct PIN hash byte-by-byte
- **FIDO2 PIN Compromise**: The 16-byte FIDO2 PIN hash could be recovered with ~160 measurements per byte (1600 total for 16 bytes)
- **Brute-force Reduction**: Instead of trying all possible PINs, an attacker could use timing to narrow down candidates

## Evidence
**File**: `components/cdc_core/src/PinManager.cpp:280-288`
The `compareHash()` function uses a simple loop without explicit constant-time guarantees:
```cpp
bool PinManager::compareHash(const uint8_t* h1, const uint8_t* h2, size_t len) const {
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) {
        diff |= h1[i] ^ h2[i];
    }
    return diff == 0;
}
```

**File**: `components/mod_fido2/src/ctap2.cpp:2572`
This hash is used for FIDO2 PIN verification, a critical security boundary.

## Recommended Fix
Use a proven constant-time comparison function from a cryptography library:

**Option 1**: Use MbedTLS's ` mbedtls_mpi_safe_const_compare()` or `mbedtls_ct_memcmp()`
```cpp
#include "mbedtls/cipher.h"

bool PinManager::compareHash(const uint8_t* h1, const uint8_t* h2, size_t len) const {
    return mbedtls_ct_memcmp(h1, h2, len) == 0;
}
```

**Option 2**: Use compiler barrier to prevent optimization
```cpp
bool PinManager::compareHash(const uint8_t* h1, const uint8_t* h2, size_t len) const {
    volatile uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) {
        diff |= h1[i] ^ h2[i];
    }
    return diff == 0;
}
```

**Option 3**: Use ESP32's built-in constant-time comparison
```cpp
#include "esp_crypto.h"

bool PinManager::compareHash(const uint8_t* h1, const uint8_t* h2, size_t len) const {
    return esp_crypto_memcmp(h1, h2, len) == 0;
}
```

## References
- RFC 8252 - OAuth 2.0 for Smart Cards (constant-time comparison)
- MbedTLS Documentation - `mbedtls_ct_memcmp()`
- ESP32 Technical Reference Manual - Crypto Acceleration
- Timing Attack Vulnerabilities (CVE-2013-4216, CVE-2016-2171)
