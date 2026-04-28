---
title: "[LOW] FIDO2 CTAP2 pinUvAuthProtocol lacks validation"
severity: LOW
domain: api-design/request-validation
lens: fido2-protocol-validation
labels:
  - "audit:api-design/request-validation"
---

## Summary
The FIDO2 CTAP2 protocol parsing in `components/mod_fido2/src/ctap2.cpp` accepts the `pinUvAuthProtocol` parameter without validating that it contains a supported protocol version. The code silently defaults to protocol 1 behavior if protocol 2 is not explicitly matched, which may lead to unexpected authentication behavior.

**File**: `components/mod_fido2/src/ctap2.cpp`  
**Lines**: 896-899, 1510-1514  
**Functions**: `parse_make_credential_params()`, `ga_parse_params()`

## Impact
- **Protocol ambiguity**: Invalid protocol values (e.g., 3, 4, 100) are accepted and silently treated as protocol 1
- **Future compatibility**: If new protocol versions are added, old code will incorrectly handle them
- **Debugging difficulty**: Users specifying an unsupported protocol won't get an error message

## Evidence
From `components/mod_fido2/src/ctap2.cpp:896-899` (makeCredential parsing):

```cpp
case 0x09: {  // pinUvAuthProtocol
    uint64_t proto;
    if (cbor_read_uint(&r, &proto)) {
        p->pin_uv_auth_protocol = (uint8_t)proto;  // No validation!
    }
    break;
}
```

From `components/mod_fido2/src/ctap2.cpp:940` (usage in verification):

```cpp
// Protocol 2 uses first 32 bytes of HMAC
size_t compare_len = (p->pin_uv_auth_protocol == 2) ? 32 : 16;
```

The code only checks `if (p->pin_uv_auth_protocol == 2)` to determine HMAC length, treating all other values (including invalid ones like 0, 3, 100) as protocol 1 (16-byte HMAC).

CTAP2 spec defines:
- Protocol 1: Legacy PIN protocol (16-byte HMAC)
- Protocol 2: Modern PIN protocol (32-byte HMAC)

## Recommended Fix
Add explicit validation for the `pinUvAuthProtocol` parameter:

```cpp
case 0x09: {  // pinUvAuthProtocol
    uint64_t proto;
    if (cbor_read_uint(&r, &proto)) {
        // Validate supported protocols (1 and 2 per CTAP2 spec)
        if (proto == 1 || proto == 2) {
            p->pin_uv_auth_protocol = (uint8_t)proto;
        } else {
            return CTAP2_ERR_PIN_AUTH_INVALID;  // or CTAP2_ERR_INVALID_OPTION
        }
    }
    break;
}
```

Alternatively, provide a clearer error for unsupported protocols:

```cpp
case 0x09: {  // pinUvAuthProtocol
    uint64_t proto;
    if (cbor_read_uint(&r, &proto)) {
        if (proto == 2) {
            p->pin_uv_auth_protocol = 2;
        } else if (proto == 1) {
            p->pin_uv_auth_protocol = 1;
        } else {
            // Unsupported protocol version
            return CTAP2_ERR_UNSUPPORTED_OPTION;
        }
    }
    break;
}
```

## References
- CTAP2.1 Specification: [PIN Protocol 2](https://fidoalliance.org/specs/fido2/fido-client-to-authenticator-protocol-v2.1-rd-20191217.html#pin-protocol-2)
- Similar validation pattern in `ctap2_make_credential()` for options (line 1227-1236)
- FIDO2 header: `components/mod_fido2/include/mod_fido2/ctap2.h` (protocol constants)
