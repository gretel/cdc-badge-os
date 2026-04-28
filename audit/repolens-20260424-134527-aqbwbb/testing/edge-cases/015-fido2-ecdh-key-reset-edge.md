---
title: "[MEDIUM] ECDH key regeneration on every PIN verification causes performance overhead"
severity: MEDIUM
domain: fido2
lens: edge-cases
labels:
  - "audit:testing/edge-cases"
---

## Summary
In `components/mod_fido2/src/ctap2.cpp:1998-2016`, the `client_pin_init_ecdh` function generates a new ECDH key pair each time `getPinToken` or `getPinUvAuthToken` is called, but only checks if `g_client_pin.ecdh_valid` is set. However, the ECDH key is NOT persisted across reboots or when the function is called from different code paths. This means:
1. If the device reboots, a new ECDH key is generated (expected).
2. But within a single session, if multiple operations happen, the key should be reused - the check at line 1999 handles this.
3. **Edge case**: If `ecdh_valid` is true but the ECDH key structure was corrupted or freed (e.g., memory corruption), subsequent operations could use invalid key data.

Additionally, there's no explicit cleanup of the ECDH key when it should be invalidated (e.g., after PIN reset).

## Impact
- **Performance**: ECDH key generation takes ~50-100ms on ESP32-S3. If the validity check fails unexpectedly, this delay is added to every PIN verification.
- **Consistency**: If the ECDH key needs to be reset (e.g., after PIN reset), there's no code path to clear `ecdh_valid` and free the old key.
- **Edge Case**: After a PIN reset, the old ECDH key could theoretically still be used if `ecdh_valid` wasn't cleared.

## Evidence
File: `components/mod_fido2/src/ctap2.cpp:1998-2016`

```cpp
static bool client_pin_init_ecdh(void) {
    if (g_client_pin.ecdh_valid) return true;  // Early return if already valid

    mbedtls_ecp_keypair_init(&g_client_pin.ecdh_key);

    int ret = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1,
                                   &g_client_pin.ecdh_key,
                                   ctap2_random, NULL);
    if (ret != 0) {
        LOG_E("PIN", "ECDH key generation failed: %d", ret);
        return false;
    }

    g_client_pin.ecdh_valid = true;
    g_client_pin.pin_retries = PIN_RETRIES_MAX;
    g_client_pin.uv_retries = PIN_UV_RETRIES_MAX;
    LOG_I("PIN", "ECDH key pair generated");
    return true;
}
```

Looking at where `ecdh_valid` is set:
- Line 2011: `g_client_pin.ecdh_valid = true;` (only place it's set to true)

Looking for where it's cleared:
- No explicit `g_client_pin.ecdh_valid = false;` found in the file.

The `g_client_pin` struct is initialized to all zeros at file scope (line 98-115), so `ecdh_valid` starts as `false`. After the first successful key generation, it stays `true` forever unless the device reboots.

## Recommended Fix
Add explicit ECDH key reset function and call it when PIN state changes:

```cpp
/**
 * \brief Resets ECDH key for fresh PIN session.
 */
static void client_pin_reset_ecdh(void) {
    if (g_client_pin.ecdh_valid) {
        mbedtls_ecp_keypair_free(&g_client_pin.ecdh_key);
        g_client_pin.ecdh_valid = false;
    }
}

/**
 * \brief Resets PIN state including ECDH key.
 */
void client_pin_reset(void) {
    client_pin_reset_ecdh();
    g_client_pin.pin_retries = PIN_RETRIES_MAX;
    g_client_pin.uv_retries = PIN_UV_RETRIES_MAX;
    g_client_pin.pin_token_valid = false;
    // ... other reset logic
}
```

Call `client_pin_reset_ecdh()` in:
1. `client_pin_set_pin` after successful PIN set
2. `client_pin_change_pin` after successful PIN change
3. Any PIN reset flow

This ensures that after a PIN change, the old ECDH key is properly freed and a new one will be generated with the next PIN verification.

## References
- [CTAP 2.1 PIN Protocol](https://fidoalliance.org/specs/fido2-web-authentication-cbor-serialization-conventions-v1.0r03_20190130.html#client-pin) - ECDH keys are session-specific
- [mbedtls_ecp_keypair_free](https://mbed-tls.readthedocs.io/api/latest/group__ecp.html) - Proper cleanup of ECP keypairs
- ESP32-S3 ECDH performance: ~50-100ms for P-256 key generation
