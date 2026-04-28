---
title: "[MEDIUM] Inappropriate Intimacy: TotpPayload struct exposes internal storage format"
severity: MEDIUM
domain: modules
lens: code-smells
labels:
  - "inappropriate-intimacy"
  - "mod_totp"
---

## Summary
In `components/mod_totp/src/TotpStore.cpp:15-24`, the `TotpPayload` struct is defined with `#pragma pack(push, 1)` which exposes the exact binary storage format. This creates tight coupling between the storage layer and the TOTP module, making it hard to change the format later.

## Impact
**Brittle format**: Any change to the struct requires updating storage migration logic.

**Platform dependency**: `#pragma pack` behavior can vary across compilers.

**Encapsulation violation**: The payload structure is exposed instead of being hidden in a serializer.

## Evidence
`components/mod_totp/src/TotpStore.cpp:15-24`:
```cpp
#pragma pack(push, 1)
struct TotpPayload {
    char issuer[TotpStore::ISSUER_LEN];
    uint8_t secret[TotpStore::SECRET_LEN];
    uint8_t secretLen;
    uint8_t digits;
    uint32_t period;
    uint8_t algorithm;
    uint8_t flags;
};
#pragma pack(pop)
```

This is used directly in storage operations:
`components/mod_totp/src/TotpStore.cpp:174-183`:
```cpp
TotpPayload payload = {};
memcpy(&payload, payloadBuf, sizeof(payload));

memset(out, 0, sizeof(*out));
strncpy(out->name, header.name, sizeof(out->name) - 1);
strncpy(out->issuer, payload.issuer, sizeof(out->issuer) - 1);
memcpy(out->secret, payload.secret, sizeof(payload.secret));
out->secretLen = payload.secretLen;
out->digits = payload.digits ? payload.digits : DEFAULT_DIGITS;
out->period = payload.period ? payload.period : DEFAULT_PERIOD;
out->algorithm = payload.algorithm;
out->flags = payload.flags;
```

Same pattern in `PasswordStore.cpp:14-21`.

## Recommended Fix
1. **Create a serializer class**:
```cpp
class TotpSerializer {
public:
    static size_t serialize(const TotpAccount& account, uint8_t* buffer, size_t bufferSize);
    static TotpAccount deserialize(const uint8_t* buffer, size_t bufferSize);
};
```

2. **Use versioned format**:
```cpp
struct TotpPayloadV1 {
    uint8_t version = 1;
    char issuer[TotpStore::ISSUER_LEN];
    // ... fields
};
```

3. **Hide the struct** in a `.cpp` file instead of exposing it.

**Estimated effort**: ~1 hour to create serializer and update read/write operations.

## References
- Refactoring.com: "Inappropriate Intimacy" - https://refactoring.com/catalog/moveMethod
- Martin Fowler, "Refactoring: Improving the Design of Existing Code", Chapter 7
