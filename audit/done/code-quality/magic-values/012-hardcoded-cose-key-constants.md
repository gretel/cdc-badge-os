---
title: "[MEDIUM] Hardcoded COSE key constants in FIDO2 CBOR encoding"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "magic-values"
---

## Summary
COSE (CBOR Object Signing and Encryption) key constants for curve types and coordinate keys are used as magic numbers in the FIDO2 CTAP2 protocol implementation. While algorithm constants (COSE_ALG_ES256, COSE_ALG_EDDSA) are defined in `ctap2.h`, other essential COSE key constants are missing.

**Files affected:**
- `components/mod_fido2/src/ctap2.cpp:2290-2320` - `client_pin_get_key_agreement` COSE key encoding
- `components/mod_fido2/src/ctap2.cpp:3095-3115` - Credential management COSE key encoding

**Magic values found:**
- `-1` - `crv` (curve) key identifier
- `-2` - `x` coordinate key identifier
- `-3` - `y` coordinate key identifier
- `1` - `kty` (key type) value for EC2
- `2` - `kty` (key type) value for EC2 (inconsistent - sometimes 1, sometimes 2)
- `3` - `alg` (algorithm) key identifier
- `6` - `crv` (curve) value for Ed25519
- `-25` - ECDH-ES+HKDF-256 algorithm value

## Impact
- **Standards compliance**: These are standard COSE key identifiers from RFC 8152, but the connection is not explicit in code
- **Maintainability**: Adding support for new curves or algorithms requires searching through encoding logic
- **Readability**: `cbor_encode_int(&w, -2)` doesn't convey "this is the 'x' coordinate key"
- **Error-prone**: Easy to confuse coordinate key identifiers with algorithm values
- **Documentation**: No link to COSE specification in the code

## Evidence
```cpp
// components/mod_fido2/src/ctap2.cpp:2290-2320
// 0x01: keyAgreement (COSE_Key)
cbor_encode_uint(&w, 0x01);
cbor_encode_map(&w, 5);

// kty: EC2 (2)
cbor_encode_uint(&w, 1);
cbor_encode_uint(&w, 2);

// alg: ECDH-ES+HKDF-256 (-25)
cbor_encode_uint(&w, 3);
cbor_encode_int(&w, -25);

// crv: P-256 (1)
cbor_encode_int(&w, -1);
cbor_encode_uint(&w, 1);

// x coordinate
cbor_encode_int(&w, -2);
cbor_encode_bytes(&w, pub_x, 32);

// y coordinate
cbor_encode_int(&w, -3);
cbor_encode_bytes(&w, pub_y, 32);
```

```cpp
// components/mod_fido2/src/ctap2.cpp:3095-3115
// EdDSA key
cbor_encode_int(w, 1);   // kty
cbor_encode_int(w, 1);   // OKP
cbor_encode_int(w, 3);   // alg
cbor_encode_int(w, -8);  // EdDSA
cbor_encode_int(w, -1);  // crv
cbor_encode_int(w, 6);   // Ed25519

// ES256 key
cbor_encode_int(w, 1);   // kty
cbor_encode_int(w, 2);   // EC2
cbor_encode_int(w, 3);   // alg
cbor_encode_int(w, -7);  // ES256
cbor_encode_int(w, -1);  // crv
cbor_encode_int(w, 1);   // P-256
```

