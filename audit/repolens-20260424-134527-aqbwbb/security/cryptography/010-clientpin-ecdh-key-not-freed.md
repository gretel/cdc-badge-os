---
title: "[LOW] ClientPIN ECDH Key Not Freed on CTAP2 Reset"
severity: LOW
domain: cryptographic-implementation
lens: cdc-badge-os
labels:
  - ecdh
  - key-management
  - resource-leak
---

## Summary

The FIDO2 CTAP2 ClientPIN module initializes an ECDH key pair (`g_client_pin.ecdh_key`) in `client_pin_init_ecdh()` but never frees it. While this is a minor issue (the key is freed when the process exits), proper cleanup is important for long-running systems and for implementing the CTAP2 `reset` command correctly.

**Location:** `components/mod_fido2/src/ctap2.cpp:1998-2013`

## Impact

- **Memory leak**: The ECDH key pair structure and its internal BIGNUMs are never freed
- **Reset command**: The CTAP2 `reset` command (0x07) should reset the ECDH key but currently doesn't
- **Long-running systems**: For a device that runs for months/years, this is negligible
- **Security**: The ECDH key is regenerated on each reset anyway, so the leak is mostly about the structure, not the key material

Note: The impact is limited because:
1. The ECDH key is only ~100-200 bytes (P-256 curve)
2. The key is regenerated on each `reset` command
3. The device resets periodically (power cycle)

## Evidence

File: `components/mod_fido2/src/ctap2.cpp`

ECDH key initialization (lines 1998-2013):
```cpp
static bool client_pin_init_ecdh(void) {
    if (g_client_pin.ecdh_valid) return true;

    mbedtls_ecp_keypair_init(&g_client_pin.ecdh_key);  // <-- Init here

    int ret = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1,
                                  &g_client_pin.ecdh_key,
                                  ctap2_random, NULL);
    if (ret != 0) {
        mbedtls_ecp_keypair_free(&g_client_pin.ecdh_key);
        return false;
    }

    g_client_pin.ecdh_valid = true;
    g_client_pin.pin_retries = PIN_RETRIES_MAX;
    g_client_pin.uv_retries = PIN_UV_RETRIES_MAX;
    return true;
}
```

ECDH key is never freed anywhere in the file. Compare with ephemeral keys which are properly freed (line 1048):
```cpp
mbedtls_ecp_keypair_free(&ephemeral_key);
```

CTAP2 reset command (0x07) doesn't reset the ECDH key:
```cpp
// In ctap2_process_command, 0x07 is "reset" but there's no handler
// that clears g_client_pin.ecdh_key
```

## Recommended Fix

1. **Add a cleanup function** for ClientPIN state:
```cpp
static void client_pin_reset(void) {
    // Clear the ECDH key so it's regenerated on next use
    mbedtls_ecp_keypair_free(&g_client_pin.ecdh_key);
    mbedtls_ecp_keypair_init(&g_client_pin.ecdh_key);
    g_client_pin.ecdh_valid = false;
    
    // Reset other state
    g_client_pin.pin_token_valid = false;
    g_client_pin.pin_retries = PIN_RETRIES_MAX;
    g_client_pin.uv_retries = PIN_UV_RETRIES_MAX;
}
```

2. **Call cleanup on CTAP2 reset command**:
```cpp
// In ctap2_process_command, for command 0x07 (reset):
case 0x07:  // authenticatorReset
    client_pin_reset();
    // ... rest of reset handling
    break;
```

3. **Alternatively, add cleanup to ctap2_init()** if the ECDH key should be regenerated on each full initialization:
```cpp
bool ctap2_init(void) {
    LOG_I("CTAP2", "Initializing...");
    memset(&g_ctap2, 0, sizeof(g_ctap2));
    g_ctap2.initialized = true;
    
    // Reset ClientPIN state
    client_pin_reset();
    
    LOG_I("CTAP2", "Initialized");
    return true;
}
```

## References

- [FIDO CTAP2 Specification - Reset Command](https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#reset)
- [Mbed TLS ECP Documentation](https://tls.mbed.org/api/ecp_8h.html)
- [CTAP2 authenticatorReset Command](https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#authenticatorReset)
