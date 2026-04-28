---
title: "[MEDIUM] NVS edit module deletes without transaction safety"
severity: MEDIUM
domain: database/migration-quality
lens: embedded-storage
labels:
  - "nvs-migration"
  - "transaction-safety"
---

## Summary
In `components/mod_nvsedit/src/NvsEditModule.cpp:149-171`, the `deleteKey()` function performs NVS deletion without transaction safety:

```cpp
static bool deleteKey(const char* ns, const char* key) {
    nvs_handle_t handle;
    if (nvs_open(ns, NVS_READWRITE, &handle) != ESP_OK) {
        return false;
    }

    esp_err_t err = nvs_erase_key(handle, key);
    if (err == ESP_OK) {
        nvs_commit(handle);
    }
    nvs_close(handle);

    LOG_I(TAG, "Deleted key '%s' from '%s': %s", key, esp_err_to_name(err));
    return err == ESP_OK;
}
```

Similarly, `deleteNamespace()` (line 173-188) erases all keys in a namespace. If power is lost between the erase and commit, the NVS partition may be left in an inconsistent state.

## Impact
- **Partial commits**: If power is lost after `nvs_erase_key()` but before `nvs_commit()`, the change may or may not persist depending on NVS wear-leveling.
- **No rollback**: There's no way to undo a deletion if it was accidental (beyond the `FEATURE_NVS_EDIT` flag protection).
- **Namespace-wide risk**: `nvs_erase_all()` is particularly dangerous as it affects all keys in a namespace at once.

## Evidence
File: `components/mod_nvsedit/src/NvsEditModule.cpp:149-188`

The delete functions:
```cpp
static bool deleteKey(const char* ns, const char* key) {
    nvs_handle_t handle;
    if (nvs_open(ns, NVS_READWRITE, &handle) != ESP_OK) {
        return false;
    }

    esp_err_t err = nvs_erase_key(handle, key);
    if (err == ESP_OK) {
        nvs_commit(handle);  // Commit happens after erase
    }
    nvs_close(handle);
    return err == ESP_OK;
}

static bool deleteNamespace(const char* ns) {
    nvs_handle_t handle;
    if (nvs_open(ns, NVS_READWRITE, &handle) != ESP_OK) {
        return false;
    }

    esp_err_t err = nvs_erase_all(handle);
    if (err == ESP_OK) {
        nvs_commit(handle);  // All-or-nothing but not atomic
    }
    nvs_close(handle);
    return err == ESP_OK;
}
```

The delete is only protected by `FEATURE_NVS_EDIT` flag (line 58-67):
```cpp
static bool deleteEnabled() {
    return FEATURE_NVS_EDIT != 0;
}
```

## Recommended Fix
Implement safer deletion:

1. **Use NVS transaction API**: If available in the ESP-IDF version, use transaction-based operations:
   ```cpp
   nvs_begin_transaction(handle);
   err = nvs_erase_key(handle, key);
   if (err == ESP_OK) {
       err = nvs_commit(handle);
   }
   nvs_end_transaction(handle);
   ```

2. **Add confirmation for namespace delete**: Require double confirmation for `deleteNamespace()`:
   ```cpp
   static void onDeleteNamespace() {
       showConfirm("Delete entire namespace?", onConfirmDeleteNamespace, 
                   onReject, ConfirmView::Icon::WARNING);
   }
   ```

3. **Log before delete**: Write a pre-delete marker to a temp location:
   ```cpp
   // Before delete
   nvs_set_str(handle, "_delete_pending", key);
   nvs_commit(handle);
   
   // After successful delete
   nvs_erase_key(handle, "_delete_pending");
   ```

4. **Add recovery mode**: On boot, check for `_delete_pending` markers and offer recovery:
   ```cpp
   void checkRecovery() {
       nvs_handle_t handle;
       if (nvs_open(ns, NVS_READONLY, &handle) == ESP_OK) {
           char pending[16];
           size_t len = sizeof(pending);
           if (nvs_get_str(handle, "_delete_pending", pending, &len) == ESP_OK) {
               LOG_W(TAG, "Recovery: %s was being deleted", pending);
               // Offer to restore or complete
           }
           nvs_close(handle);
       }
   }
   ```

## References
- ESP-IDF NVS transactions: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api/storage/nvs.html#transactions
- Atomic operations in key-value stores: https://www.cockroachlabs.com/docs/atomic-operations
