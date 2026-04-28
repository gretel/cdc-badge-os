---
title: "[HIGH] Multiple unchecked nvs_get_blob calls in openpgp.c with potential typo"
severity: HIGH
domain: module-gpg
lens: error-handling
labels:
  - "audit:error-handling/unhandled-errors"
---

## Summary
In `components/mod_gpg/src/openpgp/openpgp.cpp`, the `load_fingerprints()` function has 7 consecutive `nvs_get_blob()` calls without checking return values (lines 499-519). Additionally, there's a potential typo on line 512 where `ca_fp_1` is used instead of `ca_fp_2`.

## Impact
1. **Silent data corruption**: If any `nvs_get_blob()` fails, the output buffers contain uninitialized data that may be used as valid fingerprints
2. **Typo bug**: Line 512 reads into `ca_fp_1` twice, so `ca_fp_2` never gets loaded
3. **No error recovery**: Failed NVS reads don't trigger re-initialization or user notification
4. **Hard to debug**: GPG operations fail with seemingly random "key not found" errors

## Evidence
File: `components/mod_gpg/src/openpgp/openpgp.cpp:488-520`

```cpp
nvs_handle_t nvs;
if (nvs_open("gpg_nvs", NVS_READONLY, &nvs) == ESP_OK) {
    size_t len = 20;
    nvs_get_blob(nvs, "fp_sig", fingerprint_sig, &len);  // LINE 499 - UNCHECKED
    len = 20;
    nvs_get_blob(nvs, "fp_dec", fingerprint_dec, &len);  // LINE 502 - UNCHECKED
    len = 20;
    nvs_get_blob(nvs, "fp_aut", fingerprint_aut, &len);  // LINE 505 - UNCHECKED
    len = 20;
    nvs_get_blob(nvs, "ca_fp_1", ca_fp_1, &len);        // LINE 509 - UNCHECKED
    len = 20;
    nvs_get_blob(nvs, "ca_fp_1", ca_fp_1, &len);        // LINE 512 - TYPO: should be ca_fp_2, UNCHECKED
    len = 20;
    nvs_get_blob(nvs, "ca_fp_3", ca_fp_3, &len);        // LINE 515 - UNCHECKED
    len = 8;
    nvs_get_blob(nvs, "gt_sig", gen_time_sig, &len);    // LINE 519 - UNCHECKED
    nvs_close(nvs);
}
```

Note line 512: `nvs_get_blob(nvs, "ca_fp_1", ca_fp_1, &len)` - the key name and destination are both `ca_fp_1`, but this should be `ca_fp_2`.

## Recommended Fix
Add error checking and fix the typo:

```cpp
nvs_handle_t nvs;
if (nvs_open("gpg_nvs", NVS_READONLY, &nvs) == ESP_OK) {
    size_t len = 20;
    if (nvs_get_blob(nvs, "fp_sig", fingerprint_sig, &len) == ESP_OK && len == 20) {
        // Valid sig fingerprint loaded
    }
    len = 20;
    if (nvs_get_blob(nvs, "fp_dec", fingerprint_dec, &len) == ESP_OK && len == 20) {
        // Valid dec fingerprint loaded
    }
    len = 20;
    if (nvs_get_blob(nvs, "fp_aut", fingerprint_aut, &len) == ESP_OK && len == 20) {
        // Valid aut fingerprint loaded
    }
    len = 20;
    if (nvs_get_blob(nvs, "ca_fp_1", ca_fp_1, &len) == ESP_OK && len == 20) {
        // Valid CA fingerprint 1 loaded
    }
    len = 20;
    if (nvs_get_blob(nvs, "ca_fp_2", ca_fp_2, &len) == ESP_OK && len == 20) {  // FIXED: was ca_fp_1
        // Valid CA fingerprint 2 loaded
    }
    len = 20;
    if (nvs_get_blob(nvs, "ca_fp_3", ca_fp_3, &len) == ESP_OK && len == 20) {
        // Valid CA fingerprint 3 loaded
    }
    len = 8;
    if (nvs_get_blob(nvs, "gt_sig", gen_time_sig, &len) == ESP_OK && len == 8) {
        // Valid generation time signature loaded
    }
    nvs_close(nvs);
}
```

## References
- ESP-IDF NVS API: `nvs_get_blob()` returns `esp_err_t`
- NVS blob storage: size can change between writes
