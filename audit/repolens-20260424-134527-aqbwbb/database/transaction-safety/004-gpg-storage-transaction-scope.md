---
title: "[MEDIUM] GPG storage lacks transaction scope for multi-field key generation"
severity: MEDIUM
domain: database/transaction-safety
lens: transaction-safety
labels:
  - "audit:database/transaction-safety"
---

## Summary

In `components/mod_gpg/src/GpgStorage.cpp`, the DEC private key is saved with encrypted payload to R-Memory, but the operation doesn't coordinate with the TROPIC01 ECC slot operations for SIG/DEC/AUT key generation. If ECC slots are generated first but the encrypted DEC key write fails, the GPG module is left in an inconsistent state.

**Affected location:** `gpg_storage_save_dec_privkey()` (lines 240-320)

## Impact

**Partial key generation scenario:**
1. Application generates ECC keys for SIG, DEC, AUT slots (3 separate `eccGenerate()` calls)
2. `gpg_storage_save_dec_privkey()` is called to store encrypted DEC private key
3. Encryption succeeds, but R-Memory write fails (lines 298-303)
4. Result: ECC slots contain private keys, but encrypted payload is missing
5. On reboot: `gpg_storage_has_dec_privkey()` returns false, but ECC slots are "used"

This creates a state where:
- The GPG module thinks DEC key should exist (slot is occupied)
- But the encrypted payload to reconstruct the key is missing
- Recovery requires manual slot clearing

## Evidence

**GpgStorage::save_dec_privkey()** (lines 240-320):
```cpp
bool gpg_storage_save_dec_privkey(const uint8_t* privkey, const char* pin) {
    if (!privkey) {
        LOG_E(TAG, "Invalid parameters for save_dec_privkey");
        return false;
    }

    auto* se = get_se();
    if (!se) {
        LOG_E(TAG, "Secure element not available");
        return false;
    }

    // Derive encryption key from PIN
    uint8_t enc_key[32];
    if (!derive_key_from_pin(pin, enc_key)) {
        LOG_E(TAG, "Failed to derive encryption key");
        return false;
    }

    // Prepare storage structure
    DecKeyStorage storage = {};
    memcpy(storage.magic, DEC_KEY_MAGIC, MAGIC_SIZE);

    // Calculate R-Memory slot early
    uint16_t rmem_slot = s_storage.rmemStart + RMEM_SLOT_DEC_KEY;

    // Generate random nonce
    if (!se->getRandom(storage.nonce, NONCE_SIZE)) {
        esp_fill_random(storage.nonce, NONCE_SIZE);
    }

    // Encrypt private key with AES-256-GCM
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);

    bool success = false;

    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, enc_key, 256);
    if (ret != 0) {
        LOG_E(TAG, "GCM setkey failed: %d", ret);
        goto cleanup;
    }

    ret = mbedtls_gcm_crypt_and_tag(
        &gcm, MBEDTLS_GCM_ENCRYPT, PRIVKEY_SIZE,
        storage.nonce, NONCE_SIZE,
        storage.magic, MAGIC_SIZE,
        privkey, storage.encrypted,
        TAG_SIZE, storage.tag
    );

    if (ret != 0) {
        LOG_E(TAG, "GCM encrypt failed: %d", ret);
        goto cleanup;
    }

    // Erase existing data first
    se->rmemErase(rmem_slot);

    // Write encrypted key to R-Memory
    if (se->rmemWrite(rmem_slot, reinterpret_cast<uint8_t*>(&storage), TOTAL_SIZE)
            != cdc::hal::SeResult::OK) {
        LOG_E(TAG, "Failed to write encrypted DEC key to R-Memory slot %d", rmem_slot);
        goto cleanup;  // ECC slot already contains key, but R-Memory is empty!
    }

    LOG_I(TAG, "Saved encrypted DEC private key to R-Memory slot %d", rmem_slot);
    success = true;

cleanup:
    mbedtls_gcm_free(&gcm);
    mbedtls_platform_zeroize(enc_key, sizeof(enc_key));
    mbedtls_platform_zeroize(&storage, sizeof(storage));

    return success;
}
```

The ECC slot generation happens externally (in `GpgModule.cpp` or `openpgp.cpp`), and there's no transaction wrapper to coordinate both operations.

## Recommended Fix

**Option 1: Atomic key generation function**
Add a combined function that generates ECC slot AND stores encrypted payload:
```cpp
bool gpg_storage_generate_dec_key(const char* pin, uint8_t* outPubKey, size_t outPubKeyLen) {
    auto* se = get_se();
    if (!se) return false;

    // Generate ECC key first
    uint8_t slot = gpg_storage_dec_slot();
    if (!se->eccGenerate(slot, EccCurve::P256)) {
        return false;
    }

    // Get public key
    cdc::hal::EccCurve curve;
    if (!se->eccGetPublicKey(slot, outPubKey, &curve)) {
        // Rollback: delete ECC slot
        se->eccDelete(slot);
        return false;
    }

    // Get private key (need to read from slot - TROPIC01 may not support this)
    // If TROPIC01 doesn't export private keys, use a different approach:
    // Generate key in software, import to TROPIC01, store encrypted copy

    // Derive encryption key
    uint8_t enc_key[32];
    if (!derive_key_from_pin(pin, enc_key)) {
        se->eccDelete(slot);  // Rollback
        return false;
    }

    // ... encrypt and store ...

    // If store fails, delete ECC slot
    if (!store_encrypted_privkey(...)) {
        se->eccDelete(slot);  // Rollback
        return false;
    }

    return true;
}
```

**Option 2: Two-phase initialization with validation**
Create a `gpg_storage_validate()` function that checks both ECC slots and R-Memory consistency:
```cpp
bool gpg_storage_validate(void) {
    auto* se = get_se();
    if (!se) return false;

    // Check ECC slots are initialized
    if (!se->eccSlotUsed(gpg_storage_sig_slot())) return false;
    if (!se->eccSlotUsed(gpg_storage_dec_slot())) return false;
    if (!se->eccSlotUsed(gpg_storage_aut_slot())) return false;

    // Check R-Memory has encrypted DEC key
    if (!gpg_storage_has_dec_privkey()) return false;

    return true;
}
```

Call this at module startup; if it fails, clear all slots and reinitialize.

## References

- TROPIC01 ECC slot operations: `eccGenerate()`, `eccImport()`, `eccDelete()`
- Two-phase commit pattern for distributed transactions
- GPG OpenPGP card key generation workflow