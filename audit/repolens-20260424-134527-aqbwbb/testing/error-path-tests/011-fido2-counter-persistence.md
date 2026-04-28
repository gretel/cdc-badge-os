---
title: "[MEDIUM] FIDO2 credential count persistence on NVS commit failure"
severity: MEDIUM
domain: error-path-tests
lens: fido2-storage
labels:
  - "fido2"
  - "nvs"
  - "persistence"
---

## Summary
In `components/mod_fido2/src/fido2_storage.cpp` (lines 930-940), the sign count persistence function logs a CRITICAL error on NVS commit failure but doesn't propagate the error. This means credential counters may not persist across reboots, causing FIDO2 authentication to fail.

**Files:**
- `components/mod_fido2/src/fido2_storage.cpp:930-940`

## Impact
1. **Authentication Failures**: FIDO2 counters reset on reboot, causing "counter mismatch" errors
2. **Silent Data Loss**: Counters appear saved but reset to 0
3. **Security**: Counter reset could allow replay attacks
4. **Debug Difficulty**: User sees "counter too low" error with no explanation

## Evidence
From `components/mod_fido2/src/fido2_storage.cpp`:

```cpp
// Lines 930-940: fido2_storage_save_sign_count() - CRITICAL error ignored
void fido2_storage_save_sign_count(uint32_t count) {
    g_storage.signCount = count;
    
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        LOG_E("FIDO2", "Failed to open NVS for sign count: %s", esp_err_to_name(err));
        return;  // ❌ Returns without error propagation
    }

    err = nvs_set_u32(nvs, "sign_count", count);
    if (err != ESP_OK) {
        LOG_E("FIDO2", "Failed to set sign count: %s", esp_err_to_name(err));
        nvs_close(nvs);
        return;  // ❌ Returns without error propagation
    }

    err = nvs_commit(nvs);
    if (err != ESP_OK) {
        LOG_E("FIDO2", "CRITICAL: Sign count NVS commit failed: %s", esp_err_to_name(err));
        // ❌ Logs CRITICAL but doesn't return error!
        // ❌ Caller doesn't know save failed
    }

    nvs_close(nvs);
}

// Usage in Fido2Module.cpp or similar:
fido2_storage_save_sign_count(newCount);  // ❌ No way to know if it failed!
```

**Problem:**
- Function is `void`, no error return
- CRITICAL error logged but caller doesn't know
- Counter may be lost on reboot
- FIDO2 spec requires counter persistence

## Recommended Fix
Add error return to sign count persistence:

1. **Change function signature**:
   ```cpp
   // In fido2_storage.h
   bool fido2_storage_save_sign_count(uint32_t count);  // Return bool for success
   
   // In fido2_storage.cpp
   bool fido2_storage_save_sign_count(uint32_t count) {
       g_storage.signCount = count;
       
       nvs_handle_t nvs;
       esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
       if (err != ESP_OK) {
           LOG_E("FIDO2", "Failed to open NVS for sign count: %s", esp_err_to_name(err));
           return false;  // Return error
       }

       err = nvs_set_u32(nvs, "sign_count", count);
       if (err != ESP_OK) {
           LOG_E("FIDO2", "Failed to set sign count: %s", esp_err_to_name(err));
           nvs_close(nvs);
           return false;
       }

       err = nvs_commit(nvs);
       if (err != ESP_OK) {
           LOG_E("FIDO2", "CRITICAL: Sign count NVS commit failed: %s", esp_err_to_name(err));
           nvs_close(nvs);
           return false;  // Return error
       }

       nvs_close(nvs);
       return true;  // Success
   }
   ```

2. **Update callers to check errors**:
   ```cpp
   // In Fido2Module.cpp, after incrementing counter
   if (!fido2_storage_save_sign_count(g_storage.signCount)) {
       LOG_E("FIDO2", "Failed to persist sign count, authentication may fail on reboot");
       // Maybe show warning to user?
       // Maybe retry later?
   }
   ```

3. **Add test cases**:
   - Mock NVS to simulate commit failure
   - Verify function returns `false`
   - Verify error is logged
   - Verify caller handles error appropriately

## References
- FIDO2 CTAP2 Specification: https://fidoalliance.org/specs/fido2-web-authentication-client-to-authenticator-protocol-client-to-authenticator-protocol-v2.1-rd-20191217.html
- FIDO2 Counter Requirements: https://fidoalliance.org/specs/fido-v2.0-rd-20180309/fido-client-to-authenticator-protocol-v2.0-rd-20180309.html#sign-counter

</content>