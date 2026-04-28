---
title: "[LOW] HMAC Verification Uses Non-Constant-Time memcmp"
severity: LOW
domain: cryptographic-implementation
lens: cdc-badge-os
labels:
  - timing-attack
  - hmac
  - constant-time
  - fido2
---

## Summary

HMAC verification in FIDO2 pin_uv_auth_param validation uses `memcmp()` which is not constant-time. This could theoretically allow timing attacks to forge pin_token HMACs. The `PinManager::compareHash()` function already implements constant-time comparison and should be used instead.

**Location:** `components/mod_fido2/src/ctap2.cpp:942` and `1572`

## Impact

- **Timing attack potential**: An attacker could measure comparison times to determine how many leading bytes match
- **PIN token forgery**: With enough measurements, an attacker could potentially forge a valid pin_uv_auth_param
- **Limited practicality**: The attack requires:
  - Very precise timing measurements (microsecond level)
  - Many attempts (to average out noise)
  - Close proximity to the device (for consistent timing)

Note: The impact is limited because:
1. FIDO2 has PIN retry limits (6-8 attempts before lockout)
2. The HMAC is 16-32 bytes, so timing differences are small
3. PowerCycleState can reset the counter, but this takes time

## Evidence

File: `components/mod_fido2/src/ctap2.cpp`

MakeCredential HMAC comparison (line 942):
```cpp
mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                g_client_pin.pin_token, sizeof(g_client_pin.pin_token),
                p->client_data_hash, 32,
                expected_hmac);

// Protocol 2 uses first 32 bytes of HMAC
size_t compare_len = (p->pin_uv_auth_protocol == 2) ? 32 : 16;
if (p->pin_uv_auth_param_len < compare_len ||
    memcmp(p->pin_uv_auth_param, expected_hmac, compare_len) != 0) {
    LOG_W("CTAP2", "makeCredential: pinUvAuthParam verification failed");
    return CTAP2_ERR_PIN_AUTH_INVALID;
}
```

GetAssertion HMAC comparison (line 1572):
```cpp
mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                pin_token, PIN_TOKEN_SIZE,
                p->client_data_hash, 32,
                expected_hmac);

if (memcmp(p->pin_uv_auth_param, expected_hmac, compare_len) != 0) {
    LOG_W("CTAP2", "pinUvAuthParam verification failed");
    return CTAP2_ERR_PIN_AUTH_INVALID;
}
```

Correct implementation in PinManager (line 283-288):
```cpp
bool PinManager::compareHash(const uint8_t* h1, const uint8_t* h2, size_t len) const {
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) {
        diff |= h1[i] ^ h2[i];
    }
    return diff == 0;
}
```

## Recommended Fix

1. **Replace memcmp with constant-time comparison** in both locations:
   ```cpp
   // Instead of:
   if (memcmp(p->pin_uv_auth_param, expected_hmac, compare_len) != 0) { ... }

   // Use:
   PinManager pm;
   if (!pm.compareHash(p->pin_uv_auth_param, expected_hmac, compare_len)) { ... }
   ```

2. **Alternatively, add a static helper function** in ctap2.cpp:
   ```cpp
   static bool constant_time_hmac_compare(const uint8_t *a, const uint8_t *b, size_t len) {
       uint8_t diff = 0;
       for (size_t i = 0; i < len; i++) {
           diff |= a[i] ^ b[i];
       }
       return diff == 0;
   }
   ```

3. **Update both makeCredential and getAssertion** functions to use the new comparison

## References

- [Timing Attack on HMAC](https://en.wikipedia.org/wiki/Timing_attack)
- [HMAC.compare_digest() Python docs](https://docs.python.org/3/library/hmac.html#hmac.compare_digest)
- [crypto.timingSafeEqual() Node.js](https://nodejs.org/api/crypto.html#cryptotimingsafeequala-b)
- [FIDO CTAP2 Pin Protocol](https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#pin-protocols)
