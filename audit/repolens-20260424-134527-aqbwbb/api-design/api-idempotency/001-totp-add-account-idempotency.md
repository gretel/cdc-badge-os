---
title: "[MEDIUM] TOTP_ADD serial command lacks idempotency for duplicate account creation"
severity: MEDIUM
domain: api-design/api-idempotency
lens: api-idempotency
labels:
  - audit:api-design/api-idempotency
---

## Summary
The `TOTP_ADD` serial command (and corresponding `TotpStore::addAccount()` function) does not check for duplicate accounts before creating a new entry. When called multiple times with the same account name and secret, it creates duplicate TOTP accounts in different slots.

**Location:** `components/mod_totp/src/TotpStore.cpp:247-288` (addAccount), `components/mod_totp/src/TotpModule.cpp:235-267` (cmd_totp_add)

## Impact
- **Data duplication:** Multiple identical TOTP accounts can accumulate, wasting limited secure element R-Memory slots (only 100 slots available: 32-131)
- **User confusion:** Duplicate accounts appear in the list view with identical names
- **No deduplication:** The `findFreeSlot()` function only checks for empty slots, not existing accounts with the same name/secret

## Evidence
```cpp
// components/mod_totp/src/TotpStore.cpp:247-288
bool TotpStore::addAccount(const char* name, const char* issuer, const char* secretBase32,
                            uint8_t digits, uint32_t period, uint8_t algorithm) {
    if (!name || !secretBase32) return false;
    if (!hasSlotRange_) return false;

    uint16_t slot = 0;
    if (!findFreeSlot(&slot)) {  // Only checks for free slot, not duplicates
        LOG_W(TAG, "No free TOTP slots");
        return false;
    }

    // ... decodes secret and writes to slot without duplicate check ...

    auto res = se->rmemWriteWithHeader(
        slot, moduleId_, name, 0,
        reinterpret_cast<const uint8_t*>(&payload), sizeof(payload)
    );
    // ...
}
```

Serial command handler (no duplicate check):
```cpp
// components/mod_totp/src/TotpModule.cpp:235-267
static void cmd_totp_add(const char* args) {
    // Parses name, secret, issuer, digits, period, algo
    // Calls addAccount without checking for existing account
    bool ok = TotpStore::instance().addAccount(name, issuer, secret, digits, period, algo);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

## Recommended Fix
Add a duplicate check before creating a new TOTP account:

1. **Iterate existing accounts** and compare by name (case-insensitive) and secret
2. **Return early** if a matching account exists, indicating the slot number
3. **Add helper function** `findAccountByNameAndSecret()` that returns slot index or -1

Example implementation:
```cpp
int16_t TotpStore::findAccountByNameAndSecret(const char* name, const char* secretBase32) {
    if (!name || !secretBase32) return -1;
    
    // Decode the secret for comparison
    uint8_t newSecret[SECRET_LEN];
    int newSecretLen = base32Decode(secretBase32, newSecret, SECRET_LEN);
    if (newSecretLen <= 0) return -1;
    
    // Iterate all slots
    for (uint16_t slot = 0; slot < capacity(); slot++) {
        TotpAccount account = {};
        if (!readAccount(slot, &account)) continue;
        
        // Compare name (case-insensitive)
        if (strcasecmp(account.name, name) != 0) continue;
        
        // Compare secret bytes
        if (account.secretLen != newSecretLen) continue;
        if (memcmp(account.secret, newSecret, newSecretLen) == 0) {
            return slot;  // Found duplicate
        }
    }
    return -1;  // No duplicate found
}
```

Then modify `addAccount()`:
```cpp
bool TotpStore::addAccount(const char* name, const char* issuer, const char* secretBase32, ...) {
    // Check for duplicate first
    int16_t existing = findAccountByNameAndSecret(name, secretBase32);
    if (existing >= 0) {
        LOG_I(TAG, "Account '%s' already exists in slot %u", name, existing);
        return false;  // Or return true to indicate idempotent success
    }
    // ... rest of existing code ...
}
```

## References
- FIDO2/WebAuthn uses RP ID + User ID for credential uniqueness (similar pattern)
- CTAP2 `authenticatorMakeCredential` returns `CTAP2_ERR_CREDENTIAL_EXCLUDED` when credential already exists
- Idempotency best practices: POST operations should either be naturally idempotent or support idempotency keys
