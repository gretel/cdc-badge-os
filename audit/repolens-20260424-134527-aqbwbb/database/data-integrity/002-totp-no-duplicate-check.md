---
title: "[MEDIUM] TOTP store allows duplicate account names without constraint"
severity: MEDIUM
domain: data-integrity
lens: database
labels:
  - totp-storage
  - uniqueness
  - duplicate-keys
---

## Summary
The TOTP store (`TotpStore`) allows multiple accounts with the same name/issuer combination to be stored. There is no uniqueness constraint (database-level or application-level) to prevent duplicate entries for the same TOTP account.

**Files:**
- `components/mod_totp/src/TotpStore.cpp:247-296` (addAccount function)
- `components/mod_totp/include/mod_totp/TotpStore.h:35-38` (addAccount signature)

## Impact
Users can accidentally create duplicate TOTP accounts, leading to:
- Confusion about which account to use
- Wasted storage slots (limited to 100 TOTP accounts: slots 32-131)
- Potential for using an outdated/incorrect secret after re-adding an account

Unlike the vCard store which has `vcard_is_duplicate()` check, the TOTP store has no such validation.

## Evidence
In `TotpStore.cpp:247-296`, the `addAccount` function:
```cpp
bool TotpStore::addAccount(const char* name, const char* issuer, const char* secretBase32,
                           uint8_t digits, uint32_t period, uint8_t algorithm) {
    if (!name || !secretBase32) return false;
    if (!hasSlotRange_) return false;

    uint16_t slot = 0;
    if (!findFreeSlot(&slot)) {
        LOG_W(TAG, "No free TOTP slots");
        return false;
    }
    // ... write account without checking for duplicates
}
```

Compare to `vcard_store.cpp:544-550` which has duplicate checking:
```cpp
uint32_t hash = fnv1a_hash(vcard, len);
if (vcard_is_duplicate(nvs, vcard, len, hash)) {
    nvs_close(nvs);
    set_err(err, err_len, "Duplicate vCard");
    return false;
}
```

The TOTP store only checks for free slots, not for duplicate account data.

## Recommended Fix
Add a duplicate detection function for TOTP accounts:

1. Create `findAccountByNameIssuer(const char* name, const char* issuer)` that iterates existing accounts
2. Call this function in `addAccount()` before writing
3. Return an error or prompt for confirmation if a duplicate exists

```cpp
int8_t TotpStore::findAccountByNameIssuer(const char* name, const char* issuer) {
    uint16_t cap = capacity();
    for (uint16_t i = 0; i < cap; i++) {
        TotpAccount account;
        if (!readAccount(i, &account)) continue;
        
        if (strcmp(account.name, name) == 0) {
            if (!issuer || strcmp(account.issuer, issuer) == 0) {
                return i;  // Found duplicate
            }
        }
    }
    return -1;  // No duplicate
}

bool TotpStore::addAccount(...) {
    // ... existing validation ...
    
    int8_t existing = findAccountByNameIssuer(name, issuer);
    if (existing >= 0) {
        LOG_W(TAG, "Duplicate account: %s @ %s", name, issuer);
        return false;  // Or return existing slot for update
    }
    
    // ... proceed with add ...
}
```

## References
- FIDO2 spec uses (RP ID + User ID) as unique key: `fido2_storage_find_by_rp_user()` in `fido2_storage.cpp:558-590`
- Password store could also benefit from similar title-based uniqueness checking
