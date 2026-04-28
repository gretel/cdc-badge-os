---
title: "[MEDIUM] User Data Stored Indefinitely Without Expiration"
severity: MEDIUM
domain: compliance
lens: data-retention
labels:
  - "audit:compliance/data-retention"
---

## Summary
All user-generated data (FIDO2 credentials, TOTP secrets, password entries, GPG keys) is stored indefinitely with no mechanism for automatic expiration or archival. Data persists until explicitly deleted by the user.

**Location**:
- FIDO2: `components/mod_fido2/src/fido2_storage.cpp` (lines 32-43, struct `fido2_stored_cred_t`)
- TOTP: `components/mod_totp/src/TotpStore.cpp` (lines 15-21, struct `TotpPayload`)
- Passwords: `components/mod_password/src/PasswordStore.cpp` (lines 14-21, struct `PasswordPayload`)
- GPG: `components/mod_gpg/src/GpgStorage.cpp` (lines 432-455, key storage functions)

## Impact
1. **Storage Bloat**: Limited TROPIC01 R-Memory (512 slots) can be exhausted over time
2. **Orphaned Data**: Users may forget credentials/accounts, leaving dead data
3. **Privacy**: Old credentials remain accessible even if no longer needed
4. **No Lifecycle**: Missing distinction between "active" and "inactive" data

## Evidence
**FIDO2 credentials** (`fido2_storage.cpp:32-43`):
```cpp
typedef struct {
    uint8_t magic[FIDO2_RMEM_MAGIC_LEN];    // "FIDO2"
    uint8_t rp_id_hash[32];                 // SHA-256 of RP ID
    char rp_id[FIDO2_RP_ID_MAX_LEN];        // RP ID string
    uint8_t user_id[FIDO2_USER_ID_MAX_LEN]; // User handle
    uint8_t user_id_len;
    char user_name[FIDO2_USER_NAME_MAX_LEN];// Display name
    uint8_t cred_id_nonce[16];              // Random nonce
    uint8_t flags;
    uint8_t cred_protect;
    uint8_t curve;
    uint8_t reserved[7];                    // Reserved for future use
} fido2_stored_cred_t;
```
No `created_at`, `expires_at`, or `last_used` timestamp fields.

**TOTP accounts** (`TotpStore.cpp:15-21`):
```cpp
struct TotpPayload {
    char issuer[TotpStore::ISSUER_LEN];
    uint8_t secret[TotpStore::SECRET_LEN];
    uint8_t secretLen;
    uint8_t digits;
    uint32_t period;
    uint8_t algorithm;
    uint8_t flags;
};
```
No timestamp fields for creation or expiration.

**Password entries** (`PasswordStore.cpp:14-21`):
```cpp
struct PasswordPayload {
    char title[PasswordStore::TITLE_LEN];
    char username[PasswordStore::USERNAME_LEN];
    char password[PasswordStore::PASSWORD_LEN];
    char url[PasswordStore::URL_LEN];
    uint8_t totpSlot;
    char notes[PasswordStore::NOTES_LEN];
};
```
No metadata for creation date, last access, or expiration.

## Recommended Fix
Add optional timestamp/metadata fields to each data structure:

1. **Add `created_at` field** (Unix timestamp, 4 bytes) to each payload struct
2. **Add optional `expires_at` field** (4 bytes, 0 = no expiry)
3. **Add `last_used` field** for activity tracking (optional)

Example for FIDO2:
```cpp
typedef struct {
    uint8_t magic[FIDO2_RMEM_MAGIC_LEN];
    uint32_t created_at;          // New: creation timestamp
    uint32_t expires_at;          // New: 0 = no expiry
    uint8_t rp_id_hash[32];
    // ... existing fields ...
    uint8_t reserved[3];          // Adjust for new fields
} fido2_stored_cred_t;
```

Then implement query/filter functions to list "expired" or "inactive" credentials.

## References
- GDPR Article 5(1)(e) - Storage limitation
- ISO/IEC 27001 - Information security management
