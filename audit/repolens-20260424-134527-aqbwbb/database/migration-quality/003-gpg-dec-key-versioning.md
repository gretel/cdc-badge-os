---
title: "[MEDIUM] GPG DEC key encryption lacks versioning for algorithm upgrades"
severity: MEDIUM
domain: database/migration-quality
lens: embedded-storage
labels:
  - "encryption"
  - "key-storage"
---

## Summary
In `components/mod_gpg/src/GpgStorage.cpp`, the encrypted DEC key storage format uses a fixed structure without version information:

```cpp
#pragma pack(push, 1)
struct DecKeyStorage {
    uint8_t magic[MAGIC_SIZE];     // "ECDH"
    uint8_t nonce[NONCE_SIZE];     // AES-GCM nonce
    uint8_t encrypted[PRIVKEY_SIZE]; // Encrypted private key
    uint8_t tag[TAG_SIZE];         // GCM authentication tag
};
#pragma pack(pop)
```

The magic bytes `"ECDH"` (line 43) serve only as a presence marker, not a version identifier. If the encryption algorithm changes (e.g., switching from AES-256-GCM to a different cipher, or changing HKDF parameters), existing keys cannot be migrated.

## Impact
- **Algorithm lock-in**: Once an encryption scheme is chosen, it cannot be upgraded without forcing all users to re-enter their GPG keys.
- **Security debt**: If a weakness is found in the current scheme, there's no path to upgrade existing keys.
- **PIN change limitations**: Changing the PIN derivation logic requires re-encrypting all keys, but there's no mechanism to detect and handle version mismatches.

## Evidence
File: `components/mod_gpg/src/GpgStorage.cpp:43-58`

The storage structure is 44 bytes total:
- 4 bytes: magic ("ECDH")
- 12 bytes: nonce
- 32 bytes: encrypted key
- 16 bytes: tag

In `gpg_storage_save_dec_privkey()` (line 229-326), the structure is written directly without any version field:
```cpp
if (se->rmemWrite(rmem_slot, reinterpret_cast<uint8_t*>(&storage), TOTAL_SIZE)
        != cdc::hal::SeResult::OK) {
```

In `gpg_storage_load_dec_privkey()` (line 330-404), only the magic is checked:
```cpp
if (memcmp(storage->magic, DEC_KEY_MAGIC, MAGIC_SIZE) != 0) {
    LOG_W(TAG, "Invalid magic in DEC key storage");
    return false;
}
```

## Recommended Fix
Add versioning to the encrypted key format:

1. **Add version byte**: Prepend a version field to the structure:
   ```cpp
   struct DecKeyStorage {
       uint8_t version;         // 0x01 = current format
       uint8_t magic[3];        // "ECD" (adjusted for alignment)
       uint8_t nonce[NONCE_SIZE];
       uint8_t encrypted[PRIVKEY_SIZE];
       uint8_t tag[TAG_SIZE];
   };
   ```

2. **Update load function**: Check version and dispatch to appropriate decryption:
   ```cpp
   if (storage.version == 0x01) {
       // Current decryption logic
   } else if (storage.version == 0x00) {
       // Legacy format - decrypt and re-encrypt with new format
   }
   ```

3. **Add migration on PIN change**: When the PIN is changed, detect old format and upgrade:
   ```cpp
   bool needsMigration = (storage.version != CURRENT_VERSION);
   if (needsMigration) {
       // Decrypt with old key, re-encrypt with new
   }
   ```

## References
- AES-GCM encryption: https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-38d.pdf
- Key derivation best practices: https://cheatsheetseries.owasp.org/cheatsheets/Key_Chen_Cheatsheet.html
