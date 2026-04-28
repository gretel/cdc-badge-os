---
title: "[LOW] GPG metadata stored in NVS without integrity protection"
severity: LOW
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
GPG metadata (user ID, fingerprint, key slots, creation time) is stored in NVS without integrity protection. An attacker with NVS access could modify the metadata to change the user ID, fingerprint, or other attributes.

**File**: `components/mod_gpg/src/gpg.cpp:110-128`
```cpp
static bool load_metadata(void) {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    size_t len = sizeof(gpg_metadata_t);
    esp_err_t err = nvs_get_blob(handle, NVS_KEY_META, &s_metadata, &len);
    nvs_close(handle);
    if (err != ESP_OK || len != sizeof(gpg_metadata_t)) {
        memset(&s_metadata, 0, sizeof(s_metadata));
        return false;
    }
    if (s_metadata.magic != GPG_METADATA_MAGIC || s_metadata.version != GPG_METADATA_VERSION) {
        memset(&s_metadata, 0, sizeof(s_metadata));
        return false;
    }

    return true;
}
```

The metadata is loaded from NVS with only magic and version checks, no integrity verification.

**File**: `components/mod_gpg/src/gpg.cpp:133-149`
```cpp
static bool save_metadata(void) {
    s_metadata.magic = GPG_METADATA_MAGIC;
    s_metadata.version = GPG_METADATA_VERSION;
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        return false;
    }
    esp_err_t err = nvs_set_blob(handle, NVS_KEY_META, &s_metadata, sizeof(s_metadata));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err == ESP_OK;
}
```

Metadata is saved without any signature or hash for integrity.

## Impact
- **Metadata Tampering**: An attacker could modify the user ID to impersonate a different identity
- **Fingerprint Spoofing**: The fingerprint could be changed to match a different key
- **Key Slot Modification**: An attacker could change which ECC slots are used for keys
- **Timestamp Manipulation**: Creation time could be modified to make keys appear older/newer

## Evidence
**File**: `components/mod_gpg/src/gpg.cpp:110-128`
```cpp
if (s_metadata.magic != GPG_METADATA_MAGIC || s_metadata.version != GPG_METADATA_VERSION) {
    memset(&s_metadata, 0, sizeof(s_metadata));
    return false;
}
return true;  // No integrity check beyond magic/version
```

**File**: `components/mod_gpg/src/gpg.cpp:133-149`
```cpp
esp_err_t err = nvs_set_blob(handle, NVS_KEY_META, &s_metadata, sizeof(s_metadata));
```

The blob is saved as-is without any HMAC or signature.

**File**: `components/mod_gpg/include/mod_gpg/GpgStorage.h`
Metadata structure:
```cpp
typedef struct {
    uint8_t magic[8];
    uint8_t version;
    uint8_t curve;
    char user_id[64];
    uint8_t fingerprint[20];
    uint8_t fingerprint_v5[32];
    uint32_t created_at;
    uint8_t pubkey[64];
    uint8_t pubkey_len;
    uint32_t sign_count;
    uint8_t padding[16];
} gpg_metadata_t;
```

All fields are plain data without integrity protection.

## Recommended Fix
Add HMAC-based integrity protection:

```cpp
// Add to metadata structure
typedef struct {
    uint8_t magic[8];
    uint8_t version;
    uint8_t curve;
    char user_id[64];
    uint8_t fingerprint[20];
    uint8_t fingerprint_v5[32];
    uint32_t created_at;
    uint8_t pubkey[64];
    uint8_t pubkey_len;
    uint32_t sign_count;
    uint8_t padding[16];
    uint8_t hmac[32];  // HMAC-SHA256 of all fields above
} gpg_metadata_t;

// Master key stored in ECC slot (e.g., slot 5)
#define GPG_HMAC_SLOT 5

static uint8_t get_hmac_key(uint8_t *key) {
    auto* se = cdc::hal::getSecureElementInstance();
    if (!se) return false;
    return se->eccRead(GPG_HMAC_SLOT, key, 32) == cdc::hal::SeResult::OK;
}

static bool compute_metadata_hmac(gpg_metadata_t *meta) {
    uint8_t key[32];
    if (!get_hmac_key(key)) return false;

    // Compute HMAC over all fields except the HMAC itself
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&ctx, key, 32);
    mbedtls_md_hmac_update(&ctx, (uint8_t*)meta, sizeof(gpg_metadata_t) - 32);
    mbedtls_md_hmac_finish(&ctx, meta->hmac);
    mbedtls_md_free(&ctx);

    return true;
}

static bool verify_metadata_hmac(gpg_metadata_t *meta) {
    uint8_t expected_hmac[32];
    memcpy(expected_hmac, meta->hmac, 32);

    // Compute expected HMAC
    if (!compute_metadata_hmac(meta)) return false;

    // Compare in constant time
    return memcmp(meta->hmac, expected_hmac, 32) == 0;
}

static bool load_metadata(void) {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    size_t len = sizeof(gpg_metadata_t);
    esp_err_t err = nvs_get_blob(handle, NVS_KEY_META, &s_metadata, &len);
    nvs_close(handle);
    if (err != ESP_OK || len != sizeof(gpg_metadata_t)) {
        memset(&s_metadata, 0, sizeof(s_metadata));
        return false;
    }
    if (s_metadata.magic != GPG_METADATA_MAGIC || s_metadata.version != GPG_METADATA_VERSION) {
        memset(&s_metadata, 0, sizeof(s_metadata));
        return false;
    }

    // Verify HMAC
    if (!verify_metadata_hmac(&s_metadata)) {
        memset(&s_metadata, 0, sizeof(s_metadata));
        return false;
    }

    return true;
}

static bool save_metadata(void) {
    // Compute HMAC before saving
    if (!compute_metadata_hmac(&s_metadata)) return false;

    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        return false;
    }
    esp_err_t err = nvs_set_blob(handle, NVS_KEY_META, &s_metadata, sizeof(s_metadata));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err == ESP_OK;
}
```

## References
- NIST SP 800-107 - HMAC for Integrity
- ESP32 NVS Documentation
- CWE-354: Improper Verification of Integrity

</content>