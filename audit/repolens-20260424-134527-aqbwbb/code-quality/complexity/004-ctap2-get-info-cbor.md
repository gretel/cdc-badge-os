---
title: "[MEDIUM] Long function with complex CBOR encoding in ctap2_get_info()"
severity: MEDIUM
domain: Code Quality
lens: cyclomatic-complexity
labels:
  - "audit:code-quality/complexity"
---

## Summary

The `ctap2_get_info()` function in `components/mod_fido2/src/ctap2.cpp` (lines 534-646) is a long function (~112 lines) that builds a complex CBOR response with 10 map entries. While the logic is straightforward, the function is hard to read and maintain due to its length and nested CBOR encoding.

**Estimated Cyclomatic Complexity: ~8** (below threshold but long function)
**Function Length: ~112 lines** (threshold is 50)

## Impact

**Readability:**
- Function requires scrolling to see complete structure
- Hard to verify all 10 CBOR map entries are correct
- Debugging requires parsing through many nested calls

**Maintenance:**
- Adding new info fields requires understanding CBOR structure
- Changes to one field may affect others due to shared state
- Hard to unit test specific fields

## Evidence

**File:** `components/mod_fido2/src/ctap2.cpp:534-646`

**Code excerpt:**
```cpp
uint8_t ctap2_get_info(uint8_t *response, uint16_t *response_len) {
    cbor_writer_t w;
    cbor_writer_init(&w, response + 1, *response_len - 1);

    // Response is a map (10 items)
    cbor_encode_map(&w, 10);

    // 0x01: versions - TEST: add FIDO_2_1
    cbor_encode_uint(&w, 0x01);
    cbor_encode_array(&w, 3);
    cbor_encode_text(&w, "FIDO_2_0");
    cbor_encode_text(&w, "FIDO_2_1");
    cbor_encode_text(&w, "U2F_V2");

    // 0x02: extensions - sorted by length for CBOR canonical form
    cbor_encode_uint(&w, 0x02);
    cbor_encode_array(&w, 3);
    cbor_encode_text(&w, "appid");
    cbor_encode_text(&w, "credProtect");
    cbor_encode_text(&w, "appidExclude");

    // 0x03: aaguid
    cbor_encode_uint(&w, 0x03);
    cbor_encode_bytes(&w, AAGUID, 16);

    // 0x04: options - SORTED BY KEY LENGTH (CBOR canonical form!)
    cbor_encode_uint(&w, 0x04);
    cbor_encode_map(&w, 7);
    cbor_encode_text(&w, "rk");
    cbor_encode_bool(&w, true);
    cbor_encode_text(&w, "up");
    cbor_encode_bool(&w, true);
    // ... 5 more options

    // 0x05: maxMsgSize
    cbor_encode_uint(&w, 0x05);
    cbor_encode_uint(&w, 1200);

    // 0x06: pinUvAuthProtocols
    cbor_encode_uint(&w, 0x06);
    cbor_encode_array(&w, 1);
    cbor_encode_uint(&w, 2);

    // 0x07-0x0A: more fields...

    if (cbor_writer_error(&w)) {
        response[0] = CTAP2_ERR_OTHER;
        *response_len = 1;
        return CTAP2_ERR_OTHER;
    }

    response[0] = CTAP2_OK;
    *response_len = 1 + cbor_writer_length(&w);

#if CTAP2_DEBUG
    LOG_I("CTAP2", "getInfo response len=%u", *response_len);
    for (uint16_t offset = 0; offset < *response_len; offset += 16) {
        // Debug dump loop
    }
#endif

    return CTAP2_OK;
}
```

**Characteristics:**
- 112 lines total
- 10 CBOR map entries
- Multiple nested array/map encodings
- Debug code at end adds complexity

## Recommended Fix

**Extract CBOR encoding into helper functions:**

1. Create field encoder helpers:
```cpp
static void encodeVersions(cbor_writer_t *w) {
    cbor_encode_uint(w, 0x01);
    cbor_encode_array(w, 3);
    cbor_encode_text(w, "FIDO_2_0");
    cbor_encode_text(w, "FIDO_2_1");
    cbor_encode_text(w, "U2F_V2");
}

static void encodeExtensions(cbor_writer_t *w) {
    cbor_encode_uint(w, 0x02);
    cbor_encode_array(w, 3);
    cbor_encode_text(w, "appid");
    cbor_encode_text(w, "credProtect");
    cbor_encode_text(w, "appidExclude");
}

static void encodeAaguid(cbor_writer_t *w) {
    cbor_encode_uint(w, 0x03);
    cbor_encode_bytes(w, AAGUID, 16);
}

static void encodeOptions(cbor_writer_t *w) {
    cbor_encode_uint(w, 0x04);
    cbor_encode_map(w, 7);
    cbor_encode_text(w, "rk");
    cbor_encode_bool(w, true);
    cbor_encode_text(w, "up");
    cbor_encode_bool(w, true);
    cbor_encode_text(w, "uv");
    cbor_encode_bool(w, false);
    cbor_encode_text(w, "plat");
    cbor_encode_bool(w, false);
    cbor_encode_text(w, "credMgmt");
    cbor_encode_bool(w, true);
    cbor_encode_text(w, "clientPin");
    cbor_encode_bool(w, true);
    cbor_encode_text(w, "pinUvAuthToken");
    cbor_encode_bool(w, true);
}

static void encodeMaxMsgSize(cbor_writer_t *w) {
    cbor_encode_uint(w, 0x05);
    cbor_encode_uint(w, 1200);
}

static void encodePinUvAuthProtocols(cbor_writer_t *w) {
    cbor_encode_uint(w, 0x06);
    cbor_encode_array(w, 1);
    cbor_encode_uint(w, 2);
}
```

2. Simplify main function:
```cpp
uint8_t ctap2_get_info(uint8_t *response, uint16_t *response_len) {
    cbor_writer_t w;
    cbor_writer_init(&w, response + 1, *response_len - 1);

    cbor_encode_map(&w, 10);
    encodeVersions(&w);
    encodeExtensions(&w);
    encodeAaguid(&w);
    encodeOptions(&w);
    encodeMaxMsgSize(&w);
    encodePinUvAuthProtocols(&w);
    // ... remaining encoders

    if (cbor_writer_error(&w)) {
        response[0] = CTAP2_ERR_OTHER;
        *response_len = 1;
        return CTAP2_OK;
    }

    response[0] = CTAP2_OK;
    *response_len = 1 + cbor_writer_length(&w);
    return CTAP2_OK;
}
```

3. Move debug code to separate function:
```cpp
#if CTAP2_DEBUG
static void dumpGetInfoResponse(const uint8_t *response, uint16_t len) {
    LOG_I("CTAP2", "getInfo response len=%u", len);
    for (uint16_t offset = 0; offset < len; offset += 16) {
        // Debug dump
    }
}
#endif
```

**Expected result:**
- Main function reduced to ~40 lines
- Each field encoder is ~5-10 lines
- Easier to modify individual fields
- Better testability with isolated encoders

## References

- [Extract Function refactoring](https://refactoring.com/catalog/extractFunction.html)
- [Cyclomatic Complexity Guidelines](https://www.sonarsource.com/docs/CyclomaticComplexity.pdf)
