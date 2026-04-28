---
title: "[LOW] CBOR decoding uses tight loops in hot path"
severity: LOW
domain: performance
lens: blocking-io
labels:
  - "performance"
  - "cbor"
  - "fido2"
---

## Summary

The CBOR decoding helpers in `components/mod_fido2/src/cbor_helpers.cpp` use tight loops for parsing container data. While each loop iteration is fast, nested containers (common in FIDO2 CTAP2 messages) can result in many iterations blocking the calling task.

**Affected file:**
- `components/mod_fido2/src/cbor_helpers.cpp` (lines 557-600)

**Evidence:**
```cpp
// components/mod_fido2/src/cbor_helpers.cpp:557-600
#define CBOR_MAX_RECURSION_DEPTH 8
#define CBOR_MAX_CONTAINER_SIZE  256

static bool cbor_skip_item_impl(cbor_reader_t *r, uint8_t depth) {
    if (depth > CBOR_MAX_RECURSION_DEPTH) {
        LOG_W("CBOR", "Max recursion depth exceeded");
        return false;
    }

    cbor_item_t item;
    if (!cbor_read_item(r, &item)) return false;

    // For containers, skip all nested items with limits
    if (item.type == CBOR_ARRAY) {
        if (item.value > CBOR_MAX_CONTAINER_SIZE) {
            LOG_W("CBOR", "Array too large: %llu", (unsigned long long)item.value);
            return false;
        }
        for (uint64_t i = 0; i < item.value; i++) {  // Tight loop
            if (!cbor_skip_item_impl(r, depth + 1)) return false;
        }
    } else if (item.type == CBOR_MAP) {
        if (item.value > CBOR_MAX_CONTAINER_SIZE) {
            LOG_W("CBOR", "Map too large: %llu", (unsigned long long)item.value);
            return false;
        }
        for (uint64_t i = 0; i < item.value * 2; i++) {  // Tight loop (key + value)
            if (!cbor_skip_item_impl(r, depth + 1)) return false;
        }
    }
    return true;
}
```

**Additional loops in encoding:**
```cpp
// components/mod_fido2/src/cbor_helpers.cpp:355-368
static bool read_type_value(cbor_reader_t *r, uint8_t *type, uint64_t *value) {
    // ...
    } else if (info == 26) {
        uint8_t b[4];
        for (int i = 0; i < 4; i++) {  // Read 4 bytes
            if (!read_byte(r, &b[i])) return false;
        }
        // ...
    } else if (info == 27) {
        uint8_t b[8];
        for (int i = 0; i < 8; i++) {  // Read 8 bytes
            if (!read_byte(r, &b[i])) return false;
        }
    }
}
```

## Impact

1. **Blocking in CTAP2 parsing**: FIDO2 authentication messages can contain nested arrays and maps (e.g., credential lists, extension data). Deep nesting can result in many loop iterations.

2. **No yielding during parsing**: The parser runs to completion without allowing other tasks to run.

3. **Variable parsing time**: Depends on message complexity - simple messages are fast, complex ones (with extensions) take longer.

4. **Memory access pattern**: Sequential memory access is cache-friendly but still blocks the CPU.

## Evidence

**FIDO2 message complexity:**
- Simple credential: ~10-20 CBOR items
- Credential with extensions: ~30-50 items
- Nested credential list: ~50-100 items

**Loop iterations per operation:**
- `cbor_skip_item_impl`: O(n) where n is total items in container
- `read_type_value`: 4-8 iterations for large integers
- `cbor_parse_cose_key`: O(k) where k is map entry count

## Recommended Fix

1. **Add yield points for deep containers** (~30 min):
```cpp
static bool cbor_skip_item_impl(cbor_reader_t *r, uint8_t depth, uint32_t *counter) {
    // ...
    for (uint64_t i = 0; i < item.value; i++) {
        if (!cbor_skip_item_impl(r, depth + 1, counter)) return false;
        
        // Yield every 50 iterations
        (*counter)++;
        if (*counter % 50 == 0) {
            // Check for yield point
            if (shouldYield()) {
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }
    }
}
```

2. **Pre-validate container sizes** (faster rejection):
```cpp
// Check total size before parsing
uint32_t getContainerSize(cbor_reader_t *r) {
    // Quick size calculation
}

bool parseWithSizeLimit(cbor_reader_t *r, uint32_t maxBytes) {
    if (getContainerSize(r) > maxBytes) {
        return false;  // Fast rejection
    }
    // ...
}
```

3. **Use iterative instead of recursive parsing**:
```cpp
// Avoid recursion overhead for deep nesting
bool cbor_parse_iterative(cbor_reader_t *r) {
    Stack<cbor_item_t> stack;
    while (r->offset < r->size) {
        cbor_item_t item;
        cbor_read_item(r, &item);
        
        // Process based on stack state
    }
}
```

**Estimated effort**: 30-60 minutes for yield point implementation

## References

- [CBOR specification (RFC 8949)](https://www.rfc-editor.org/rfc/rfc8949.html)
- [CTAP2 specification](https://fidoalliance.org/specs/fido-v2.1-ps-20210615/fido-client-to-authenticator-protocol-v2.1-ps-20210615.html)
- Typical FIDO2 messages: 50-500 bytes

</content>