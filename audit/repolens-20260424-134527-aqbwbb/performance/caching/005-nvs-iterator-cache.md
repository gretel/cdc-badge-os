---
title: "[MEDIUM] NVS iterator caching missing - namespace/key enumeration repeated on each access"
severity: MEDIUM
domain: performance/caching
lens: embedded-firmware
labels:
  - "nvs-cache"
  - "iterator-optimization"
---

## Summary
The NVS Editor module (`components/mod_nvsedit/src/NvsEditModule.cpp`) reloads all namespaces and keys from NVS every time the views are refreshed. The `loadNamespaces()` and `loadKeys()` functions perform full NVS iteration without caching results between UI refreshes.

**Evidence:**
- File: `components/mod_nvsedit/src/NvsEditModule.cpp`
- Function: `loadNamespaces()` (line 91-119) - full NVS iteration
- Function: `loadKeys()` (line 121-141) - full namespace iteration  
- Called from: `showNamespaceListView()` (line 406), `showKeyListView()` (line 471)
- Each call opens NVS, iterates all entries, closes NVS

## Impact
**Performance Cost:**
- NVS iteration requires opening namespace, iterating all entries, releasing iterator
- Typical NVS has 50-200 entries across multiple namespaces
- Each iteration: 10-50ms depending on entry count
- UI refresh (e.g., returning to namespace list) triggers full reload

**User Experience:**
- Lag when navigating between namespace and key views
- Delayed refresh after returning from value detail view
- Unnecessary flash activity when data hasn't changed

## Evidence
From `components/mod_nvsedit/src/NvsEditModule.cpp`:

```cpp
/**
 * \brief Loads unique namespace names from the default NVS partition.
 */
static void loadNamespaces() {
    s_namespaceCount = 0;
    memset(s_namespaces, 0, sizeof(s_namespaces));

    nvs_iterator_t it = nullptr;
    esp_err_t err = nvs_entry_find(NVS_DEFAULT_PART_NAME, nullptr, NVS_TYPE_ANY, &it);

    char lastNs[16] = {};
    while (err == ESP_OK && s_namespaceCount < MAX_NAMESPACES) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        // ... process entry
        err = nvs_entry_next(&it);
    }

    if (it) nvs_release_iterator(it);
    LOG_I(TAG, "Found %d namespaces", s_namespaceCount);
}

/** \brief Shows namespace list view. */
static void showNamespaceListView() {
    loadNamespaces();  // Full reload every time!
    // ...
}

/** \brief Shows key list view for a namespace. */
static void showKeyListView(const char* ns) {
    loadKeys(ns);  // Full reload every time!
    // ...
}
```

Every view refresh triggers complete NVS iteration.

## Recommended Fix
Implement cache with invalidation on data changes:

1. **Add cache validity flag**:
```cpp
static bool s_namespaceCacheValid = false;
static bool s_keyCacheValid = false;
```

2. **Modify load functions to check cache**:
```cpp
static void loadNamespaces() {
    if (s_namespaceCacheValid) {
        return;  // Use cached data
    }
    
    // ... existing NVS iteration code ...
    
    s_namespaceCacheValid = true;
}

static void loadKeys(const char* ns) {
    // Invalidate key cache if namespace changed
    if (strcmp(s_selectedNamespace, ns) != 0) {
        s_keyCacheValid = false;
        strncpy(s_selectedNamespace, ns, sizeof(s_selectedNamespace) - 1);
    }
    
    if (s_keyCacheValid) {
        return;  // Use cached data
    }
    
    // ... existing NVS iteration code ...
    
    s_keyCacheValid = true;
}
```

3. **Invalidate cache on mutations**:
```cpp
static void onDeleteNamespace() {
    if (deleteNamespace(s_selectedNamespace)) {
        showToastInfo("Deleted");
        loadNamespaces();  // This will re-read
        // ...
        s_namespaceCacheValid = true;  // Mark fresh cache valid
    }
    hideContextMenu();
}

static void onDeleteKey() {
    if (deleteKey(s_selectedNamespace, s_selectedKey)) {
        showToastInfo("Deleted");
        loadKeys(s_selectedNamespace);
        // ...
        s_keyCacheValid = true;
    }
    hideContextMenu();
}
```

4. **Optional: Add cache timestamp for auto-invalidation**:
```cpp
static uint32_t s_cacheTimestamp = 0;
static const uint32_t CACHE_TTL_MS = 1000;  // 1 second

static bool isCacheValid(uint32_t timestamp) {
    return (esp_timer_get_time() / 1000 - timestamp) < CACHE_TTL_MS;
}
```

## References
- NVS iteration performance: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
- NVS entry_find/entry_next overhead: ~1-2ms per entry
- Cache invalidation patterns: https://www.espressif.com/sites/default/files/documentation/esp32-nvs-flash-best-practices.pdf
