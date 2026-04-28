---
title: "[MEDIUM] NVS commit failures in FIDO2 storage silently ignored"
severity: MEDIUM
domain: error-path-tests
lens: storage-persistence
labels:
  - "nvs"
  - "data-persistence"
  - "fido2"
---

## Summary
In `components/mod_fido2/src/fido2_storage.cpp` (lines 413-415), NVS commit failures are logged as warnings but the error is not propagated to the caller. This means FIDO2 credential updates may appear to succeed but are actually lost after power cycle.

**Files:**
- `components/mod_fido2/src/fido2_storage.cpp:413-415`
- `components/mod_fido2/src/fido2_storage.cpp:391-415`

## Impact
1. **Data Loss**: FIDO2 credentials appear saved but are lost on reboot
2. **Silent Failure**: User thinks credential was added but it's not persisted
3. **Inconsistent State**: Counter values may be lost, causing authentication failures
4. **Debug Difficulty**: No obvious error message tells user what went wrong
5. **Security**: Credential slot might show as "used" but actually empty

## Evidence
From `components/mod_fido2/src/fido2_storage.cpp`:

```cpp
// Lines 391-415: saveCredentials() - NVS commit failure ignored
esp_err_t saveCredentials(const fido2_credential_t* cred) {
    // ... NVS open ...
    esp_err_t err = nvs_set_blob(nvs, "cred", cred, sizeof(fido2_credential_t));
    if (err != ESP_OK) {
        LOG_E("FIDO2", "Failed to set credential blob: %s", esp_err_to_name(err));
        return err;
    }
    
    // Update counter
    err = nvs_set_u32(nvs, "counter", cred->counter);
    if (err != ESP_OK) {
        LOG_E("FIDO2", "Failed to set counter: %s", esp_err_to_name(err));
        return err;
    }
    
    // Commit changes
    err = nvs_commit(nvs);
    if (err != ESP_OK) {
        LOG_W("FIDO2", "NVS commit failed: %s", esp_err_to_name(err));
        // ❌ Returns ESP_OK anyway!
        return ESP_OK;  // Caller thinks it succeeded!
    }
    
    nvs_close(nvs);
    return ESP_OK;
}

// Lines 352-365: loadCredentials() - similar issue with read failures
esp_err_t loadCredentials(fido2_credential_t* cred) {
    // ... open NVS ...
    size_t size = sizeof(fido2_credential_t);
    esp_err_t err = nvs_get_blob(nvs, "cred", cred, &size);
    if (err != ESP_OK) {
        LOG_W("FIDO2", "Failed to read credential: %s", esp_err_to_name(err));
        // Returns ESP_OK, caller gets garbage data
        return ESP_OK;
    }
    // ...
}
```

**Problem:**
- Line 413-415: `nvs_commit()` failure logged but `ESP_OK` returned
- Line 355-365: `nvs_get_blob()` failure logged but `ESP_OK` returned
- Callers assume success when it actually failed

## Recommended Fix
Propagate NVS errors to callers:

1. **Fix saveCredentials() return value**:
   ```cpp
   esp_err_t saveCredentials(const fido2_credential_t* cred) {
       // ... existing code ...
       
       esp_err_t err = nvs_commit(nvs);
       if (err != ESP_OK) {
           LOG_E("FIDO2", "NVS commit failed: %s", esp_err_to_name(err));
           nvs_close(nvs);
           return err;  // Return actual error
       }
       
       nvs_close(nvs);
       return ESP_OK;
   }
   ```

2. **Fix loadCredentials() to return error**:
   ```cpp
   esp_err_t loadCredentials(fido2_credential_t* cred) {
       // ... open NVS ...
       
       size_t size = sizeof(fido2_credential_t);
       esp_err_t err = nvs_get_blob(nvs, "cred", cred, &size);
       if (err != ESP_OK) {
           LOG_E("FIDO2", "Failed to read credential: %s", esp_err_to_name(err));
           nvs_close(nvs);
           return err;  // Return actual error
       }
       
       // ... rest of code ...
       nvs_close(nvs);
       return ESP_OK;
   }
   ```

3. **Update callers to handle errors**:
   ```cpp
   // In Fido2Module.cpp or wherever saveCredentials is called
   esp_err_t err = saveCredentials(cred);
   if (err != ESP_OK) {
       LOG_E("FIDO2", "Failed to save credential: %s", esp_err_to_name(err));
       // Show error to user via display/USB
       return err;
   }
   LOG_I("FIDO2", "Credential saved successfully");
   ```

4. **Add test cases**:
   - Mock NVS to simulate commit failure
   - Verify error is returned and propagated
   - Verify user gets clear error message

## References
- ESP-IDF NVS Documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/storage/nvs_flash.html
- NVS Error Codes: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/storage/nvs_flash.html#error-codes
- FIDO2 CTAP2 Specification: https://fidoalliance.org/specs/fido2-web-authentication-client-to-authenticator-protocol-client-to-authenticator-protocol-v2.1-rd-20191217.html

</content>