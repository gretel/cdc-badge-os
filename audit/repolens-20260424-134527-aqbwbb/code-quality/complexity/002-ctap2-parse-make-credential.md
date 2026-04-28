---
title: "[HIGH] High cyclomatic complexity in parse_make_credential_params() function"
severity: HIGH
domain: Code Quality
lens: cyclomatic-complexity
labels:
  - "audit:code-quality/complexity"
---

## Summary

The `parse_make_credential_params()` function in `components/mod_fido2/src/ctap2.cpp` (lines 849-920) has high cyclomatic complexity due to a large switch statement with 9 cases, nested parsing function calls, and multiple validation checks.

**Estimated Cyclomatic Complexity: ~15** (threshold is 10)

## Impact

**Maintenance Burden:**
- Adding new CTAP2 parameters requires understanding all existing cases
- CBOR parsing logic is tightly coupled with validation
- Error handling paths are scattered across multiple switch cases

**Readability:**
- Function mixes parameter parsing with validation
- Switch statement has 9 cases plus default
- Some cases call helper functions, others inline parsing

## Evidence

**File:** `components/mod_fido2/src/ctap2.cpp:849-920`

**Code excerpt:**
```cpp
static uint8_t parse_make_credential_params(const uint8_t *data, uint16_t data_len,
                                            MakeCredentialParams *p) {
    cbor_reader_t r;
    cbor_reader_init(&r, data, data_len);
    p->clear();

    int map_count = cbor_read_map(&r);
    if (map_count < 0) {
        return CTAP2_ERR_INVALID_CBOR;
    }

    for (int i = 0; i < map_count; i++) {
        uint64_t key;
        if (!cbor_read_uint(&r, &key)) {
            return CTAP2_ERR_INVALID_CBOR;
        }

        switch (key) {
            case 0x01: {  // clientDataHash
                size_t len;
                if (!cbor_read_bytes(&r, p->client_data_hash, 32, &len) || len != 32) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                p->has_client_data = true;
                break;
            }
            case 0x02:  // rp
                if (!parse_rp_map(&r, p)) return CTAP2_ERR_INVALID_CBOR;
                break;
            case 0x03:  // user
                if (!parse_user_map(&r, p)) return CTAP2_ERR_INVALID_CBOR;
                break;
            case 0x04:  // pubKeyCredParams
                if (!parse_pubkey_cred_params(&r, p)) return CTAP2_ERR_INVALID_CBOR;
                break;
            case 0x06:  // extensions
                if (!parse_extensions_map(&r, p)) return CTAP2_ERR_INVALID_CBOR;
                break;
            case 0x07:  // options
                if (!parse_options_map(&r, p)) return CTAP2_ERR_INVALID_CBOR;
                break;
            case 0x08:  // pinUvAuthParam
                cbor_read_bytes(&r, p->pin_uv_auth_param, sizeof(p->pin_uv_auth_param),
                               &p->pin_uv_auth_param_len);
                break;
            case 0x09: {  // pinUvAuthProtocol
                uint64_t proto;
                if (cbor_read_uint(&r, &proto)) {
                    p->pin_uv_auth_protocol = (uint8_t)proto;
                }
                break;
            }
            default:
                cbor_skip_item(&r);
                break;
        }
    }

    // Validate required parameters
    if (!p->has_client_data || !p->has_rp || !p->has_user || !p->has_alg) {
        return CTAP2_ERR_MISSING_PARAMETER;
    }

    return CTAP2_OK;
}
```

**Branching count:**
- Line 860: `if (map_count < 0)` (+1)
- Line 864: `if (!cbor_read_uint(&r, &key))` (+1)
- Line 868: `switch (key)` with 9 cases (+8)
- Line 870: `if (!cbor_read_bytes(...) || len != 32)` (+2)
- Line 903: `if (cbor_read_uint(&r, &proto))` (+1)
- Line 914: `if (!p->has_client_data || !p->has_rp || !p->has_user || !p->has_alg)` (+3)

Total: ~16 independent paths

## Recommended Fix

**Split into focused functions:**

1. Create a parameter parsing struct for cleaner state:
```cpp
struct CborFieldParser {
    uint64_t key;
    uint8_t (*parse)(cbor_reader_t*, MakeCredentialParams*);
};
```

2. Extract validation to separate function:
```cpp
static uint8_t validateMakeCredentialParams(const MakeCredentialParams *p) {
    if (!p->has_client_data) return CTAP2_ERR_MISSING_PARAMETER;
    if (!p->has_rp) return CTAP2_ERR_MISSING_PARAMETER;
    if (!p->has_user) return CTAP2_ERR_MISSING_PARAMETER;
    if (!p->has_alg) return CTAP2_ERR_MISSING_PARAMETER;
    return CTAP2_OK;
}
```

3. Use a lookup table for switch reduction:
```cpp
static uint8_t parseSingleField(cbor_reader_t *r, uint64_t key, MakeCredentialParams *p) {
    switch (key) {
        case 0x01: return parseClientDataHash(r, p);
        case 0x02: return parseRpMap(r, p);
        case 0x03: return parseUserMap(r, p);
        case 0x04: return parsePubKeyCredParams(r, p);
        case 0x06: return parseExtensionsMap(r, p);
        case 0x07: return parseOptionsMap(r, p);
        case 0x08: return parsePinUvAuthParam(r, p);
        case 0x09: return parsePinUvAuthProtocol(r, p);
        default: return parseSkipItem(r);
    }
}

static uint8_t parse_make_credential_params(const uint8_t *data, uint16_t data_len,
                                            MakeCredentialParams *p) {
    cbor_reader_t r;
    cbor_reader_init(&r, data, data_len);
    p->clear();

    int map_count = cbor_read_map(&r);
    if (map_count < 0) return CTAP2_ERR_INVALID_CBOR;

    for (int i = 0; i < map_count; i++) {
        uint64_t key;
        if (!cbor_read_uint(&r, &key)) return CTAP2_ERR_INVALID_CBOR;
        
        uint8_t status = parseSingleField(&r, key, p);
        if (status != CTAP2_OK) return status;
    }

    return validateMakeCredentialParams(p);
}
```

**Expected result:**
- Main function reduced to ~30 lines
- Each parser function has complexity < 5
- Validation logic isolated and testable
- Easier to add new parameters

## References

- [Cyclomatic Complexity - Wikipedia](https://en.wikipedia.org/wiki/Cyclomatic_complexity)
- [Strategy Pattern for Switch Reduction](https://refactoring.com/catalog/replaceConditionalsWithPolymorphism.html)
- [CTAP2 Specification](https://fidoalliance.org/specs/fido-v2.0-rd-20180130/fido-client-to-authenticator-protocol-v2.0-rd-20180130.html)
