---
title: "[LOW] Shared secret (ECDH) not cleared after PIN token generation"
severity: LOW
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
The shared secret derived from ECDH key exchange is used to encrypt the pinToken but is never explicitly cleared from memory. The shared secret is stored in a local variable within the `client_pin_get_pin_token` function, but after the function returns, the stack memory may still contain the secret.

**File**: `components/mod_fido2/src/ctap2.cpp:2403-2470`
```cpp
static uint8_t client_pin_get_pin_token(const uint8_t *params, uint16_t params_len,
                                         uint8_t *response, uint16_t *response_len) {
    // ...
    // Get stored ECDH key
    mbedtls_ecp_keypair *ecdh_key = g_client_pin.ecdh_key;
    
    // Get platform public key
    uint8_t platform_key_x[32] = {0};
    uint8_t platform_key_y[32] = {0};
    // ...
    
    // Compute shared secret (ECDH)
    uint8_t shared_secret[32];
    mbedtls_ecp_point point;
    mbedtls_mpi z;
    mbedtls_ecp_point_init(&point);
    mbedtls_mpi_init(&z);
    
    // Load platform public key into point
    // ...
    
    // Compute shared secret
    mbedtls_ecp_mul(&ecdh_key->grp, &point, &ecdh_key->d, &point, mbedtls_ctr_drbg_random, NULL);
    mbedtls_ecp_write_key_ext(&point, shared_secret, 32);
    
    // Use shared secret to encrypt pinToken
    // ...
    
    // Function returns but shared_secret is not cleared
}
```

The `shared_secret` array contains the ECDH-derived key used for AES encryption of the pinToken.

## Impact
- **Stack Memory Leak**: The shared secret remains on the stack after the function returns
- **RAM Dump Attack**: An attacker with physical access could dump RAM and recover the shared secret
- **Token Decryption**: If the shared secret is recovered, an attacker could decrypt captured pinTokens
- **Re-use Potential**: The shared secret could be used to encrypt/decrypt other data

## Evidence
**File**: `components/mod_fido2/src/ctap2.cpp:2403-2633`
The shared secret is computed and used but never cleared:
```cpp
uint8_t shared_secret[32];  // ECDH shared secret
// ... ECDH computation ...
// ... use shared_secret for AES encryption ...
// Function returns, shared_secret still in stack memory
```

**File**: `components/mod_fido2/src/ctap2.cpp:2597-2612`
```cpp
if (!aes_256_cbc_encrypt_p2(shared_secret, g_client_pin.pin_token, PIN_TOKEN_SIZE, encrypted_token)) {
```

The shared secret is used for encryption but not cleared afterward.

## Recommended Fix
Clear the shared secret immediately after use:

```cpp
// After encrypting the pinToken
if (!aes_256_cbc_encrypt_p2(shared_secret, g_client_pin.pin_token, PIN_TOKEN_SIZE, encrypted_token)) {
    // Clear shared secret on error
    memset(shared_secret, 0, sizeof(shared_secret));
    response[0] = CTAP2_ERR_OTHER;
    *response_len = 1;
    return CTAP2_ERR_OTHER;
}

// Clear shared secret after successful encryption
memset(shared_secret, 0, sizeof(shared_secret));

// Build response...
```

Also clear the ECDH point and MPI structures:

```cpp
// After computing shared secret
uint8_t shared_secret[32];
mbedtls_ecp_write_key_ext(&point, shared_secret, 32);

// Clear the point and MPI
mbedtls_ecp_point_init(&point);
mbedtls_mpi_init(&z);
// ... compute ...
// Clear before freeing
mbedtls_mpi_free(&z);
mbedtls_ecp_point_free(&point);

// Clear shared secret
memset(shared_secret, 0, sizeof(shared_secret));
```

## References
- NIST SP 800-56A - ECDH Key Agreement
- CWE-459: Incomplete Cleanup of Sensitive Data
- Mbed TLS Documentation - `mbedtls_ecp_point`, `mbedtls_mpi`

</content>