---
title: "[MEDIUM] NVS iterator scans without caching cause repeated full-table enumeration"
severity: MEDIUM
domain: embedded-storage
lens: query-performance
labels:
  - "nvs-lookup"
  - "iteration"
---

## Summary

In `components/mod_nvsedit/src/NvsEditModule.cpp`, the `loadNamespaces()` and `loadKeys()` functions perform full NVS partition scans using `nvs_entry_find()` and `nvs_entry_next()` iterators every time they are called. These functions are invoked repeatedly during UI navigation without any caching mechanism.

**Location**: `components/mod_nvsedit/src/NvsEditModule.cpp:98-115` (loadNamespaces), `components/mod_nvsedit/src/NvsEditModule.cpp:122-138` (loadKeys)

## Impact

**Performance Cost**:
- Each call to `loadNamespaces()` enumerates ALL entries in the NVS partition
- Each call to `loadKeys(ns)` enumerates ALL keys in a namespace
- For a partition with N entries, each scan is O(N) with NVS read latency
- In a typical UI flow (namespace list → key list → value view → back → key list), the same data is scanned multiple times

**User Experience**:
- UI lag when navigating between NVS browser views
- Unnecessary flash memory reads (NVS is stored in SPI flash with ~1ms read latency per page)

**Scalability**:
- As NVS partition fills up, scan time increases linearly
- No early termination when max results reached (continues scanning even after `MAX_NAMESPACES` or `MAX_KEYS` filled)

## Evidence

**Inefficient pattern in `loadNamespaces()` (lines 98-115)**:
```cpp
static void loadNamespaces() {
    s_namespaceCount = 0;
    memset(s_namespaces, 0, sizeof(s_namespaces));

    nvs_iterator_t it = nullptr;
    esp_err_t err = nvs_entry_find(NVS_DEFAULT_PART_NAME, nullptr, NVS_TYPE_ANY, &it);

    char lastNs[16] = {};
    while (err == ESP_OK && s_namespaceCount < MAX_NAMESPACES) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);  // Read entry info

        // Add namespace if not already in list
        if (strcmp(info.namespace_name, lastNs) != 0) {
            strncpy(s_namespaces[s_namespaceCount], info.namespace_name, 15);
            s_namespaces[s_namespaceCount][15] = '\0';
            strncpy(lastNs, info.namespace_name, 15);
            s_namespaceCount++;
        }

        err = nvs_entry_next(&it);  // Next entry - continues even if we have enough
    }

    if (it) nvs_release_iterator(it);
    LOG_I(TAG, "Found %d namespaces", s_namespaceCount);
}
```

**Called repeatedly in UI flow**:
- `showNamespaceListView()` (line 406) calls `loadNamespaces()`
- `onDeleteNamespace()` (line 308) calls `loadNamespaces()` after delete
- Each view push/pop cycle may trigger re-scanning

## Recommended Fix

**Option 1: Add cache invalidation flag**

Add a timestamp or version counter that tracks when NVS data changes. Only rescan when the cache is stale:

```cpp
static uint32_t s_cacheVersion = 0;
static uint32_t s_lastCacheVersion = 0;
static bool s_namespacesValid = false;

// Increment s_cacheVersion in deleteKey() and deleteNamespace()
// Check s_lastCacheVersion == s_cacheVersion before scanning
```

**Option 2: Early termination optimization**

Stop scanning once we have enough results (minor improvement since we need unique namespaces):

```cpp
// After incrementing s_namespaceCount, check if we have the maximum
if (s_namespaceCount >= MAX_NAMESPACES) {
    break;  // Exit loop early instead of continuing to nvs_entry_next()
}
```

**Option 3: Batch scan with memoization**

Perform a single scan and cache both namespaces AND keys together:

```cpp
struct NvsCache {
    uint32_t version;
    char namespaces[MAX_NAMESPACES][16];
    uint8_t namespaceCount;
    char keys[MAX_NAMESPACES][MAX_KEYS][16];  // Keys per namespace
    nvs_type_t keyTypes[MAX_NAMESPACES][MAX_KEYS];
    uint8_t keyCounts[MAX_NAMESPACES];
    bool valid;
};

static NvsCache s_cache = {};
```

**Recommended approach**: Option 1 (cache invalidation) is simplest and fits the ~1 hour scope. Add a `s_cacheValid` flag that is set to `false` after any write operation, and check it before calling the scan functions.

## References

- [ESP-NVS API Reference](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api/reference/storage/nvs_flash.html) - `nvs_entry_find()` and `nvs_entry_next()` enumeration functions
- [NVS Internal Structure](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html#internal-structure) - NVS uses a linked list of pages; enumeration requires traversing all pages
