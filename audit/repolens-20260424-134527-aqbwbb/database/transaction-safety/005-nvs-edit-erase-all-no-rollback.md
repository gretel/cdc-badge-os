---
title: "[MEDIUM] NVS erase-all operation lacks rollback on partial failure"
severity: MEDIUM
domain: database/transaction-safety
lens: transaction-safety
labels:
  - "audit:database/transaction-safety"
---

## Summary

In `components/mod_nvsedit/src/NvsEditModule.cpp`, the `deleteNamespace()` function uses `nvs_erase_all()` followed by `nvs_commit()` without intermediate error checking. If `nvs_erase_all()` succeeds but `nvs_commit()` fails, the namespace is left in an empty state with no way to recover the original data.

**Affected location:** `deleteNamespace()` (lines 170-187) and `deleteKey()` (lines 151-165)

## Impact

**Data loss scenarios:**
1. `nvs_erase_all()` erases all keys in namespace
2. `nvs_commit()` fails (flash wear, power loss, corruption)
3. Result: Namespace exists but is empty, original data unrecoverable
4. Same issue with `deleteKey()` - erase succeeds, commit fails

**Specific to NVS:**
- NVS uses pages with wear leveling; erase-all marks pages as free
- If commit fails, the old page chain may be invalidated
- On next read, NVS sees "no data" rather than "old data"
- The `nvs_entry_find()` iterator may still find the namespace header but no keys

## Evidence

**deleteNamespace()** (lines 170-187):
```cpp
static bool deleteNamespace(const char* ns) {
    nvs_handle_t handle;
    if (nvs_open(ns, NVS_READWRITE, &handle) != ESP_OK) {
        return false;
    }

    esp_err_t err = nvs_erase_all(handle);
    if (err == ESP_OK) {
        nvs_commit(handle);  // No error check!
    }
    nvs_close(handle);

    LOG_I(TAG, "Deleted namespace '%s': %s", ns, esp_err_to_name(err));
    return err == ESP_OK;
}
```

**deleteKey()** (lines 151-165):
```cpp
static bool deleteKey(const char* ns, const char* key) {
    nvs_handle_t handle;
    if (nvs_open(ns, NVS_READWRITE, &handle) != ESP_OK) {
        return false;
    }

    esp_err_t err = nvs_erase_key(handle, key);
    if (err == ESP_OK) {
        nvs_commit(handle);  // No error check!
    }
    nvs_close(handle);

    LOG_I(TAG, "Deleted key '%s' from '%s': %s", key, ns, esp_err_to_name(err));
    return err == ESP_OK;
}
```

Note: `nvs_commit()` return value is ignored. If it fails, `err` still contains the result of `nvs_erase_all()` or `nvs_erase_key()`, which was successful.

## Recommended Fix

**Check commit result and log appropriately:**
```cpp
static bool deleteNamespace(const char* ns) {
    nvs_handle_t handle;
    if (nvs_open(ns, NVS_READWRITE, &handle) != ESP_OK) {
        return false;
    }

    esp_err_t err = nvs_erase_all(handle);
    if (err != ESP_OK) {
        nvs_close(handle);
        LOG_E(TAG, "Erase-all failed for '%s': %s", ns, esp_err_to_name(err));
        return false;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        LOG_W(TAG, "Commit failed for '%s' (data may be lost): %s", ns, esp_err_to_name(err));
        // Note: Data is already erased, can't rollback
        return false;
    }

    LOG_I(TAG, "Deleted namespace '%s'", ns);
    return true;
}

static bool deleteKey(const char* ns, const char* key) {
    nvs_handle_t handle;
    if (nvs_open(ns, NVS_READWRITE, &handle) != ESP_OK) {
        return false;
    }

    esp_err_t err = nvs_erase_key(handle, key);
    if (err != ESP_OK) {
        nvs_close(handle);
        LOG_E(TAG, "Erase key '%s' failed: %s", key, esp_err_to_name(err));
        return false;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        LOG_W(TAG, "Commit failed for key '%s' (data may be lost): %s", key, esp_err_to_name(err));
        return false;
    }

    LOG_I(TAG, "Deleted key '%s' from '%s'", key, ns);
    return true;
}
```

**Option 2: Pre-commit backup (for critical namespaces)**
For namespaces that hold important data, implement a backup-restore pattern:
```cpp
static bool deleteNamespaceWithBackup(const char* ns) {
    // 1. Read all keys into memory
    // 2. Erase all
    // 3. Commit
    // 4. If commit fails, restore from backup
}
```

## References

- ESP32 NVS API documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
- NVS page structure and commit semantics
- Flash wear leveling considerations