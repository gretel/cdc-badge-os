---
title: "[MEDIUM] Hardcoded CBOR string keys in FIDO2 protocol"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "magic-values"
---

## Summary
CBOR string keys used in CTAP2 protocol encoding are hardcoded as string literals throughout the FIDO2 implementation. These keys represent standard CTAP2 parameter names but are scattered as magic strings without centralized definitions.

**Files affected:**
- `components/mod_fido2/src/ctap2.cpp:230-622` - Multiple CTAP2 command encodings

**Magic strings found:**
- `"credProtect"` - Credential protection extension
- `"appid"` - Application ID extension
- `"none"` / `"packed"` - Attestation format strings
- `"alg"` / `"sig"` / `"x5c"` - Attestation statement keys
- `"FIDO_2_0"` / `"FIDO_2_1"` / `"U2F_V2"` - Protocol version strings
- `"appidExclude"` - AppID exclude extension
- `"rk"` / `"up"` / `"uv"` / `"plat"` - Options short names
- `"credMgmt"` / `"clientPin"` / `"pinUvAuthToken"` - Options full names
- `"type"` / `"public-key"` - Credential type strings
- `"id"` / `"name"` - Credential/user object keys

## Impact
- **Standards compliance**: These are standard CTAP2 strings, but the connection is not explicit in code
- **Maintainability**: Adding new CTAP2 parameters requires searching through multiple locations
- **Readability**: `cbor_encode_text(&w, "alg")` doesn't convey context
- **Error-prone**: Easy to typo a string key (e.g., `"al"` vs `"alg"`)
- **Consistency**: String keys are repeated many times without DRY principle

## Evidence
```cpp
// components/mod_fido2/src/ctap2.cpp:552-582
// Versions
cbor_encode_text(&w, "FIDO_2_0");
cbor_encode_text(&w, "FIDO_2_1");
cbor_encode_text(&w, "U2F_V2");

// Extensions
cbor_encode_text(&w, "appid");          // 5 chars
cbor_encode_text(&w, "credProtect");    // 11 chars
cbor_encode_text(&w, "appidExclude");   // 12 chars

// Options
cbor_encode_text(&w, "rk");              // 2 chars
cbor_encode_text(&w, "up");              // 2 chars
cbor_encode_text(&w, "uv");              // 2 chars
cbor_encode_text(&w, "plat");            // 4 chars
cbor_encode_text(&w, "credMgmt");        // 8 chars
cbor_encode_text(&w, "clientPin");       // 9 chars
cbor_encode_text(&w, "pinUvAuthToken");  // 14 chars

// Algorithms
cbor_encode_text(&w, "alg");
cbor_encode_text(&w, "type");
cbor_encode_text(&w, "public-key");
```

```cpp
// components/mod_fido2/src/ctap2.cpp:326-347 (attestation)
switch (attestation_format) {
    case FIDO2_ATTESTATION_NONE:
        cbor_encode_text(&w, "none");
        break;
    case FIDO2_ATTESTATION_PACKED:
        cbor_encode_text(&w, "packed");
        break;
}

cbor_encode_text(&w, "alg");
cbor_encode_text(&w, "sig");
cbor_encode_text(&w, "x5c");
```

```cpp
// components/mod_fido2/src/ctap2.cpp:1714-1717 (credential)
cbor_encode_text(&w, "id");
cbor_encode_text(&w, "type");
cbor_encode_text(&w, "public-key");
```

