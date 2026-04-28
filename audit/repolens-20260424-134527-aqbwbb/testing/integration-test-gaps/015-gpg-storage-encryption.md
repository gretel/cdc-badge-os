---
title: "[MEDIUM] GPG storage encryption with PIN-derived key lacks integration tests"
severity: MEDIUM
domain: security
lens: integration-test-gaps
labels:
  - "audit:testing/integration-test-gaps"
  - "component:mod_gpg"
  - "area:storage"
---

## Summary
The `GpgStorage` component (`components/mod_gpg/src/GpgStorage.cpp`) encrypts the DEC private key in R-Memory using a PIN-derived HKDF key, but **no integration tests** verify the complete encryption/decryption workflow with actual PIN derivation and storage.

## Impact
- **Encryption bugs**: DEC key may not be encrypted correctly
- **PIN derivation**: HKDF derivation may produce wrong keys
- **Storage round-trip**: Encrypted data may not survive write/read cycle
- **Session state**: Session key caching may leak or expire incorrectly

## Evidence

**GpgStorage encryption API** (`components/mod_gpg/src/GpgStorage.cpp:28-150`):
```cpp
struct DecKeyStorage {
    uint8_t magic[4];        // "ECDH"
    uint8_t nonce[12];       // AES-GCM nonce
    uint8_t encrypted[32];   // Encrypted private key
    uint8_t tag[16];         // GCM authentication tag
};

static bool derive_device_key(uint8_t* key_out);
static bool encrypt_dec_key(const uint8_t* decKey, uint8_t* storage);
static bool decrypt_dec_key(const uint8_t* storage, uint8_t* decKey);
```

**PIN-based encryption** (`components/mod_gpg/src/GpgStorage.cpp:95-130`):
```cpp
static bool derive_device_key(uint8_t* key_out) {
    // Get chip ID as IKM
    uint8_t chip_id[16] = {};
    se->getChipId(chip_id);
    
    // Get PIN hash as salt
    PinManager& pm = PinManager::instance();
    uint8_t pinHash[16];
    pm.getBadgePinHash(pinHash);
    
    // HKDF-SHA256 to derive 32-byte key
    mbedtls_hkdf(..., chip_id, pinHash, ...);
}
```

**Storage operations** (`components/mod_gpg/src/GpgStorage.cpp:200-300`):
```cpp
bool storeDecKey(uint8_t slot, const uint8_t* decKey) {
    derive_device_key(sessionKey_);
    encrypt_dec_key(decKey, &decKeyStorage_);
    se->rmemWrite(slot, &decKeyStorage_, sizeof(DecKeyStorage));
}

bool loadDecKey(uint8_t slot, uint8_t* decKey) {
    se->rmemRead(slot, &decKeyStorage_, sizeof(DecKeyStorage));
    derive_device_key(sessionKey_);
    decrypt_dec_key(&decKeyStorage_, decKey);
}
```

**Usage in GpgModule** (`components/mod_gpg/src/GpgModule.cpp:200-400`):
```cpp
// Module uses storage for ECDH operations
GpgStorage::storeDecKey(slot, decKey);
GpgStorage::loadDecKey(slot, decKey);
```

**Current test coverage**: None

## Recommended Fix

Create integration test `test_gpg_storage_encryption/` that verifies:

1. **PIN derivation**: HKDF derives correct key from PIN + chip ID
2. **Encryption**: DEC key encrypts with AES-GCM correctly
3. **Decryption**: Encrypted key decrypts to original
4. **Storage round-trip**: Write to R-Memory and read back
5. **PIN change**: Key re-encrypts when PIN changes
6. **Session caching**: Session key reused within session

**Test structure** (example):
```cpp
// test/test_gpg_storage_encryption/test_dec_key.cpp
#include "mod_gpg/GpgStorage.h"
#include "cdc_core/PinManager.h"

void test_dec_key_encryption_roundtrip() {
    // Initialize
    PinManager& pm = PinManager::instance();
    pm.init();
    
    uint8_t decKey[32] = {0};
    esp_fill_random(decKey, 32);
    
    // Encrypt
    uint8_t storage[48];
    bool success = GpgStorage::encryptDecKey(decKey, storage);
    ASSERT_TRUE(success);
    
    // Decrypt
    uint8_t decrypted[32];
    success = GpgStorage::decryptDecKey(storage, decrypted);
    ASSERT_TRUE(success);
    
    // Verify
    ASSERT_EQ(memcmp(decKey, decrypted, 32), 0);
}

void test_dec_key_storage_roundtrip() {
    uint8_t decKey[32];
    esp_fill_random(decKey, 32);
    
    // Store to R-Memory
    GpgStorage::storeDecKey(1, decKey);
    
    // Load from R-Memory
    uint8_t loaded[32];
    GpgStorage::loadDecKey(1, loaded);
    
    ASSERT_EQ(memcmp(decKey, loaded, 32), 0);
}
```

## References
- [GpgStorage implementation](components/mod_gpg/src/GpgStorage.cpp)
- [PinManager for PIN hash](components/cdc_core/src/PinManager.cpp)
- [HKDF/MbedTLS](components/mod_gpg/src/GpgStorage.cpp:13-20)

</content>