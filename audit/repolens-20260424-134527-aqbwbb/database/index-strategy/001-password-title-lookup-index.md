---
title: "[MEDIUM] Password store lacks efficient title-based lookup index"
severity: MEDIUM
domain: database
lens: index-strategy
labels:
  - audit:database/index-strategy
---

## Summary
The password store (`components/mod_password/src/PasswordStore.cpp`) stores entries in TROPIC01 R-Memory slots with title as the primary user-facing identifier, but there is no efficient index for title-based lookups. The `listEntriesSorted()` method (line 348-380) loads ALL entries into memory and sorts them alphabetically every time the list is needed.

**Evidence:**
- `PasswordStore::listEntriesSorted()` at `components/mod_password/src/PasswordStore.cpp:348-380`
- `findSlotByIndex()` at `components/mod_password/src/PasswordModule.cpp:161-177` loads the entire sorted list just to find one entry
- Each call to list passwords iterates through all 353 possible slots (RMEM 159-511) via `forEachSlot()`

## Impact
- **Performance cost**: O(n) scan and sort on every list operation, where n can be up to 353 entries
- **Memory overhead**: Allocates array of `EntryIndex` structs for entire capacity on each lookup
- **User experience**: UI list refresh may be noticeably slow when many passwords are stored
- **Scalability**: As password count grows, lookup time increases linearly

## Evidence
```cpp
// components/mod_password/src/PasswordStore.cpp:348-380
bool PasswordStore::listEntriesSorted(EntryIndex* entries, uint16_t maxEntries, uint16_t* countOut) const {
    // ...
    cdc::core::TropicStorage::instance().forEachSlot(
        moduleId_, rmemStart_, rmemEnd_, cb, &ctx);  // Scans ALL slots
    
    std::sort(entries, entries + *countOut, /* sorts every time */);  // O(n log n) sort
}
```

```cpp
// components/mod_password/src/PasswordModule.cpp:161-177
static bool findSlotByIndex(uint16_t index, uint16_t* slotOut) {
    auto list = std::unique_ptr<PasswordStore::EntryIndex[]>(
        new (std::nothrow) PasswordStore::EntryIndex[cap]);  // Allocates for FULL capacity
    store.listEntriesSorted(list.get(), cap, &count);  // Loads and sorts all
}
```

## Recommended Fix
Add a cached index that is updated when passwords are added/updated/deleted:

1. **Create an index structure** stored in NVS alongside the TROPIC cache:
   ```cpp
   struct TitleIndex {
       char title[PasswordStore::TITLE_LEN + 1];
       uint16_t slot;
   };
   ```

2. **Add index maintenance methods** to `PasswordStore`:
   - `buildTitleIndex()` - builds index from current entries (called during rebuild)
   - `addToTitleIndex(const char* title, uint16_t slot)` - inserts on add
   - `updateTitleIndex(uint16_t slot, const char* newTitle)` - updates on edit
   - `removeFromTitleIndex(uint16_t slot)` - removes on delete
   - `findSlotByTitle(const char* title)` - O(log n) binary search lookup

3. **Update existing methods** to use the index:
   - `listEntriesSorted()` can now return cached index (already sorted)
   - `findSlotByTitle()` provides fast lookup for search functionality

4. **Ensure index consistency**:
   - Index rebuilt automatically when `TropicStorage::rebuild()` is called
   - Index updated synchronously with each write/erase operation

## References
- SQLite index strategy: https://www.sqlite.org/optoverview.html
- B-tree index for ordered lookups: https://en.wikipedia.org/wiki/B-tree
- Embedded database indexing patterns: https://www.sqlite.org/intern-v-extern-blob.html
