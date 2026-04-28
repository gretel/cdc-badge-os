---
title: "[LOW] Repeated strcmp in FIDO2 CBOR parsing for same keys"
severity: LOW
domain: mod_fido2
lens: algorithm-efficiency
labels:
  - "audit:performance/algorithm"
---

## Summary
In `components/mod_fido2/src/ctap2.cpp`, the CBOR parsing functions (`parse_rp_map`, `parse_user_map`, `parse_extensions_map`, `parse_options_map`) use multiple `strcmp()` calls to match key names. While the keys are short and the maps are small, this represents a pattern of repeated string comparisons that could be optimized.

**Evidence:**
```cpp
// parse_user_map (lines 716-739)
for (int j = 0; j < user_count; j++) {
    char user_key[16];
    size_t key_len;
    if (!cbor_read_text(r, user_key, sizeof(user_key), &key_len)) {
        cbor_skip_item(r);
        continue;
    }
    if (strcmp(user_key, "id") == 0) {
        // ...
    } else if (strcmp(user_key, "name") == 0) {
        // ...
    }
}
```

Similar patterns appear in:
- `parse_rp_map` (lines 691-706): 1 strcmp
- `parse_extensions_map` (lines 783-809): 2 strcmp calls
- `parse_options_map` (lines 818-839): 3 strcmp calls

## Impact
- **Minor overhead**: CBOR parsing happens once per FIDO2 operation (makeCredential, getAssertion).
- **Small maps**: The maps are typically small (2-5 keys), so the impact is minimal.
- **Short strings**: Key names are short (2-15 chars), so `strcmp()` is fast.

## Evidence
**File**: `components/mod_fido2/src/ctap2.cpp`
**Lines**: 
- `parse_rp_map`: 691-706
- `parse_user_map`: 716-739
- `parse_extensions_map`: 783-809
- `parse_options_map`: 818-839

## Recommended Fix
Since CBOR keys are typically single words, use a hash or switch on first character for faster matching:

```cpp
// Optimized key matching
static inline int matchKey(const char* key, const char* expected) {
    // Fast path: compare first char
    if (key[0] != expected[0]) return 0;
    // Slow path: full compare
    return strcmp(key, expected) == 0;
}

// Or use a simple hash for common keys
static inline uint8_t hashKey(const char* key) {
    return (key[0] * 7 + key[strlen(key) - 1]) & 0x0F;
}

// Switch on hash for fast dispatch
switch (hashKey(user_key)) {
    case hashKey("id"):
        if (strcmp(user_key, "id") == 0) { /* ... */ }
        break;
    case hashKey("name"):
        if (strcmp(user_key, "name") == 0) { /* ... */ }
        break;
}
```

Given the small map sizes, this is a low-priority optimization. The current implementation is acceptable for FIDO2 use cases.

## References
- [String interning for fast comparison](https://en.wikipedia.org/wiki/String_interning)
