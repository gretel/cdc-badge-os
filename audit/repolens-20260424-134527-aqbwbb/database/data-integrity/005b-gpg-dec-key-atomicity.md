---
title: "[HIGH] GPG DEC key storage lacks atomicity for encrypted private key"
severity: HIGH
domain: data-integrity
lens: database
labels:
  - gpg-storage
  - encryption
  - atomicity
  - private-key
---

## Summary
The GPG DEC (decryption) private key is stored encrypted in R-Memory using AES-256-GCM. The storage format (magic + nonce + encrypted key + tag = 64 bytes) is written in a single `rmemWrite()` call, but there's no versioning or sequence number to detect partial writes, corruption, or stale data.

**Files:**
- `components/mod_gpg/src/GpgStorage.cpp:241-322` (save_dec_privkey function)
- `components/mod_gpg/src/GpgStorage.cpp:329-406` (load_dec_privkey function)
- `components/mod_gpg/src/GpgStorage.cpp:53-66` (DecKeyStorage structure)

## Impact
If power is lost during the write operation:
- The R-Memory slot could contain partial/corrupted encrypted data
- The magic bytes might be correct but the encrypted data or tag could be stale
- On load, `gpg_storage_load_dec_privkey()` might succeed with corrupted data (if magic matches and AES-GCM tag happens to validate)

The current validation only checks magic bytes (`memcmp(storage->magic, DEC_KEY_MAGIC, MAGIC_SIZE)`) before attempting decryption. If magic is correct but other fields are corrupted, AES-GCM should detect it via the tag, but there's no version field to detect "stale" data from a previous save that wasn't fully overwritten.

## Evidence
From `GpgStorage.cpp:53-66`:
```cpp
#pragma pack(push, 1)
struct DecKeyStorage {
    uint8_t magic[MAGIC_SIZE];     // "ECDH"
    uint8_t nonce[NONCE_SIZE];     // AES-GCM nonce
    uint8_t encrypted[PRIVKEY_SIZE]; // Encrypted private key
    uint8_t tag[TAG_SIZE];         // GCM authentication tag
};
#pragma pack(pop)

static_assert(sizeof(DecKeyStorage) == TOTAL_SIZE, "DecKeyStorage size mismatch");
```

From `save_dec_privkey()` at line 241-322:
```cpp
// Erase existing data first
se->rmemErase(rmem_slot);

// Write encrypted key to R-Memory
if (se->rmemWrite(rmem_slot, reinterpret_cast<uint8_t*>(&storage), TOTAL_SIZE)
        != cdc::hal::SeResult::OK) {
    LOG_E(TAG, "Failed to write encrypted DEC key to R-Memory slot %d", rmem_slot);
    goto cleanup;
}
```

From `load_dec_privkey()` at line 329-406:
```cpp
// Verify magic
if (memcmp(storage->magic, DEC_KEY_MAGIC, MAGIC_SIZE) != 0) {
    LOG_W(TAG, "Invalid magic in DEC key storage");
    return false;
}
```

The validation only checks magic bytes. If magic is correct, it proceeds to decrypt. AES-GCM tag validation will catch most corruption, but:
1. No version field to detect schema changes
2. No sequence number to detect stale partial writes
3. No checksum for quick validation before decryption

## Recommended Fix
Add a version/sequence number to the storage format:

```cpp
#pragma pack(push, 1)
struct DecKeyStorage {
    uint8_t magic[MAGIC_SIZE];     // "ECDH"
    uint8_t version;               // Schema version (1 = current)
    uint8_t sequence;              // Write sequence (wraps at 255)
    uint8_t nonce[NONCE_SIZE];     // AES-GCM nonce
    uint8_t encrypted[PRIVKEY_SIZE]; // Encrypted private key
    uint8_t tag[TAG_SIZE];         // GCM authentication tag
    uint8_t reserved[2];           // Padding for alignment
};
// Total: 4 + 1 + 1 + 12 + 32 + 16 + 2 = 68 bytes
#pragma pack(pop)

// On save:
static uint8_t g_write_sequence = 0;
storage.version = 1;
storage.sequence = ++g_write_sequence;

// On load:
if (memcmp(storage->magic, DEC_KEY_MAGIC, MAGIC_SIZE) != 0) {
    return false;
}
if (storage->version != 1) {
    LOG_W(TAG, "Unknown DEC key version: %d", storage->version);
    return false;
}
// Optionally track sequence for debugging:
LOG_D(TAG, "Loaded DEC key (sequence: %d)", storage->sequence);
```

Alternatively, use the existing R-Memory header checksum if available:
```cpp
// Use rmemWriteWithHeader for built-in checksum
se->rmemWriteWithHeader(rmem_slot, moduleId, "ECDH_KEY", 0, 
                        reinterpret_cast<uint8_t*>(&storage), TOTAL_SIZE);
```

## References
- AES-GCM provides integrity via authentication tag, but versioning helps detect schema changes
- See `ISecureElement.h:183-195` for `rmemWriteWithHeader` helper
- GPG storage already uses magic bytes (line 32-33, 357-360)
