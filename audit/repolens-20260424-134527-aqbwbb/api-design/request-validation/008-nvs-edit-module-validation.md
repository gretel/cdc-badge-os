---
title: "[LOW] NVS editor module loads all namespaces without size validation"
severity: LOW
domain: api-design/request-validation
lens: serial-command-interface
labels:
  - "request-validation"
  - "nvs"
  - "memory"
---

## Summary

The NVS edit module in `components/mod_nvsedit/src/NvsEditModule.cpp` loads all namespaces from NVS without validating that the number of namespaces fits within allocated buffers. While there are compile-time limits (`MAX_NAMESPACES = 32`, `MAX_KEYS = 48`), the code doesn't validate that these limits are appropriate for the actual NVS partition size.

**Location:** `components/mod_nvsedit/src/NvsEditModule.cpp:91-118`

## Impact

- **Silent truncation**: If more than 32 namespaces exist, only the first 32 are shown
- **Memory pressure**: Large NVS partitions could cause performance issues when loading all entries
- **Incomplete view**: Users might not see all available namespaces

## Evidence

```cpp
// File: components/mod_nvsedit/src/NvsEditModule.cpp:34-36
static constexpr uint8_t MAX_NAMESPACES = 32;
static constexpr uint8_t MAX_KEYS = 48;

// File: components/mod_nvsedit/src/NvsEditModule.cpp:91-118
static void loadNamespaces() {
    s_namespaceCount = 0;
    memset(s_namespaces, 0, sizeof(s_namespaces));

    nvs_iterator_t it = nullptr;
    esp_err_t err = nvs_entry_find(NVS_DEFAULT_PART_NAME, nullptr, NVS_TYPE_ANY, &it);

    char lastNs[16] = {};
    while (err == ESP_OK && s_namespaceCount < MAX_NAMESPACES) {  // Line 100
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);

        // Add namespace if not already in list
        if (strcmp(info.namespace_name, lastNs) != 0) {
            strncpy(s_namespaces[s_namespaceCount], info.namespace_name, 15);
            s_namespaces[s_namespaceCount][15] = '\0';
            strncpy(lastNs, info.namespace_name, 15);
            s_namespaceCount++;
        }

        err = nvs_entry_next(&it);
    }

    if (it) nvs_release_iterator(it);
    LOG_I(TAG, "Found %d namespaces", s_namespaceCount);
}
```

Issues:
1. No warning when namespace count exceeds `MAX_NAMESPACES`
2. No validation that namespace names fit in the 16-byte buffer
3. Similar issue in `loadKeys()` for keys within a namespace

## Recommended Fix

Add validation and warning when limits are exceeded:

```cpp
static void loadNamespaces() {
    s_namespaceCount = 0;
    memset(s_namespaces, 0, sizeof(s_namespaces));

    nvs_iterator_t it = nullptr;
    esp_err_t err = nvs_entry_find(NVS_DEFAULT_PART_NAME, nullptr, NVS_TYPE_ANY, &it);

    char lastNs[16] = {};
    bool truncated = false;
    
    while (err == ESP_OK) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);

        // Add namespace if not already in list
        if (strcmp(info.namespace_name, lastNs) != 0) {
            if (s_namespaceCount < MAX_NAMESPACES) {
                strncpy(s_namespaces[s_namespaceCount], info.namespace_name, 15);
                s_namespaces[s_namespaceCount][15] = '\0';
                strncpy(lastNs, info.namespace_name, 15);
                s_namespaceCount++;
            } else {
                truncated = true;
            }
        }

        err = nvs_entry_next(&it);
    }

    if (it) nvs_release_iterator(it);
    
    LOG_I(TAG, "Found %d namespaces", s_namespaceCount);
    if (truncated) {
        LOG_W(TAG, "Namespace list truncated (max %d)", MAX_NAMESPACES);
    }
}
```

And add validation for key loading:

```cpp
static void loadKeys(const char* ns) {
    s_keyCount = 0;
    memset(s_keys, 0, sizeof(s_keys));

    nvs_iterator_t it = nullptr;
    esp_err_t err = nvs_entry_find(NVS_DEFAULT_PART_NAME, ns, NVS_TYPE_ANY, &it);

    bool truncated = false;
    
    while (err == ESP_OK) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);

        if (s_keyCount < MAX_KEYS) {
            // Validate key name length
            size_t keyLen = strlen(info.key);
            if (keyLen <= 15) {
                strncpy(s_keys[s_keyCount], info.key, 15);
                s_keys[s_keyCount][15] = '\0';
                s_keyTypes[s_keyCount] = info.type;
                s_keyCount++;
            } else {
                LOG_W(TAG, "Key '%s' too long, skipping", info.key);
            }
        } else {
            truncated = true;
        }

        err = nvs_entry_next(&it);
    }

    if (it) nvs_entry_next(&it);
    
    LOG_I(TAG, "Found %d keys in '%s'", s_keyCount, ns);
    if (truncated) {
        LOG_W(TAG, "Key list truncated (max %d)", MAX_KEYS);
    }
}
```

## References

- ESP-IDF NVS documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
- CWE-131: Incorrect calculation of buffer size
- CWE-195: Signed to unsigned conversion error
