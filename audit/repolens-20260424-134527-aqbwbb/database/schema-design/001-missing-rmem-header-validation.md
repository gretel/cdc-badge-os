---
title: "[MEDIUM] Missing R-Memory header checksum validation"
severity: MEDIUM
domain: database/schema-design
lens: storage-integrity
labels:
  - "storage"
  - "data-integrity"
  - "tropic01"
---

## Summary
The `RMemHeader` struct in `components/cdc_hal/include/cdc_hal/ISecureElement.h:161` defines a `checksum` field, but there is no consistent validation logic across the codebase to verify this checksum when reading R-Memory slots. The checksum is defined but appears to be computed and validated inconsistently.

**Evidence:**
- `ISecureElement.h:161`: `struct RMemHeader { uint8_t magic; uint8_t checksum; uint8_t moduleId; uint8_t flags; char name[RMEM_NAME_LEN]; uint16_t payloadLen; };`
- `TotpStore.cpp:170`: Reads header with `rmemReadWithHeader()` but no explicit checksum validation visible
- `PasswordStore.cpp:125`: Same pattern - reads header but checksum validation not explicit
- `GpgStorage.cpp:350`: Reads raw data and checks magic, but checksum validation not shown

## Impact
Without consistent checksum validation:
1. **Silent data corruption**: Corrupted R-Memory entries may be read as valid, leading to silent failures
2. **Security risk**: An attacker could potentially modify stored data without detection
3. **Debug difficulty**: Hard to distinguish between write failures and read corruption

## Evidence
From `ISecureElement.h:161`:
```cpp
struct __attribute__((packed)) RMemHeader {
    uint8_t magic;
    uint8_t checksum;  // Defined but validation unclear
    uint8_t moduleId;
    uint8_t flags;
    char name[RMEM_NAME_LEN];
    uint16_t payloadLen;
};
```

From `TotpStore.cpp:165-175`:
```cpp
auto res = se->rmemReadWithHeader(physSlot, &header, payloadBuf, sizeof(payloadBuf), &payloadLen);
if (res != cdc::hal::SeResult::OK) {
    return false;
}
// No explicit checksum validation here
```

## Recommended Fix
1. Add explicit checksum validation in `rmemReadWithHeader()` implementation
2. Define checksum algorithm (e.g., simple sum, CRC8, XOR)
3. Add validation in all read paths: `TotpStore::readAccount()`, `PasswordStore::readEntry()`, `GpgStorage::load_dec_privkey()`
4. Consider adding a `validateHeader()` helper function

## References
- OpenPGP Card specification for similar header validation patterns
- Embedded systems data integrity best practices
