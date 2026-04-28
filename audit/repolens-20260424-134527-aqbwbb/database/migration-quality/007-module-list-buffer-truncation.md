---
title: "[LOW] NVS namespace for module list uses fixed-size buffer risking truncation"
severity: LOW
domain: database/migration-quality
lens: embedded-storage
labels:
  - "nvs-migration"
  - "buffer-overflow"
---

## Summary
In `components/cdc_core/src/ModuleRegistry.cpp:424-465`, the module list is stored in a fixed-size NVS buffer:

```cpp
static constexpr size_t MAX_MODULE_LIST_SIZE = 256;

void ModuleRegistry::saveModuleList() {
    // ...
    char moduleList[MAX_MODULE_LIST_SIZE] = {0};
    size_t offset = 0;

    for (uint8_t i = 0; i < count_; i++) {
        const char* name = modules_[i]->getName();
        size_t nameLen = strlen(name);

        // Check if it fits
        if (offset + nameLen + 2 > sizeof(moduleList)) {
            LOG_W(TAG, "Module list too long, truncating");
            break;
        }
        // ...
    }
    // ...
    nvs_set_str(handle, MODULES_NVS_KEY, moduleList);
}
```

When the list is truncated, it may end with an incomplete module name (no trailing comma), which could cause parsing issues when loading.

## Impact
- **Data truncation**: If more than ~250 characters of module names are registered, the list is silently truncated.
- **Parsing issues**: A truncated list might have a partial module name at the end, causing `loadDisabledList()` to read a malformed name.
- **Silent failure**: The warning is logged but the operation continues with incomplete data.

## Evidence
File: `components/cdc_core/src/ModuleRegistry.cpp:424-465`

The truncation check (line 437-440):
```cpp
if (offset + nameLen + 2 > sizeof(moduleList)) {
    LOG_W(TAG, "Module list too long, truncating");
    break;
}
```

Then the list is saved as-is (line 457-461):
```cpp
moduleList[offset] = '\0';

// Save to NVS
nvs_handle_t handle;
if (nvs_open(MODULES_NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
    nvs_set_str(handle, MODULES_NVS_KEY, moduleList);
```

In `loadDisabledList()` (line 472-483), the parsing assumes comma-separated tokens but doesn't handle partial trailing tokens.

## Recommended Fix
Improve buffer handling:

1. **Increase buffer size**: 256 bytes is small for a module list. Consider 512 or 1024:
   ```cpp
   static constexpr size_t MAX_MODULE_LIST_SIZE = 512;
   ```

2. **Validate on load**: Check that the last token is complete:
   ```cpp
   void loadDisabledList() {
       // ...
       // Check last character isn't a partial token
       size_t len = strlen(disabledModules_);
       if (len > 0 && disabledModules_[len-1] == ',') {
           LOG_W(TAG, "Module list truncated, clearing");
           disabledModules_[0] = '\0';
       }
   }
   ```

3. **Add count check**: Warn if truncation occurs:
   ```cpp
   if (offset + nameLen + 2 > sizeof(moduleList)) {
       LOG_E(TAG, "Module list too long for %d modules, consider increasing buffer", count_);
       // Optionally save with a truncated count marker
       break;
   }
   ```

## References
- Buffer sizing best practices: https://www.owasp.org/index.php/Buffer_overflow
- ESP-IDF NVS limits: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api/storage/nvs.html#namespaces-and-keys
