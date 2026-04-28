---
title: "[011] [LOW] Use of goto for cleanup in C++"
severity: LOW
domain: Code Readability
lens: code-quality/readability
labels:
  - "audit:code-quality/readability"
---

## Summary
The code uses `goto` statements for cleanup in C++ functions, which is less idiomatic than RAII or early returns.

## Impact
- `goto` is uncommon in modern C++ and can confuse readers
- C++ has better mechanisms (RAII, scope-based cleanup)
- Makes control flow harder to follow

## Evidence
**File: `components/mod_gpg/src/GpgStorage.cpp:279-320`**
```cpp
bool gpg_storage_save_dec_privkey(const uint8_t* privkey, const char* pin) {
    // ... setup code ...

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);

    bool success = false;

    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, enc_key, 256);
    if (ret != 0) {
        LOG_E(TAG, "GCM setkey failed: %d", ret);
        goto cleanup;  // GOTO #1
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
        goto cleanup;  // GOTO #2
    }

    // Erase and write to R-Memory
    se->rmemErase(rmem_slot);
    if (se->rmemWrite(rmem_slot, reinterpret_cast<uint8_t*>(&storage), TOTAL_SIZE)
            != cdc::hal::SeResult::OK) {
        LOG_E(TAG, "Failed to write encrypted DEC key to R-Memory slot %d", rmem_slot);
        goto cleanup;  // GOTO #3
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

Same pattern in `gpg_storage_load_dec_privkey()` at line 378.

## Recommended Fix
Use early returns with a scope-based cleanup block:

```cpp
bool gpg_storage_save_dec_privkey(const uint8_t* privkey, const char* pin) {
    // ... setup code ...

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    
    // Define cleanup scope
    auto cleanup = [&]() {
        mbedtls_gcm_free(&gcm);
        mbedtls_platform_zeroize(enc_key, sizeof(enc_key));
        mbedtls_platform_zeroize(&storage, sizeof(storage));
    };

    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, enc_key, 256);
    if (ret != 0) {
        LOG_E(TAG, "GCM setkey failed: %d", ret);
        cleanup();
        return false;
    }

    ret = mbedtls_gcm_crypt_and_tag(...);
    if (ret != 0) {
        LOG_E(TAG, "GCM encrypt failed: %d", ret);
        cleanup();
        return false;
    }

    // Erase and write to R-Memory
    se->rmemErase(rmem_slot);
    if (se->rmemWrite(rmem_slot, reinterpret_cast<uint8_t*>(&storage), TOTAL_SIZE)
            != cdc::hal::SeResult::OK) {
        LOG_E(TAG, "Failed to write encrypted DEC key to R-Memory slot %d", rmem_slot);
        cleanup();
        return false;
    }

    LOG_I(TAG, "Saved encrypted DEC private key to R-Memory slot %d", rmem_slot);
    cleanup();
    return true;
}
```

Or better, use RAII wrappers around mbedtls contexts:
```cpp
struct GcmContext {
    mbedtls_gcm_context ctx;
    GcmContext() { mbedtls_gcm_init(&ctx); }
    ~GcmContext() { mbedtls_gcm_free(&ctx); }
};

// Then no manual cleanup needed!
```

## References
- Effective Modern C++: Item 13 - Use RAII for resource management
- C++ Core Guidelines: RA.1 - Use RAII for resource acquisition
