---
title: "[MEDIUM] Hardcoded CBOR map keys in FIDO2 CTAP2 protocol"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "magic-values"
---

## Summary
CBOR map keys (numeric identifiers) are used directly in the FIDO2 CTAP2 protocol implementation without named constants. These keys represent standard CTAP2 parameter IDs but are scattered as magic numbers throughout the encoding logic.

**Files affected:**
- `components/mod_fido2/src/ctap2.cpp:549-600` - `authenticatorGetInfo` response encoding
- `components/mod_fido2/src/ctap2.cpp:867-894` - `makeCredential` parameter parsing
- `components/mod_fido2/src/ctap2.cpp:1469-1508` - `getAssertion` parameter parsing
- `components/mod_fido2/src/ctap2.cpp:2387-2750` - PIN protocol parameter parsing

**Magic values found:**
- `0x01` - "versions" (getInfo), "fmt" (makeCredential), "rpId" (getAssertion), "subCommand" (clientPIN)
- `0x02` - "extensions" (getInfo), "authData" (makeCredential), "clientDataHash" (getAssertion)
- `0x03` - "aaguid" (getInfo), "attStmt" (makeCredential), "user" (makeCredential), "allowList" (getAssertion)
- `0x04` - "options" (getInfo), "pubKeyCredParams" (makeCredential), "extensions" (getAssertion)
- `0x05` - "maxMsgSize" (getInfo), "options" (getAssertion)
- `0x06` - "pinUvAuthProtocols" (getInfo), "pinUvAuthParam" (getAssertion), "pinHashEnc" (clientPIN)
- `0x07` - "maxCredentialCountInList" (getInfo), "options" (getAssertion), "pinUvAuthProtocol" (clientPIN)
- `0x08` - "pinUvAuthParam" (makeCredential)
- `0x09` - "pinUvAuthProtocol" (makeCredential), "pinUvAuthProtocol" (clientPIN)
- `0x0A` - "permissions" (clientPIN)
- `0x0B` - "rpId" (clientPIN)

## Impact
- **Standards compliance**: CTAP2 spec defines these IDs, but the connection is not explicit in code
- **Maintainability**: Adding new CTAP2 parameters requires searching through multiple locations
- **Readability**: `cbor_encode_uint(&w, 0x01)` doesn't convey "this is the 'versions' key"
- **Error-prone**: Easy to use wrong key ID when adding new features
- **Documentation**: No link to CTAP2 specification in the code

## Evidence
```cpp
// components/mod_fido2/src/ctap2.cpp:549-590
// 0x01: versions - TEST: add FIDO_2_1
cbor_encode_uint(&w, 0x01);
cbor_encode_array(&w, 3);
cbor_encode_text(&w, "FIDO_2_0");
cbor_encode_text(&w, "FIDO_2_1");

// 0x02: extensions
cbor_encode_uint(&w, 0x02);
cbor_encode_array(&w, 3);
cbor_encode_text(&w, "appid");

// 0x03: aaguid
cbor_encode_uint(&w, 0x03);
cbor_encode_bytes(&w, AAGUID, 16);

// 0x04: options
cbor_encode_uint(&w, 0x04);
cbor_encode_map(&w, 7);
cbor_encode_text(&w, "rk");
cbor_encode_bool(&w, true);

// 0x05: maxMsgSize
cbor_encode_uint(&w, 0x05);
cbor_encode_uint(&w, 1200);

// 0x06: pinUvAuthProtocols
cbor_encode_uint(&w, 0x06);

// 0x07: maxCredentialCountInList
cbor_encode_uint(&w, 0x07);
```

```cpp
// components/mod_fido2/src/ctap2.cpp:867-894 (makeCredential parsing)
switch (key) {
    case 0x01: {  // clientDataHash
        // ...
    }
    case 0x02:  // rp
        // ...
    case 0x03:  // user
        // ...
    case 0x04:  // pubKeyCredParams
        // ...
    case 0x06:  // extensions
        // ...
    case 0x07:  // options
        // ...
    case 0x08:  // pinUvAuthParam
        // ...
}
```

