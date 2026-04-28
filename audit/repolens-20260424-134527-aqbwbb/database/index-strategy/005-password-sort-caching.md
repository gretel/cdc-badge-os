---
title: "[LOW] Password store list sorting recalculates on every UI refresh"
severity: LOW
domain: database
lens: index-strategy
labels:
  - audit:database/index-strategy
---

## Summary
The password store's `listEntriesSorted()` method (`components/mod_password/src/PasswordStore.cpp:348-380`) performs a full sort of all entries every time it is called. The sorted result is not cached, so UI list refreshes (which call this method) incur O(n log n) sorting cost repeatedly.

**Evidence:**
- `PasswordStore::listEntriesSorted()` at `components/mod_password/src/PasswordStore.cpp:348-380`
- Called from UI list view initialization (in `PasswordModule.cpp`)
- Called from `findSlotByIndex()` at `components/mod_password/src/PasswordModule.cpp:161-177`
- No cache or memoization of sorted results

## Impact
- **Redundant computation**: Same data sorted multiple times per session
- **CPU overhead**: O(n log n) sort for each list view refresh
- **Memory allocation**: `EntryIndex[]` array allocated on every call
- **Inconsistent with cache design**: TROPIC cache stores metadata but not sorted order

## Evidence
```cpp
// components/mod_password/src/PasswordStore.cpp:348-380
bool PasswordStore::listEntriesSorted(EntryIndex* entries, uint16_t maxEntries, uint16_t* countOut) const {
    // ...
    cdc::core::TropicStorage::instance().forEachSlot(
        moduleId_, rmemStart_, rmemEnd_, cb, &ctx);
    
    std::sort(entries, entries + *countOut, /* comparison function */);  // Every time!
    
    return true;
}
```

```cpp
// Called from findSlotByIndex() which allocates fresh array each time:
// components/mod_password/src/PasswordModule.cpp:161-177
auto list = std::unique_ptr<PasswordStore::EntryIndex[]>(
    new (std::nothrow) PasswordStore::EntryIndex[cap]);
store.listEntriesSorted(list.get(), cap, &count);  // Sorts again
```

## Recommended Fix
Cache the sorted index with invalidation:

1. **Add sorted index cache** to `PasswordStore`:
   ```cpp
   struct SortedIndex {
       EntryIndex entries[MAX_CAPACITY];
       uint16_t count;
       uint32_t version;  // Invalidates when entries change
   };
   ```

2. **Track cache version**:
   - Increment on `addEntry()`, `updateEntry()`, `deleteEntry()`
   - Compare version in `listEntriesSorted()` to determine if re-sort needed

3. **Return cached result**:
   ```cpp
   bool PasswordStore::listEntriesSorted(EntryIndex* entries, uint16_t maxEntries, uint16_t* countOut) const {
       if (sortedIndex_.version == currentVersion_ && sortedIndex_.count <= maxEntries) {
           // Return cached
           memcpy(entries, sortedIndex_.entries, sortedIndex_.count * sizeof(EntryIndex));
           *countOut = sortedIndex_.count;
           return true;
       }
       // Rebuild cache
       // ...
   }
   ```

4. **Alternative: Use insertion-stable data structure**:
   - Maintain sorted order incrementally on insert/delete
   - O(log n) insert instead of O(n log n) full sort

## References
- Memoization pattern: https://en.wikipedia.org/wiki/Memoization
- Incremental sorting: https://en.wikipedia.org/wiki/Sorting_algorithm#Incremental_sorting
- Cache invalidation strategies: https://martinfowler.com/eaaDev/CacheInvalidation.html