## Recommended Fix
1. **Add COSE key constants** to `components/mod_fido2/include/mod_fido2/ctap2.h`:

    ```cpp
    // ============================================================================
    // COSE Key Constants (RFC 8152 / RFC 8037)
    // ============================================================================
    
    // COSE Key Type (kty) values
    #define COSE_KEY_TYPE_OKP       1   // Octet Key Pair
    #define COSE_KEY_TYPE_EC2       2   // Elliptic Curve 2D coordinates
    #define COSE_KEY_TYPE_SYMMETRIC 4   // Symmetric keys
    
    // COSE Elliptic Curves (crv) values
    #define COSE_CRV_P256           1   // NIST P-256
    #define COSE_CRV_P384           2   // NIST P-384
    #define COSE_CRV_P521           3   // NIST P-521
    #define COSE_CRV_X25519         4   // X25519 for ECDH
    #define COSE_CRV_X448           5   // X448 for ECDH
    #define COSE_CRV_ED25519        6   // Ed25519 for EdDSA
    #define COSE_CRV_ED448          7   // Ed448 for EdDSA
    
    // COSE Key Common Parameters (negative numbers)
    #define COSE_KEY_LABEL_KTY      1   // Key type
    #define COSE_KEY_LABEL_K        4   // Symmetric key value
    #define COSE_KEY_LABEL_OPS      5   // Operations list
    #define COSE_KEY_LABEL_ALG      3   // Algorithm
    #define COSE_KEY_LABEL_CRV      -1  // Curve
    #define COSE_KEY_LABEL_X        -2  // X coordinate
    #define COSE_KEY_LABEL_Y        -3  // Y coordinate
    #define COSE_KEY_LABEL_D        -4  // Private key (for encoding)
    
    // COSE Algorithm values for ECDH-ES (not in ctap2.h yet)
    #define COSE_ALG_ECDH_ES        -25     // ECDH-ES
    #define COSE_ALG_ECDH_ES_HKDF_256 -26   // ECDH-ES+HKDF-256
    #define COSE_ALG_ECDH_ES_HKDF_384 -27   // ECDH-ES+HKDF-384
    ```

2. **Refactor usage** to use constants:

    ```cpp
    // Before:
    // kty: EC2 (2)
    cbor_encode_uint(&w, 1);
    cbor_encode_uint(&w, 2);
    
    // alg: ECDH-ES+HKDF-256 (-25)
    cbor_encode_uint(&w, 3);
    cbor_encode_int(&w, -25);
    
    // crv: P-256 (1)
    cbor_encode_int(&w, -1);
    cbor_encode_uint(&w, 1);
    
    // x coordinate
    cbor_encode_int(&w, -2);
    cbor_encode_bytes(&w, pub_x, 32);
    
    // y coordinate
    cbor_encode_int(&w, -3);
    cbor_encode_bytes(&w, pub_y, 32);
    
    // After:
    // kty: EC2
    cbor_encode_uint(&w, COSE_KEY_LABEL_KTY);
    cbor_encode_uint(&w, COSE_KEY_TYPE_EC2);
    
    // alg: ECDH-ES+HKDF-256
    cbor_encode_uint(&w, COSE_KEY_LABEL_ALG);
    cbor_encode_int(&w, COSE_ALG_ECDH_ES_HKDF_256);
    
    // crv: P-256
    cbor_encode_int(&w, COSE_KEY_LABEL_CRV);
    cbor_encode_uint(&w, COSE_CRV_P256);
    
    // x coordinate
    cbor_encode_int(&w, COSE_KEY_LABEL_X);
    cbor_encode_bytes(&w, pub_x, 32);
    
    // y coordinate
    cbor_encode_int(&w, COSE_KEY_LABEL_Y);
    cbor_encode_bytes(&w, pub_y, 32);
    ```

3. **Add COSE specification reference** in header:

    ```cpp
    /**
     * \brief COSE key constants
     * 
     * Based on:
     * - RFC 8152: COSE (CBOR Object Signing and Encryption)
     *   https://datatracker.ietf.org/doc/html/rfc8152
     * - RFC 8037: COSE EdDSA (Ed25519/Ed448)
     *   https://datatracker.ietf.org/doc/html/rfc8037
     * - COSE Registry: https://www.iana.org/assignments/cose/cose.xhtml
     */
    ```

## References
- [COSE (RFC 8152)](https://datatracker.ietf.org/doc/html/rfc8152)
- [COSE EdDSA (RFC 8037)](https://datatracker.ietf.org/doc/html/rfc8037)
- [COSE IANA Registry](https://www.iana.org/assignments/cose/cose.xhtml)
- [CTAP2.1 Specification](https://fidoalliance.org/specs/fido-v2.1-rd-20191217/fido-client-to-authenticator-protocol-v2.1-rd-20191217.html)

</content>