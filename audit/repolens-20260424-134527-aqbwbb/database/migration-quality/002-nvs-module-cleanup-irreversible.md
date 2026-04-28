---
title: "[HIGH] NVS module data cleanup is irreversible without backup"
severity: HIGH
domain: database/migration-quality
lens: embedded-storage
labels:
  - "nvs-migration"
  - "data-loss"
---

## Summary
In `components/cdc_core/src/ModuleRegistry.cpp:364-425`, the `cleanupOrphanedModuleData()` function permanently erases NVS data for modules that are no longer registered:

```cpp
if (!found) {
    // Module no longer exists - erase its NVS namespace
    char nsName[20];
    snprintf(nsName, sizeof(nsName), "%s%s", NVS_PREFIX, token);

    LOG_W(TAG, "Module '%s' removed - erasing NVS namespace '%s'", token, nsName);

    nvs_handle_t modHandle;
    if (nvs_open(nsName, NVS_READWRITE, &modHandle) == ESP_OK) {
        nvs_erase_all(modHandle);
        nvs_commit(modHandle);
        nvs_close(modHandle);
        LOG_I(TAG, "Erased NVS data for removed module '%s'", token);
    }
}
```

There is no backup, export, or migration step before the irreversible `nvs_erase_all()`.

## Impact
- **Permanent data loss**: If a module is accidentally removed from the build (e.g., feature flag change, refactoring), all its NVS data is immediately wiped.
- **No rollback path**: Users cannot restore module data after a build change since the cleanup happens automatically on boot.
- **Silent failures**: The operation logs at INFO level but doesn't warn users about data that will be lost.

## Evidence
File: `components/cdc_core/src/ModuleRegistry.cpp:364-425`

The function reads the saved module list from NVS (line 371-377):
```cpp
char savedList[MAX_MODULE_LIST_SIZE] = {0};
size_t len = sizeof(savedList);
esp_err_t err = nvs_get_str(handle, MODULES_NVS_KEY, savedList, &len);
```

Then for each module in the saved list that's no longer registered, it directly erases the namespace without any confirmation or backup (line 407-417).

## Recommended Fix
Implement a safe migration workflow:

1. **Add a "migrate" flag**: Before erasing, check if the module namespace should be preserved (e.g., a special "keep" marker).
2. **Export before erase**: Serialize the old data to a backup namespace (`backup_<module>`) before erasing:
   ```cpp
   // Export data first
   nvs_handle_t backupHandle;
   if (nvs_open(("backup_" + nsName).c_str(), NVS_READWRITE, &backupHandle) == ESP_OK) {
       // Copy all keys to backup
       // ...
       nvs_commit(backupHandle);
   }
   ```
3. **Add boot-time warning**: If orphaned data is detected, show a user-visible warning before cleanup.
4. **Provide "restore" command**: Allow users to restore backed-up module data if they re-add the module.

## References
- ESP-IDF NVS API: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api/storage/nvs.html
- Data migration patterns: https://www.theserverside.com/answer/Database-migration-best-practices