## Recommended Fix
1. **Create CBOR key constants** in `components/mod_fido2/include/mod_fido2/ctap2_constants.h`:
    ```cpp
    #pragma once
    #include <cstdint>
    
    // CTAP2 Command IDs (CTAP specification Table 13)
    static constexpr uint8_t CTAP2_MAKE_CREDENTIAL = 0x01;
    static constexpr uint8_t CTAP2_GET_ASSERTION = 0x02;
    static constexpr uint8_t CTAP2_GET_INFO = 0x04;
    static constexpr uint8_t CTAP2_CLIENT_PIN = 0x06;
    static constexpr uint8_t CTAP2_RESET = 0x07;
    static constexpr uint8_t CTAP2_GET_NEXT_ASSERTION = 0x08;
    static constexpr uint8_t CTAP2_CRED_MGMT = 0x0A;
    static constexpr uint8_t CTAP2_SELECTION = 0x0B;
    
    // CTAP2 getInfo response keys (CTAP specification Section 6.1)
    static constexpr uint8_t CTAP2_INFO_VERSIONS = 0x01;
    static constexpr uint8_t CTAP2_INFO_EXTENSIONS = 0x02;
    static constexpr uint8_t CTAP2_INFO_AAGUID = 0x03;
    static constexpr uint8_t CTAP2_INFO_OPTIONS = 0x04;
    static constexpr uint8_t CTAP2_INFO_MAX_MSG_SIZE = 0x05;
    static constexpr uint8_t CTAP2_INFO_PIN_UV_AUTH_PROTOCOLS = 0x06;
    static constexpr uint8_t CTAP2_INFO_MAX_CRED_COUNT_IN_LIST = 0x07;
    static constexpr uint8_t CTAP2_INFO_MAX_CRED_ID_SIZE = 0x08;
    static constexpr uint8_t CTAP2_INFO_TRANSPORTS = 0x09;
    static constexpr uint8_t CTAP2_INFO_ALGORITHMS = 0x0A;
    static constexpr uint8_t CTAP2_INFO_MAX_LARGE_BLOB = 0x0B;
    static constexpr uint8_t CTAP2_INFO_FORCE_PRESERVE = 0x0C;
    static constexpr uint8_t CTAP2_INFO_MAX_CREDENTIALS_IN_SET = 0x0D;
    static constexpr uint8_t CTAP2_INFO_MAX_SETS = 0x0E;
    static constexpr uint8_t CTAP2_INFO_MIN_PIN_LENGTH = 0x0F;
    static constexpr uint8_t CTAP2_INFO_FIRMWARE_VERSION = 0x10;
    static constexpr uint8_t CTAP2_INFO_MAX_CRED_ATTRIBUTES = 0x11;
    static constexpr uint8_t CTAP2_INFO_PRESENCE_REQUIRED = 0x12;
    static constexpr uint8_t CTAP2_INFO_LARGE_BLOB_SUPPORT = 0x13;
    
    // CTAP2 makeCredential parameter keys (CTAP specification Section 6.2)
    static constexpr uint8_t CTAP2_MC_RP = 0x01;
    static constexpr uint8_t CTAP2_MC_USER = 0x02;
    static constexpr uint8_t CTAP2_MC_PUB_KEY_CRED_PARAMS = 0x03;
    static constexpr uint8_t CTAP2_MC_EXCLUDE_LIST = 0x04;
    static constexpr uint8_t CTAP2_MC_EXTENSIONS = 0x05;
    static constexpr uint8_t CTAP2_MC_OPTIONS = 0x06;
    static constexpr uint8_t CTAP2_MC_PIN_UV_AUTH_PARAM = 0x07;
    static constexpr uint8_t CTAP2_MC_PIN_UV_AUTH_PROTOCOL = 0x08;
    static constexpr uint8_t CTAP2_MC_PREV_LARGE_BLOB = 0x09;
    static constexpr uint8_t CTAP2_MC_CRED_PROTECT = 0x0A;
    static constexpr uint8_t CTAP2_MC_ENTER_SELECT = 0x0B;
    static constexpr uint8_t CTAP2_MC_ATTESTATION_FORMAT = 0x0C;
    
    // CTAP2 getAssertion parameter keys (CTAP specification Section 6.3)
    static constexpr uint8_t CTAP2_GA_RP_ID = 0x01;
    static constexpr uint8_t CTAP2_GA_CLIENT_DATA_HASH = 0x02;
    static constexpr uint8_t CTAP2_GA_ALLOW_LIST = 0x03;
    static constexpr uint8_t CTAP2_GA_EXTENSIONS = 0x04;
    static constexpr uint8_t CTAP2_GA_OPTIONS = 0x05;
    static constexpr uint8_t CTAP2_GA_PIN_UV_AUTH_PARAM = 0x06;
    static constexpr uint8_t CTAP2_GA_PIN_UV_AUTH_PROTOCOL = 0x07;
    static constexpr uint8_t CTAP2_GA_PREV_LARGE_BLOB = 0x08;
    static constexpr uint8_t CTAP2_GA_ASSERTION_SIGNATURES = 0x09;
    static constexpr uint8_t CTAP2_GA_MAX_ASSERTION_SETS = 0x0A;
    
    // CTAP2 clientPIN subCommand parameter keys (CTAP specification Section 6.5)
    static constexpr uint8_t CTAP2_PIN_SUBCOMMAND = 0x01;
    static constexpr uint8_t CTAP2_PIN_SUBCOMMAND_PARAMS = 0x02;
    
    // CTAP2 clientPIN subCommand IDs
    static constexpr uint8_t CTAP2_PIN_GET_RETRIES = 0x01;
    static constexpr uint8_t CTAP2_PIN_GET_KEY_AGREEMENT = 0x02;
    static constexpr uint8_t CTAP2_PIN_SET_PIN = 0x03;
    static constexpr uint8_t CTAP2_PIN_CHANGE_PIN = 0x04;
    static constexpr uint8_t CTAP2_PIN_GET_TOKEN = 0x05;
    static constexpr uint8_t CTAP2_PIN_GET_TOKEN_V2 = 0x06;
    static constexpr uint8_t CTAP2_PIN_GET_UV_TOKEN = 0x09;
    ```

