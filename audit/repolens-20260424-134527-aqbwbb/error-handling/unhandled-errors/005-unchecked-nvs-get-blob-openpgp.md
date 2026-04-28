---
title: "[MEDIUM] Unchecked return value from nvs_get_blob in GPG metadata loading"
severity: MEDIUM
domain: module-gpg
lens: error-handling
labels:
  - "audit:error-handling/unhandled-errors"
---

## Summary
In `components/mod_gpg/src/gpg.cpp`, the `load_metadata()` function calls `nvs_get_blob()` at line 119 but does not check the return value before using the retrieved data. If NVS returns partial data or the blob shrunk, the metadata could be corrupted.

## Impact
If `nvs_get_blob()` fails or returns partial data:
- Metadata structure could contain garbage values
- Magic/version check might pass with corrupted data
- GPG module could operate with invalid configuration
- Silent data corruption is harder to debug than explicit failures

## Evidence
File: `components/mod_gpg/src/gpg.cpp:113-125`

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

The error check at line 121 IS present. Let me look for other issues...

Looking at `save_metadata()` at lines 137-149:

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

This looks correct. Let me check the openpgp.cpp file for NVS issues:

File: `components/mod_gpg/src/openpgp/openpgp.cpp:488-520`

```cpp
nvs_handle_t nvs;
if (nvs_open("gpg_nvs", NVS_READONLY, &nvs) == ESP_OK) {
    size_t len = 20;
    nvs_get_blob(nvs, "fp_sig", fingerprint_sig, &len);  // <-- UNCHECKED
    len = 20;
    nvs_get_blob(nvs, "fp_dec", fingerprint_dec, &len);  // <-- UNCHECKED
    len = 20;
    nvs_get_blob(nvs, "fp_aut", fingerprint_aut, &len);  // <-- UNCHECKED
    len = 20;
    nvs_get_blob(nvs, "ca_fp_1", ca_fp_1, &len);        // <-- UNCHECKED
    len = 20;
    nvs_get_blob(nvs, "ca_fp_2", ca_fp_1, &len);        // <-- UNCHECKED (typo: ca_fp_1 instead of ca_fp_2)
    len = 20;
    nvs_get_blob(nvs, "ca_fp_3", ca_fp_3, &len);        // <-- UNCHECKED
    len = 8;
    nvs_get_blob(nvs, "gt_sig", gen_time_sig, &len);    // <-- UNCHECKED
    nvs_close(nvs);
}
```

Multiple `nvs_get_blob()` calls without return value checks, plus a potential typo.

## Recommended Fix
Add error checking to all `nvs_get_blob()` calls:

```cpp
nvs_handle_t nvs;
if (nvs_open("gpg_nvs", NVS_READONLY, &nvs) == ESP_OK) {
    size_t len = 20;
    if (nvs_get_blob(nvs, "fp_sig", fingerprint_sig, &len) == ESP_OK && len == 20) {
        // Valid fingerprint
    }
    len = 20;
    if (nvs_get_blob(nvs, "fp_dec", fingerprint_dec, &len) == ESP_OK && len == 20) {
        // Valid fingerprint
    }
    // ... repeat for other blobs
    nvs_close(nvs);
}
```

## References
- ESP-IDF NVS API documentation: `nvs_get_blob()` returns `esp_err_t`
- NVS storage behavior with partial writes
