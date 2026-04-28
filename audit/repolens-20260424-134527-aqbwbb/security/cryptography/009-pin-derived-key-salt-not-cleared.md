---
title: "[LOW] PIN-Derived Key Salt Not Cleared After HKDF"
severity: LOW
domain: cryptographic-implementation
lens: cdc-badge-os
labels:
  - hkdf
  - key-management
  - memory-clearing
---

## Summary

The `derive_key_from_pin()` function in the GPG module does not clear the `salt` buffer (chip ID) and the PIN-derived encryption key after use. While the salt itself is not secret (it's the chip ID), the PIN string passed as input should be considered sensitive.

**Location:** `components/mod_gpg/src/GpgStorage.cpp:134-162`

## Impact

- **Minimal impact**: The `salt` is just the chip ID (not a secret), so not clearing it is less critical
- **PIN string**: The `pin` parameter is passed by pointer, so the actual PIN string is in the caller's stack (not this function's stack)
- **Defense in depth**: Good practice to clear all intermediate values for consistency
- **Key material**: The derived `enc_key`/`dec_key` are properly cleared by callers

Note: The impact is limited because:
1. The salt is the chip ID, which is not secret
2. The PIN string is in the caller's stack, not this function's stack
3. The GPG module properly clears the derived keys in the calling functions

## Evidence

File: `components/mod_gpg/src/GpgStorage.cpp`

```cpp
static bool derive_key_from_pin(const char* pin, uint8_t* key_out) {
    if (!key_out) return false;

    // If no PIN provided, use device-specific key
    if (!pin || pin[0] == '\0') {
        return derive_device_key(key_out);
    }

    // Get chip ID as salt (unique per device)
    uint8_t salt[16] = {};  // <-- Not cleared after use
    auto* se = get_se();
    if (se) {
        se->getChipId(salt, sizeof(salt));
    }

    // HKDF: PIN -> 32-byte key
    const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!md) return false;

    int ret = mbedtls_hkdf(
        md,
        salt, sizeof(salt),
        reinterpret_cast<const uint8_t*>(pin), strlen(pin),
        reinterpret_cast<const uint8_t*>(HKDF_INFO), strlen(HKDF_INFO),
        key_out, 32
    );

    // <-- salt not cleared here
    return ret == 0;
}
```

Compare with `derive_device_key()` which properly clears `chip_id` (line 124):
```cpp
static bool derive_device_key(uint8_t* key_out) {
    // ...
    uint8_t chip_id[16] = {};
    // ...
    mbedtls_hkdf(..., chip_id, ...);
    
    mbedtls_platform_zeroize(chip_id, sizeof(chip_id));  // <-- Properly cleared
    return ret == 0;
}
```

## Recommended Fix

Add `mbedtls_platform_zeroize()` to clear the salt after use:

```cpp
static bool derive_key_from_pin(const char* pin, uint8_t* key_out) {
    // ... existing code ...

    int ret = mbedtls_hkdf(
        md,
        salt, sizeof(salt),
        reinterpret_cast<const uint8_t*>(pin), strlen(pin),
        reinterpret_cast<const uint8_t*>(HKDF_INFO), strlen(HKDF_INFO),
        key_out, 32
    );

    mbedtls_platform_zeroize(salt, sizeof(salt));  // <-- Add this
    return ret == 0;
}
```

For consistency, consider also adding a comment explaining why the salt is cleared:
```cpp
// Clear salt (chip ID) for consistency with derive_device_key()
mbedtls_platform_zeroize(salt, sizeof(salt));
```

## References

- [Mbed TLS Platform Util Documentation](https://tls.mbed.org/api/platform__util_8h.html)
- [NIST SP 800-131A Rev 2 - Cryptographic Key Management](https://csrc.nist.gov/publications/detail/sp/800-131a/rev-2/final)
- [FIDO CTAP2 Pin Protocol](https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#pin-protocols)
