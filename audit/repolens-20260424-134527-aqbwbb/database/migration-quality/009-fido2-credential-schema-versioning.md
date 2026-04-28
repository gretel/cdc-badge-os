---
title: "[MEDIUM] FIDO2 credential storage uses reserved bytes instead of version field"
severity: MEDIUM
domain: database/migration-quality
lens: embedded-storage
labels:
  - "schema-evolution"
  - "fido2"
---

## Summary
In `components/mod_fido2/src/fido2_storage.cpp:28-43`, the FIDO2 credential storage structure uses a fixed-size reserved array instead of a proper version field:

```cpp
typedef struct {
    uint8_t magic[FIDO2_RMEM_MAGIC_LEN];    // "FID2"
    uint8_t rp_id_hash[32];
    char rp_id[FIDO2_RP_ID_MAX_LEN];
    uint8_t user_id[FIDO2_USER_ID_MAX_LEN];
    uint8_t user_id_len;
    char user_name[FIDO2_USER_NAME_MAX_LEN];
    uint32_t sign_count;
    uint8_t cred_id_nonce[16];
    uint8_t flags;
    uint8_t cred_protect;
    uint8_t curve;
    uint8_t reserved[7];                    // Reserved for future use
} fido2_stored_cred_t;
```

The 7 reserved bytes (line 41) are intended for "future use" but there's no documented version field or migration strategy. The magic bytes `"FID2"` only serve as a presence check, not a schema version identifier.

## Impact
- **Schema lock-in**: Once the structure is deployed, adding new fields requires either:
  - Using one of the 7 reserved bytes (limited space)
  - Changing the structure size, which breaks backward compatibility
- **No upgrade path**: If a new feature requires more than 7 bytes or a different field arrangement, all existing FIDO2 credentials must be re-created.
- **Ambiguous reserved usage**: Without a version field, code doesn't know which fields are valid in older credential records.

## Evidence
File: `components/mod_fido2/src/fido2_storage.cpp:28-43`

The structure is ~180 bytes total. In `read_rmem_credential()` (line 187-210), only the magic is validated:

```cpp
if (memcmp(tmp->magic, FIDO2_RMEM_MAGIC, FIDO2_RMEM_MAGIC_LEN) != 0) {
    return false;
}
memcpy(stored, tmp, sizeof(fido2_stored_cred_t));
```

In `create_credential()` (line 760-888), the structure is written directly:

```cpp
memcpy(stored.magic, FIDO2_RMEM_MAGIC, FIDO2_RMEM_MAGIC_LEN);
// ... populate fields ...
if (!write_rmem_credential(static_cast<uint8_t>(slot), &stored)) {
```

There's no version tracking or migration logic anywhere in the file.

## Recommended Fix
Implement schema versioning:

1. **Replace reserved with version**: Change the structure to include an explicit version field:
   ```cpp
   typedef struct {
       uint8_t version;                    // Schema version (0x01 = current)
       uint8_t magic[3];                   // "FID" (adjusted for alignment)
       uint8_t rp_id_hash[32];
       // ... rest of fields ...
       uint8_t flags;
       uint8_t cred_protect;
       uint8_t curve;
       uint8_t reserved[6];                // Reduced to accommodate version
   } fido2_stored_cred_t;
   ```

2. **Add version constants**:
   ```cpp
   #define FIDO2_SCHEMA_VERSION_V1 0x01
   #define FIDO2_CURRENT_VERSION   FIDO2_SCHEMA_VERSION_V1
   ```

3. **Update read function**: Check version and handle migrations:
   ```cpp
   static bool read_rmem_credential(uint8_t logical_slot, fido2_stored_cred_t* stored) {
       // ... read data ...
       if (memcmp(tmp->magic, FIDO2_RMEM_MAGIC, 3) != 0) {
           return false;
       }
       
       // Handle version migrations
       if (tmp->version == FIDO2_SCHEMA_VERSION_V1) {
           memcpy(stored, tmp, sizeof(fido2_stored_cred_t));
           return true;
       } else if (tmp->version == 0x00) {
           // Legacy format - migrate to V1
           return migrate_v0_to_v1(tmp, stored);
       }
       return false;
   }
   ```

4. **Update create function**: Write version field:
   ```cpp
   stored.version = FIDO2_CURRENT_VERSION;
   memcpy(stored.magic, FIDO2_RMEM_MAGIC, 3);
   ```

## References
- FIDO2 credential format: https://www.w3.org/TR/webauthn-2/#sctn-authenticator-data
- Schema versioning patterns: https://martinfowler.com/articles/schema-evolution.html