2. **Refactor usage** to use constants:
    ```cpp
    // Before:
    cbor_encode_uint(&w, 0x01);  // versions
    cbor_encode_text(&w, "FIDO_2_0");
    
    // After:
    cbor_encode_uint(&w, CTAP2_INFO_VERSIONS);
    cbor_encode_text(&w, "FIDO_2_0");
    
    // Before:
    case 0x01: {  // clientDataHash
        // ...
    }
    
    // After:
    case CTAP2_MC_CLIENT_DATA_HASH: {
        // ...
    }
    ```

3. **Add CTAP2 specification reference** in header:
    ```cpp
    /**
     * \brief CTAP2 protocol constants
     * 
     * Based on:
     * - CTAP2.1 Specification: https://fidoalliance.org/specs/fido-v2.1-rd-20191217/fido-client-to-authenticator-protocol-v2.1-rd-20191217.html
     * - CTAP2.0 Specification: https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html
     */
    ```

## References
- [CTAP 2.1 Specification](https://fidoalliance.org/specs/fido-v2.1-rd-20191217/fido-client-to-authenticator-protocol-v2.1-rd-20191217.html)
- [CTAP 2.0 Specification](https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html)
- [CTAP2 getInfo response format](https://fidoalliance.org/specs/fido-v2.1-rd-20191217/fido-client-to-authenticator-protocol-v2.1-rd-20191217.html#authenticatorgetinfo)
- [CTAP2 makeCredential parameters](https://fidoalliance.org/specs/fido-v2.1-rd-20191217/fido-client-to-authenticator-protocol-v2.1-rd-20191217.html#authenticatormakecredential)
