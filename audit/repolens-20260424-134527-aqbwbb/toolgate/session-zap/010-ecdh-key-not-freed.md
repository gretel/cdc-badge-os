---
title: "[LOW] ECDH key pair not freed in ClientPIN module"
severity: LOW
domain: security
lens: toolgate/session-zap
labels:
  - "audit:toolgate/session-zap"
---

## Summary

The FIDO2 CTAP2 module generates an ECDH key pair in `g_client_pin.ecdh_key` but never frees it. The mbedtls key structure holds dynamically allocated memory (EC point coordinates, private key scalar) that persists until program termination.

**Location:** `components/mod_fido2/src/ctap2.cpp`
- Line 100: `mbedtls_ecp_keypair ecdh_key;` (structure member)
- Line 2001: `mbedtls_ecp_keypair_init(&g_client_pin.ecdh_key);` (initialization)
- Line 2003: `mbedtls_ecp_gen_key(...)` (key generation)

**Missing:** No call to `mbedtls_ecp_keypair_free(&g_client_pin.ecdh_key)` anywhere in the codebase.

## Impact

**Memory implications:**

1. **Memory leak**: Each ECDH key pair allocates:
   - Private key scalar (32 bytes for P-256)
   - Public key point (64 bytes for P-520, 32-64 bytes for P-256)
   - Group structure metadata
   - Total: ~150-200 bytes per key pair

2. **Lifetime**: The key persists for the entire application lifetime since:
   - `g_client_pin` is a static global structure
   - No cleanup function is called on module unload
   - The key is only freed if the ESP32 resets

3. **Security context**: While the key is regenerated on reset, the memory leak means:
   - Private key material remains in allocated heap memory
   - Memory allocator may not reuse the freed space immediately
   - In long-running applications, this accumulates

**Note:** This is a **LOW** severity issue because:
- The ECDH key is ephemeral (regenerated on each reset)
- The key is already in a static global, so memory is "always there"
- ESP32 has limited heap (~300KB), so a single 200-byte leak is relatively small
- The key is not "leaked" in the security sense - it's just not freed

## Evidence

**File: `components/mod_fido2/src/ctap2.cpp`**

Structure definition (line 95-115):
```cpp
static struct {
    bool initialized;

    // ECDH key pair (generated on init, regenerated on reset)
    mbedtls_ecp_keypair ecdh_key;
    bool ecdh_valid;

    // PIN token (regenerated on each getPinToken)
    uint8_t pin_token[PIN_TOKEN_SIZE];
    bool pin_token_valid;

    // Token permissions (CTAP 2.1) - 0 means all permissions (legacy)
    uint8_t token_permissions;
    uint8_t token_rp_id_hash[32];   // RP restriction (if any)
    bool token_rp_id_set;

    // Retry counters
    uint8_t pin_retries;
    uint8_t uv_retries;
} g_client_pin = {};
```

Key initialization (lines 1998-2015):
```cpp
static bool client_pin_init_ecdh(void) {
    if (g_client_pin.ecdh_valid) return true;

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

**No cleanup found:** Search for `mbedtls_ecp_keypair_free` in the file returns no results for `g_client_pin.ecdh_key`.

**Comparison:** The ephemeral key in `makeCredential()` is properly freed (lines 1007-1048):
```cpp
mbedtls_ecp_keypair ephemeral_key;
mbedtls_ecp_keypair_init(&ephemeral_key);
// ... use key ...
mbedtls_ecp_keypair_free(&ephemeral_key);  // Properly freed
```

## Recommended Fix

Add a cleanup function and call it when the module is unloaded or reset:

**Option 1: Add module-level cleanup**

```cpp
// Add to ctap2.cpp
static void client_pin_cleanup(void) {
    // Clear ECDH key
    mbedtls_ecp_keypair_free(&g_client_pin.ecdh_key);
    mbedtls_ecp_keypair_init(&g_client_pin.ecdh_key);
    g_client_pin.ecdh_valid = false;

    // Clear PIN token
    memset(g_client_pin.pin_token, 0, PIN_TOKEN_SIZE);
    g_client_pin.pin_token_valid = false;

    // Clear permissions
    g_client_pin.token_permissions = 0;
    memset(g_client_pin.token_rp_id_hash, 0, sizeof(g_client_pin.token_rp_id_hash));
    g_client_pin.token_rp_id_set = false;

    // Reset counters
    g_client_pin.pin_retries = PIN_RETRIES_MAX;
    g_client_pin.uv_retries = PIN_UV_RETRIES_MAX;

    g_client_pin.initialized = false;
}

// Call from module shutdown/reset
void mod_fido2_shutdown(void) {
    client_pin_cleanup();
    // ... other cleanup ...
}
```

**Option 2: Add reset function**

```cpp
// Add a reset function that clears and reinitializes
static bool client_pin_reset(void) {
    // Clear existing key
    mbedtls_ecp_keypair_free(&g_client_pin.ecdh_key);

    // Reinitialize
    mbedtls_ecp_keypair_init(&g_client_pin.ecdh_key);
    g_client_pin.ecdh_valid = false;

    // ... rest of reset ...

    return client_pin_init_ecdh();
}
```

**Option 3: Minimal fix - add free before reinit**

```cpp
static bool client_pin_init_ecdh(void) {
    if (g_client_pin.ecdh_valid) return true;

    // Free existing key if any (for reinit scenarios)
    mbedtls_ecp_keypair_free(&g_client_pin.ecdh_key);
    mbedtls_ecp_keypair_init(&g_client_pin.ecdh_key);

    int ret = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1,
                                   &g_client_pin.ecdh_key,
                                   ctap2_random, NULL);
    // ... rest unchanged ...
}
```

**Recommended:** Option 1 provides the most complete cleanup for module shutdown scenarios.

## References

- [mbedtls ECP API](https://mbed-tls.readthedocs.io/en/latest/api-reference/ecp/#_CPPv420mbedtls_ecp_keypair_freeP18mbedtls_ecp_keypair)
- CWE-404: Improper Resource Shutdown or Release
- ESP32 memory management: [Memory Types](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/memory/memory.html)
