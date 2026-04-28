---
title: "[MEDIUM] Synchronous NVS file I/O blocks startup on every boot"
severity: MEDIUM
domain: startup-performance
lens: startup-perf
labels:
  - "audit:performance/startup-perf"
---

## Summary
Multiple **synchronous NVS (flash) operations** execute on the main thread during startup, causing blocking I/O that delays system readiness. These include:
- Module list persistence (`saveModuleList()`, `loadDisabledList()`)
- TROPIC storage cache header validation (`loadHeader()`)
- Display backlight settings (`loadBacklight()`)
- Attestation key hash persistence (`saveStoredHash()`, `loadStoredHash()`)

**Location:** `components/cdc_core/src/ModuleRegistry.cpp:364-500`, `components/cdc_core/src/TropicStorage.cpp:318-360`

## Impact
- **Flash wear**: Each boot writes NVS data, consuming limited flash endurance (~100K cycles)
- **Blocking I/O**: NVS operations can take 10-50ms each (depending on fragmentation)
- **Cumulative delay**: 5-10 NVS operations × 20ms = 100-200ms added to boot time
- **No progress feedback**: User sees no indication during these blocking operations

## Evidence
From `components/cdc_core/src/ModuleRegistry.cpp:428-465`:
```cpp
void ModuleRegistry::saveModuleList() {
    // ... build module list ...
    nvs_handle_t handle;
    if (nvs_open(MODULES_NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_str(handle, MODULES_NVS_KEY, moduleList);
        nvs_commit(handle);  // Blocking flash write!
        nvs_close(handle);
    }
}
```

From `components/cdc_core/src/TropicStorage.cpp:340-360`:
```cpp
bool TropicStorage::saveHeader() {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        esp_err_t err = nvs_set_blob(nvs, NVS_KEY_HEADER, &header_, sizeof(header_));
        if (err == ESP_OK) {
            err = nvs_commit(nvs);  // Blocking flash write!
        }
        nvs_close(nvs);
    }
}
```

From `main/main.cpp:183-188`:
```cpp
auto& tropicStorage = cdc::core::TropicStorage::instance();
tropicStorage.setSecureElement(s_secureElement);
tropicStorage.init();  // Calls loadHeader() - NVS read
tropicStorage.start();
```

## Recommended Fix
1. **Defer non-critical NVS writes**: Only write NVS if data actually changed (add dirty tracking)
2. **Batch NVS operations**: Group multiple writes into single `nvs_commit()` call
3. **Async writes**: Use `nvs_commit()` with callback or schedule writes after startup complete

**Implementation:**
```cpp
// In ModuleRegistry, track if re-save is needed
void ModuleRegistry::saveModuleList() {
    // Only write if module list changed
    char currentList[MAX_MODULE_LIST_SIZE] = {0};
    // ... build current list ...
    
    // Compare with saved list
    if (strcmp(savedList, currentList) == 0) {
        return;  // No change, skip write
    }
    
    // ... existing write logic ...
}

// In TropicStorage, batch header + chunk writes
bool TropicStorage::rebuild() {
    // ... load all chunks ...
    
    // Single commit for all changes
    nvs_handle_t nvs;
    nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    // ... set all blobs ...
    nvs_commit(nvs);  // Single blocking operation instead of many
    nvs_close(nvs);
}
```

## References
- ESP-IDF NVS: [Performance Considerations](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/storage/nvs_flash.html#performance)
- NVS Flash Endurance: [Typical 100K write cycles](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/storage/nvs_flash.html#erasing-nvs)
