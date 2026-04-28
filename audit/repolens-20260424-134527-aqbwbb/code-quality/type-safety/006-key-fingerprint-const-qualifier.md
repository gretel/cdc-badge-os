---
title: "[LOW] Missing null check for char* pointer in key_fingerprint_from_pubkey"
severity: LOW
domain: cdc_core
lens: type-safety
labels:
  - "audit:code-quality/type-safety"
---

## Summary
The function `key_fingerprint_from_pubkey` in `KeyFingerprint.cpp` checks for null `pubkey` and `buf` pointers, but the `char* buf` parameter lacks a `const` qualifier despite being an output parameter. Additionally, the function could benefit from a more explicit check for empty buffer size.

## Impact
1. **Type clarity**: The `char* buf` parameter is used only as an output, but lacks `const` qualifier to indicate it's not reassigned.
2. **Edge case**: `len < KEY_FINGERPRINT_MAX_LEN` check is correct, but could be more explicit about minimum required size.

## Evidence
In `components/cdc_core/include/cdc_core/KeyFingerprint.h`:
```cpp
bool key_fingerprint_from_pubkey(const uint8_t* pubkey, size_t pubkey_len,
                                 char* buf, size_t len);  // buf should be: char* const buf
```

In `components/cdc_core/src/KeyFingerprint.cpp` lines 42-46:
```cpp
bool key_fingerprint_from_pubkey(const uint8_t* pubkey, size_t pubkey_len,
                                 char* buf, size_t len) {
    if (!pubkey || !buf || len < KEY_FINGERPRINT_MAX_LEN || pubkey_len == 0) {
        return false;
    }
    // ...
}
```

The function correctly checks for null pointers, but the type signature could be more precise.

## Recommended Fix
Update the function signature to use `char* const buf` to indicate the pointer itself is not reassigned:

```cpp
bool key_fingerprint_from_pubkey(const uint8_t* const pubkey, size_t pubkey_len,
                                 char* const buf, size_t len);
```

This is a minor improvement that makes the API more self-documenting.

## References
- C++ Core Guidelines F.46: "Use 'const' and 'constexpr' to help avoid unintended conversions and reassignments"

</content>