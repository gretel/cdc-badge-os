---
title: "[MEDIUM] Untested TOTP Base32 decode edge cases and error paths"
severity: MEDIUM
domain: mod_totp
lens: error-path-tests
labels:
  - "audit:testing/error-path-tests"
---

## Summary
The TOTP module's `base32Decode()` function in `components/mod_totp/src/TotpStore.cpp` has several error paths that are never tested with invalid input:

1. **Invalid character handling** - Characters outside A-Z, a-z, 2-7 are returned as `-1`, but tests never try invalid characters like `1`, `8`, `9`, `0`, special symbols
2. **Buffer overflow protection** - When `count >= outMax`, returns `-1`, but tests never try secrets larger than `SECRET_LEN`
3. **Null pointer handling** - Returns `-1` for null `encoded` or `out`, but tests never pass null pointers
4. **Whitespace handling** - Handles `=`, space, tab, CR, LF but never tested with unusual whitespace combinations

Additionally, `addAccount()` and `updateAccount()` in the same file don't test:
- What happens when `findFreeSlot()` returns false (full storage)
- What happens when `se->rmemWriteWithHeader()` fails
- What happens when TOTP storage cache update fails

**Evidence:**
- `components/mod_totp/src/TotpStore.cpp:44-71` - `base32Decode()` function
- `components/mod_totp/src/TotpStore.cpp:238-295` - `addAccount()` function  
- `components/mod_totp/src/TotpStore.cpp:309-357` - `updateAccount()` function
- `components/mod_totp/src/TotpStore.cpp:278-281` - Base32 decode error not tested
- `components/mod_totp/src/TotpStore.cpp:288-292` - R-MEM write failure not tested

## Impact
**Security/Usability Risk:**
- Invalid Base32 secrets might cause unexpected behavior if edge cases aren't handled correctly
- Full storage condition might leave user with unclear error message
- Silent failures in storage could lead to data loss without user notification

**Testing Gap:**
- No unit tests exist for TOTP module at all (only 3 smoke tests in `/test/` directory for vCard module)
- Error paths in Base32 decoding could hide bugs in secret parsing
- Storage failure paths could lead to corrupted state if not properly handled

## Evidence
**Code snippets showing untested error paths:**

```cpp
// components/mod_totp/src/TotpStore.cpp:54-58
int value = base32CharValue(*p);
if (value < 0) {
    return -1;  // Never tested with invalid chars like '1', '8', '@', etc.
}

// components/mod_totp/src/TotpStore.cpp:64-67
if (count >= outMax) {
    return -1;  // Never tested with oversized secrets
}

// components/mod_totp/src/TotpStore.cpp:278-281
int secretLen = base32Decode(secretBase32, secret, SECRET_LEN);
if (secretLen <= 0) {
    LOG_E(TAG, "Invalid Base32 secret");
    return false;  // Return path never tested
}

// components/mod_totp/src/TotpStore.cpp:288-292
auto res = se->rmemWriteWithHeader(...);
if (res != cdc::hal::SeResult::OK) {
    LOG_E(TAG, "Failed to write slot %u", slot);
    return false;  // Storage failure path never tested
}
```

**Current test coverage:**
- Only 3 trivial tests exist in entire codebase, all for vCard module
- `test/test_vcard_store/test_vcard_store.cpp` - One smoke test for vCard
- `test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp` - One smoke test for BLE
- `test/test_vcard_module_link/test_vcard_module_link.cpp` - One link test
- **Zero tests for TOTP module error paths**

## Recommended Fix
Create comprehensive unit tests for TOTP module error paths:

1. **Base32 decode tests** (`test/test_totp_base32/`):
   - Test with valid Base32 strings (A-Z, a-z, 2-7)
   - Test with invalid characters: `1`, `8`, `9`, `0`, `@`, `#`, `$`, etc.
   - Test with oversized secrets (> SECRET_LEN bytes)
   - Test with null pointers
   - Test with empty strings
   - Test with various whitespace combinations

2. **Storage error tests** (`test/test_totp_storage/`):
   - Mock `ISecureElement` to return `SLOT_EMPTY` on `rmemWriteWithHeader()`
   - Mock `ISecureElement` to return `ERROR` on `rmemWriteWithHeader()`
   - Test `findFreeSlot()` returns false (all slots used)
   - Test cache update failure after successful write
   - Test with invalid slot indices (out of range)

3. **Account CRUD tests** (`test/test_totp_crud/`):
   - Test `addAccount()` with duplicate names
   - Test `updateAccount()` with non-existent slot
   - Test `deleteAccount()` with invalid slot
   - Test `readAccount()` with corrupted data

Each test file should be ~1 hour of work and follow existing test structure.

## References
- FIDO2 spec on credential storage: https://fidoalliance.org/specs/fido2/
- Base32 encoding RFC 4648: https://datatracker.ietf.org/doc/html/rfc4648
- Existing test structure: `test/test_vcard_store/test_vcard_store.cpp`
- TOTP algorithm RFC 6238: https://datatracker.ietf.org/doc/html/rfc6238