## Recommended Fix
1. **Create CBOR string constants** in `components/mod_fido2/include/mod_fido2/ctap2_constants.h`:

    ```cpp
    #pragma once
    #include <string.h>
    
    // ============================================================================
    // CTAP2 CBOR String Keys (CTAP specification Section 6)
    // ============================================================================
    
    // CTAP2 getInfo version strings
    static constexpr char CTAP2_VER_FIDO_2_0[] = "FIDO_2_0";
    static constexpr char CTAP2_VER_FIDO_2_1[] = "FIDO_2_1";
    static constexpr char CTAP2_VER_U2F_V2[] = "U2F_V2";
    
    // CTAP2 getInfo extension names
    static constexpr char CTAP2_EXT_APPID[] = "appid";
    static constexpr char CTAP2_EXT_CRED_PROTECT[] = "credProtect";
    static constexpr char CTAP2_EXT_APPID_EXCLUDE[] = "appidExclude";
    
    // CTAP2 getInfo option names
    static constexpr char CTAP2_OPT_RK[] = "rk";              // Resident key
    static constexpr char CTAP2_OPT_UP[] = "up";              // User presence
    static constexpr char CTAP2_OPT_UV[] = "uv";              // User verification
    static constexpr char CTAP2_OPT_PLAT[] = "plat";          // Platform device
    static constexpr char CTAP2_OPT_CRED_MGMT[] = "credMgmt"; // Credential management
    static constexpr char CTAP2_OPT_CLIENT_PIN[] = "clientPin";
    static constexpr char CTAP2_OPT_PIN_UV_AUTH_TOKEN[] = "pinUvAuthToken";
    static constexpr char CTAP2_OPT_LARGE_BLOB[] = "largeBlob";
    
    // CTAP2 makeCredential/getAssertion parameter names
    static constexpr char CTAP2_KEY_ALG[] = "alg";
    static constexpr char CTAP2_KEY_TYPE[] = "type";
    static constexpr char CTAP2_KEY_ID[] = "id";
    static constexpr char CTAP2_KEY_NAME[] = "name";
    static constexpr char CTAP2_KEY_SIG[] = "sig";
    static constexpr char CTAP2_KEY_X5C[] = "x5c";
    static constexpr char CTAP2_KEY_CRV[] = "crv";
    static constexpr char CTAP2_KEY_X[] = "x";
    static constexpr char CTAP2_KEY_Y[] = "y";
    static constexpr char CTAP2_KEY_D[] = "d";
    
    // CTAP2 credential type
    static constexpr char CTAP2_TYPE_PUBLIC_KEY[] = "public-key";
    
    // CTAP2 attestation format strings
    static constexpr char CTAP2_FMT_NONE[] = "none";
    static constexpr char CTAP2_FMT_PACKED[] = "packed";
    static constexpr char CTAP2_FMT_FIDO_U2F[] = "fido-u2f";
    static constexpr char CTAP2_FMT_BASIC_ATTESTATION[] = "basic";
    static constexpr char CTAP2_FMT_ATTESTATION_NULL[] = "attestation-null";
    
    // Helper macro for encoding string keys
    #define CTAP2_CBOR_TEXT(str) cbor_encode_text(&w, str)
    ```

2. **Refactor usage** to use constants:

    ```cpp
    // Before:
    cbor_encode_text(&w, "FIDO_2_0");
    cbor_encode_text(&w, "FIDO_2_1");
    cbor_encode_text(&w, "U2F_V2");
    
    // After:
    cbor_encode_text(&w, CTAP2_VER_FIDO_2_0);
    cbor_encode_text(&w, CTAP2_VER_FIDO_2_1);
    cbor_encode_text(&w, CTAP2_VER_U2F_V2);
    
    // Before:
    cbor_encode_text(&w, "rk");
    cbor_encode_text(&w, "up");
    cbor_encode_text(&w, "uv");
    
    // After:
    cbor_encode_text(&w, CTAP2_OPT_RK);
    cbor_encode_text(&w, CTAP2_OPT_UP);
    cbor_encode_text(&w, CTAP2_OPT_UV);
    
    // Before:
    cbor_encode_text(&w, "alg");
    cbor_encode_text(&w, "type");
    cbor_encode_text(&w, "public-key");
    
    // After:
    cbor_encode_text(&w, CTAP2_KEY_ALG);
    cbor_encode_text(&w, CTAP2_KEY_TYPE);
    cbor_encode_text(&w, CTAP2_TYPE_PUBLIC_KEY);
    ```

3. **Add CTAP2 specification reference** in header:

    ```cpp
    /**
     * \brief CTAP2 CBOR string keys
     * 
     * Based on:
     * - CTAP 2.1 Specification: https://fidoalliance.org/specs/fido-v2.1-rd-20191217/fido-client-to-authenticator-protocol-v2.1-rd-20191217.html
     * - CTAP 2.0 Specification: https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html
     * - WebAuthn Level 1: https://www.w3.org/TR/webauthn-1/
     * - WebAuthn Level 2: https://www.w3.org/TR/webauthn-2/
     */
    ```

## References
- [CTAP 2.1 Specification](https://fidoalliance.org/specs/fido-v2.1-rd-20191217/fido-client-to-authenticator-protocol-v2.1-rd-20191217.html)
- [CTAP 2.0 Specification](https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html)
- [WebAuthn Level 2](https://www.w3.org/TR/webauthn-2/)
- [CTAP2 getInfo response format](https://fidoalliance.org/specs/fido-v2.1-rd-20191217/fido-client-to-authenticator-protocol-v2.1-rd-20191217.html#dom-authenticatorgetinforesponse)

</content>