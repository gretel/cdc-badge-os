---
title: "[LOW] TOTP payload struct uses fixed-size char array without null terminator space"
severity: LOW
domain: security-sast
lens: toolgate
labels:
  - "CWE-170: Improper Null Termination"
---

## Summary
The `TotpPayload` struct stores the `issuer` field as `char[TotpStore::ISSUER_LEN]` (32 bytes) without space for a null terminator. While this works because the struct is zero-initialized and `strncpy()` is used with `sizeof - 1`, this design is fragile and could break if the initialization pattern changes.

**Location:** `components/mod_totp/src/TotpStore.cpp:16-17`

## Impact
- **Fragile design**: Relies on zero-initialization and careful use of `strncpy(..., sizeof - 1)`
- **Confusion**: Inconsistent with `TotpAccount` which has `char[32 + 1]`
- **Maintenance risk**: Future developers may not understand the implicit null-termination requirement

## Evidence

### TotpPayload (storage format, no null terminator space)
```cpp
#pragma pack(push, 1)
struct TotpPayload {
    char issuer[TotpStore::ISSUER_LEN];  // 32 bytes, no +1 for null
    // ...
};
```

### TotpAccount (in-memory format, with null terminator space)
```cpp
struct TotpAccount {
    char name[16 + 1];   // 17 bytes with null
    char issuer[32 + 1]; // 33 bytes with null
    // ...
};
```

### Usage in readAccount (TotpStore.cpp:178)
```cpp
memset(out, 0, sizeof(*out));  // Zero-initialize output
strncpy(out->issuer, payload.issuer, sizeof(out->issuer) - 1);  // Copy 32 chars
// Null terminator comes from memset, not strncpy
```

## Recommended Fix
Add explicit null-termination after `strncpy()`:

```cpp
strncpy(out->issuer, payload.issuer, sizeof(out->issuer) - 1);
out->issuer[sizeof(out->issuer) - 1] = '\0';  // Explicit null-terminate
```

Or change the payload struct to include null terminator space (requires storage format change):
```cpp
struct TotpPayload {
    char issuer[TotpStore::ISSUER_LEN + 1];  // 33 bytes with null
    // ...
};
```

## References
- CWE-170: Improper Null Termination - https://cwe.mitre.org/data/definitions/170.html
- Struct packing for storage formats - https://en.wikipedia.org/wiki/Data_structure_alignment
