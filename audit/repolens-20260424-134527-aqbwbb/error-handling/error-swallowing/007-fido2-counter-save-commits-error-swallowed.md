---
title: "[MEDIUM] FIDO2 counter save swallows NVS commit failure"
severity: MEDIUM
domain: mod_fido2
lens: error-handling
labels:
  - "audit:error-handling/error-swallowing"
---

## Summary
In `components/mod_fido2/src/fido2_storage.cpp`, the function that saves the FIDO2 authentication counter logs an NVS commit failure but returns `true` anyway, masking the actual write failure.

**Location:** `components/mod_fido2/src/fido2_storage.cpp` (around line 399-412)

```cpp
err = nvs_commit(nvs);
if (err != ESP_OK) {
    LOG_W("FIDO2", "NVS commit failed: %s", esp_err_to_name(err));
}

nvs_close(nvs);
return true;  // Returns true even when commit failed!
```

## Impact
- Authentication counter may not be persisted when NVS commit fails
- Caller assumes counter was saved successfully
- On reboot, counter resets to old value, potentially allowing replay attacks
- FIDO2 security guarantees compromised silently

## Evidence
The function:
1. Opens NVS and checks for errors (returns `false` on failure)
2. Sets the counter value and checks for errors (returns `false` on failure)
3. Commits the NVS change but only logs the error (doesn't return `false`)
4. Always returns `true` at the end

This inconsistency means callers cannot detect when the counter actually persisted.

## Recommended Fix
Return `false` when commit fails:

```cpp
err = nvs_commit(nvs);
nvs_close(nvs);
if (err != ESP_OK) {
    LOG_E("FIDO2", "NVS commit failed for counter: %s", esp_err_to_name(err));
    return false;  // Signal failure to caller
}
return true;
```

Also consider changing the log level from `LOG_W` to `LOG_E` since this is a real failure that affects FIDO2 security.

## References
- FIDO2 CTAP2 Specification: https://fidoalliance.org/specs/fido2/fido-client-to-authenticator-protocol-v2.1-rd-20191217.html
- ESP-IDF NVS Documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
