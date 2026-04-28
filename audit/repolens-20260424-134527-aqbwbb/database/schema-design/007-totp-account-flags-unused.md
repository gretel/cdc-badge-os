---
title: "[LOW] TOTP account flags field defined but never used"
severity: LOW
domain: database/schema-design
lens: schema-design
labels:
  - "storage"
  - "totp"
  - "schema-waste"
---

## Summary
The `TotpAccount` struct and `TotpPayload` include a `flags` field that is defined but never used anywhere in the codebase. This adds 1 byte of overhead per TOTP account without providing any functionality.

**Evidence:**
- `TotpStore.h:23`: `uint8_t flags;` in `TotpAccount` struct
- `TotpStore.cpp:24`: `uint8_t flags;` in `TotpPayload` struct
- `TotpStore.cpp:272`: `payload.flags = 0;` - always set to 0
- `TotpStore.cpp:184`: `out->flags = payload.flags;` - copied but never used

## Impact
1. **Wasted space**: 1 byte per TOTP account (100 accounts = 100 bytes)
2. **Confusion**: Future developers may wonder what flags are for
3. **Maintenance**: Unused field adds to schema complexity

## Evidence
From `TotpStore.cpp:268-273`:
```cpp
TotpPayload payload = {};
if (issuer) {
    strncpy(payload.issuer, issuer, sizeof(payload.issuer) - 1);
}
memcpy(payload.secret, secret, static_cast<size_t>(secretLen));
payload.secretLen = static_cast<uint8_t>(secretLen);
payload.digits = digits ? digits : DEFAULT_DIGITS;
payload.period = period ? period : DEFAULT_PERIOD;
payload.algorithm = algorithm;
payload.flags = 0;  // Always 0, never set to anything else
```

From `TotpStore.cpp:179-185`:
```cpp
out->digits = payload.digits ? payload.digits : DEFAULT_DIGITS;
out->period = payload.period ? payload.period : DEFAULT_PERIOD;
out->algorithm = payload.algorithm;
out->flags = payload.flags;  // Copied but never used anywhere
```

## Recommended Fix
1. **Remove the flags field** from both `TotpPayload` and `TotpAccount` structs
2. **Update all references** to remove assignments and copies
3. **Reclaim space**: This adds 1 byte to the notes field in Password or reduces total payload size

Alternative: If flags are planned for future use, add a comment explaining planned usage:
```cpp
uint8_t flags;  // Reserved for future use (e.g., account status, encryption flags)
```

## References
- Embedded data structure optimization
- Schema design for constrained storage
