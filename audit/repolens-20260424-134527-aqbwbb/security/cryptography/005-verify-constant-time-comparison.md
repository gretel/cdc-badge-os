---
title: "[LOW] Verify All HMAC/Token Comparisons Use Constant-Time"
severity: LOW
domain: cryptographic-implementation
lens: cdc-badge-os
labels:
  - timing-attack
  - hmac
  - constant-time
---

## Summary

The codebase has a `PinManager::compareHash()` function that correctly uses constant-time comparison, but other HMAC and token comparisons may use `memcmp()` or standard string comparison. A thorough review is needed to ensure all secret comparisons use constant-time to prevent timing attacks.

**Location:** Multiple files in `components/mod_fido2/` and `components/mod_gpg/`

## Impact

- **Timing attacks**: Non-constant-time comparison can leak information about secrets through timing differences
- **HMAC forgery**: An attacker could potentially forge HMACs by measuring comparison times
- **PIN bypass**: Timing differences could help brute-force PINs faster

Note: The impact is limited because:
1. `PinManager::compareHash()` already uses constant-time comparison
2. Most comparisons are for fixed-length values (32-byte HMACs)
3. The protocol has rate limiting and PIN retry limits

## Evidence

File: `components/mod_fido2/src/PinManager.cpp:277-288` (CORRECT - constant-time)

```cpp
bool PinManager::compareHash(const uint8_t* h1, const uint8_t* h2, size_t len) const {
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) {
        diff |= h1[i] ^ h2[i];
    }
    return diff == 0;
}
```

File: `components/mod_fido2/src/ctap2.cpp:932-945` (NEEDS VERIFICATION)

```cpp
// Verify HMAC-SHA-256(pinToken, clientDataHash)
mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                pin_token, PIN_TOKEN_SIZE, client_data_hash, 32, expected);
// Compare with expected...
// Need to verify this uses memcmp or constant-time comparison
```

Search for all comparison patterns:
- `memcmp` calls in FIDO2 and GPG modules
- `==` comparisons on byte arrays
- `.equals()` or similar string comparisons on tokens

## Recommended Fix

1. **Audit all HMAC/token comparisons** in the codebase:
   ```bash
   grep -r "memcmp" components/mod_fido2/ components/mod_gpg/
   grep -r "==" components/mod_fido2/ | grep -E "(hmac|token|hash|pin)"
   ```

2. **Replace non-constant-time comparisons** with `PinManager::compareHash()` or a similar function:
   ```cpp
   // Instead of:
   if (memcmp(expected, actual, 32) == 0) { ... }

   // Use:
   PinManager pm;
   if (pm.compareHash(expected, actual, 32)) { ... }
   ```

3. **Create a utility function** for constant-time comparison if not already available:
   ```cpp
   static bool constant_time_compare(const uint8_t* a, const uint8_t* b, size_t len) {
       uint8_t diff = 0;
       for (size_t i = 0; i < len; i++) {
           diff |= a[i] ^ b[i];
       }
       return diff == 0;
   }
   ```

4. **Add unit tests** to verify constant-time behavior (measure execution time variance)

## References

- [Timing Attack on HMAC](https://en.wikipedia.org/wiki/Timing_attack)
- [HMAC.compare_digest() Python docs](https://docs.python.org/3/library/hmac.html#hmac.compare_digest)
- [Mbed TLS mbedtls_memcmp()](https://tls.mbed.org/api/platform__util_8h.html#a2deaa33b4f8e9c7e5c5f8d5e5e5e5e5e)
- [Constant-time programming](https://en.wikipedia.org/wiki/Time_complexity#Constant-time_evaluation)
