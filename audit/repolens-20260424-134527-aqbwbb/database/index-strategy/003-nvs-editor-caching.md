---
title: "[LOW] NVS editor reloads namespace/key lists on every access without caching"
severity: LOW
domain: database
lens: index-strategy
labels:
  - audit:database/index-strategy
---

## Summary
The NVS editor module (`components/mod_nvsedit/src/NvsEditModule.cpp`) reloads all namespaces and keys from NVS on every view refresh. The `loadNamespaces()` (line 94-118) and `loadKeys()` (line 121-145) functions use `nvs_entry_find()` iterator which scans the entire NVS partition each time.

**Evidence:**
- `loadNamespaces()` at `components/mod_nvsedit/src/NvsEditModule.cpp:94-118`
- `loadKeys()` at `components/mod_nvsedit/src/NvsEditModule.cpp:121-145`
- Called from `showNamespaceListView()` (line 406) and `showKeyListView()` (line 474)
- Called again after delete operations (lines 311, 338)

## Impact
- **Redundant NVS scans**: Every time user navigates back/forth, full NVS partition is scanned
- **No cache invalidation**: NVS is relatively slow (flash-based); repeated scans add latency
- **Memory inefficiency**: Static arrays `s_namespaces[MAX_NAMESPACES]` and `s_keys[MAX_KEYS]` are reallocated on each load

## Evidence
```cpp
// components/mod_nvsedit/src/NvsEditModule.cpp:94-118
static void loadNamespaces() {
    s_namespaceCount = 0;
    memset(s_namespaces, 0, sizeof(s_namespaces));
    
    nvs_iterator_t it = nullptr;
    esp_err_t err = nvs_entry_find(NVS_DEFAULT_PART_NAME, nullptr, NVS_TYPE_ANY, &it);
    
    while (err == ESP_OK && s_namespaceCount < MAX_NAMESPACES) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);  // Iterator scans NVS
        // ...
        err = nvs_entry_next(&it);
    }
}
```

```cpp
// Called multiple times per user session:
showNamespaceListView() { loadNamespaces(); }  // Line 406
showKeyListView(ns) { loadKeys(ns); }          // Line 474
onDeleteNamespace() { loadNamespaces(); }      // Line 311
onDeleteKey() { loadKeys(s_selectedNamespace); }  // Line 338
```

## Recommended Fix
Implement lazy loading with cache invalidation:

1. **Add cache state tracking**:
   ```cpp
   static uint32_t s_namespaceCacheVersion = 0;
   static uint32_t s_keyCacheVersion[MAX_NAMESPACES] = {};
   ```

2. **Track NVS modifications**:
   - Increment global version on `deleteKey()`, `deleteNamespace()`
   - Per-namespace version on namespace-specific changes

3. **Conditional reload**:
   ```cpp
   static void loadNamespacesIfNeeded() {
       uint32_t currentVersion = getNvsVersion();
       if (s_namespaceCacheVersion != currentVersion) {
           loadNamespaces();
           s_namespaceCacheVersion = currentVersion;
       }
   }
   ```

4. **Alternative: Use NVS get_next() efficiently**:
   - Keep iterator open between calls if frequently accessed
   - Cache results with timestamp for short-term validity

## References
- NVS API documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
- Cache invalidation patterns: https://martinfowler.com/eaaDev/CacheInvalidation.html
