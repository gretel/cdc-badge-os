---
title: "[MEDIUM] Missing Data Subject Rights Implementation (Art. 15-22 DSGVO)"
severity: MEDIUM
domain: gdpr-dsgvo
lens: compliance/gdpr-dsgvo
labels:
  - "audit:compliance/gdpr-dsgvo"
---

## Summary
The CDC Badge OS firmware stores personal data (vCards, passwords, TOTP secrets, GPG keys) but lacks implementation of core data subject rights required by Articles 15-22 of the GDPR/DSGVO. There are no mechanisms for users to:
- Request a structured export of all their personal data (Art. 20 - Data Portability)
- Request bulk deletion of all stored data (Art. 17 - Right to be Forgotten)
- Obtain a comprehensive list of all stored data with metadata (Art. 15 - Right of Access)

## Impact
**Legal Risk**: Missing data subject rights implementation creates compliance gaps that could lead to regulatory scrutiny or fines if the badge is used in an EU context with documented personal data processing.

**User Experience**: Users cannot easily export their data for backup or migration purposes, nor can they perform a complete data wipe with a single action.

**Maintenance**: Individual deletion functions exist (`vcard_store_delete()`, `PasswordStore::deleteEntry()`, `TotpStore::deleteAccount()`) but no unified bulk operations.

## Evidence

### Data Storage Locations
1. **vCard data** (names, phone numbers, emails, addresses): `components/mod_vcard/src/vcard_store.cpp`
   - NVS namespace `mod_vcard`, key `own` for own vCard
   - Keys `c00` to `c99` for up to 100 peer vCards
   - Lines 317-442: Storage and deletion functions

2. **Password entries** (usernames, passwords, URLs, notes): `components/mod_password/src/PasswordStore.cpp`
   - TROPIC01 R-Memory slots 150-511 (configurable)
   - Lines 1-383: Full CRUD operations but no bulk export/delete

3. **TOTP secrets** (issuer names, account names, Base32 secrets): `components/mod_totp/src/TotpStore.cpp`
   - TROPIC01 R-Memory slots 32-131 (configurable)
   - Lines 1-529: Full CRUD operations but no bulk export/delete

4. **GPG key metadata** (key fingerprints, issuer info): `components/mod_gpg/src/GpgStorage.cpp`
   - TROPIC01 ECC slots 1-31 and R-Memory slot 0
   - Lines 1-488: Key storage with encrypted DEC key

### Missing Functions
No implementation of:
- `dataSubjectExportAll()` - Returns all personal data in machine-readable format (JSON, CSV)
- `dataSubjectDeleteAll()` - Complete wipe of all personal data
- `getDataInventory()` - Lists all stored data with metadata (type, size, date)

Existing deletion functions are granular only:
```cpp
// vcard_store.h:18
bool vcard_store_delete(uint16_t slot);

// PasswordStore.h:52
bool PasswordStore::deleteEntry(uint16_t slot);

// TotpStore.h:38
bool TotpStore::deleteAccount(uint16_t slot);
```

## Recommended Fix

### Option 1: Add Bulk Export/Delete Functions (~1 hour)
Create new functions in each module:

**vcard_store.cpp** (add after line 699):
```cpp
/**
 * \brief Exports all stored vCards as JSON array for data portability (Art. 20 DSGVO).
 * \param out Output JSON buffer.
 * \param max_len Output buffer size.
 * \return Number of bytes written.
 */
size_t vcard_store_export_all(char* out, size_t max_len) {
    if (!out || max_len == 0) return 0;
    
    uint16_t slots[VCARD_MAX_CARDS];
    uint16_t count = vcard_store_get_sorted(slots, VCARD_MAX_CARDS);
    
    size_t pos = 0;
    out[pos++] = '[';
    
    for (uint16_t i = 0; i < count && pos < max_len - 5; i++) {
        char vcard[VCARD_MAX_LEN + 1];
        size_t len = vcard_store_get(slots[i], vcard, sizeof(vcard));
        
        if (i > 0 && pos < max_len - 2) out[pos++] = ',';
        out[pos++] = '"';
        
        // Escape JSON string
        for (size_t j = 0; j < len && pos < max_len - 3; j++) {
            if (vcard[j] == '"' || vcard[j] == '\\') out[pos++] = '\\';
            out[pos++] = vcard[j];
        }
        
        out[pos++] = '"';
    }
    
    out[pos++] = ']';
    out[pos] = '\0';
    return pos;
}

/**
 * \brief Deletes all stored vCards for right to be forgotten (Art. 17 DSGVO).
 * \return Number of deleted vCards.
 */
uint16_t vcard_store_delete_all(void) {
    uint16_t deleted = 0;
    for (uint16_t slot = 0; slot < VCARD_MAX_CARDS; slot++) {
        if (vcard_store_delete(slot)) deleted++;
    }
    return deleted;
}
```

**Similar functions for PasswordStore and TotpStore.**

### Option 2: Add Serial Command Interface (~30 min)
Add commands to `serial_cmd` component:
```
DATA_EXPORT_ALL - Returns JSON of all personal data
DATA_DELETE_ALL - Wipes all personal data
DATA_INVENTORY - Lists all stored data with metadata
```

### Option 3: Add Menu Entry in Settings (~30 min)
Add UI entries in `components/cdc_os_ui` for:
- "Export all data" (triggers export, outputs via USB CDC)
- "Delete all data" (prompts confirmation, wipes all)

## References
- **Art. 15 DSGVO** - Right of access by the data subject
- **Art. 17 DSGVO** - Right to be forgotten (erasure)
- **Art. 20 DSGVO** - Right to data portability
- **ENISA Guidelines on Data Subject Rights** - https://www.enisa.europa.eu/publications/data-subject-rights
- **BfD (German Federal Data Protection) Checklist** - https://www.bfdi.bund.de/
