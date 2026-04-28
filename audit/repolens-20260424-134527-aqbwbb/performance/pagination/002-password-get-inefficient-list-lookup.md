---
title: "[LOW] Inefficient full-list allocation for single-entry lookup in PASSWORD_GET command"
severity: LOW
domain: performance/pagination
lens: performance
labels:
  - audit:performance/pagination
---

## Summary
In `components/mod_password/src/PasswordModule.cpp:163-184`, the `findSlotByIndex()` function is used by `cmd_password_get()` to resolve a single entry index to its physical slot. However, it currently allocates and populates the **entire** password list (up to 160 entries), sorts it, and then selects just one entry. This is O(n) memory allocation and O(n log n) sorting for a single lookup.

**Location**: `components/mod_password/src/PasswordModule.cpp:163-184`

```cpp
static bool findSlotByIndex(uint16_t index, uint16_t* slotOut) {
    if (!slotOut) return false;
    auto& store = PasswordStore::instance();
    if (!store.hasSlotRange()) return false;

    uint16_t cap = store.capacity();
    if (cap == 0) return false;
    auto list = std::unique_ptr<PasswordStore::EntryIndex[]>(new (std::nothrow) PasswordStore::EntryIndex[cap]);
    if (!list) return false;

    uint16_t count = 0;
    if (!store.listEntriesSorted(list.get(), cap, &count)) return false;
    if (index >= count) return false;

    *slotOut = list[index].slot;
    return true;
}
```

## Impact
- **Memory waste**: Allocates ~5KB heap memory just to look up one entry
- **CPU waste**: Performs full sort of all entries when only one is needed
- **Scalability**: As the password list grows, this inefficiency compounds

## Evidence
1. `findSlotByIndex()` at line 163 is called by `cmd_password_get()` at line 221
2. `cmd_password_get()` only needs one entry but triggers full list allocation and sort
3. The `listEntriesSorted()` function loads all entries, then sorts them (line 348-383 in PasswordStore.cpp)

## Recommended Fix
Add a direct index-to-slot resolution method to `PasswordStore` that doesn't require loading the full list. Since the store already has a direct mapping from logical index to physical slot via `toPhysicalSlot()`, the sorted index lookup could be optimized:

**Option A**: Add an unsorted index lookup (faster, order not guaranteed):
```cpp
// In PasswordStore.h
bool getEntryByIndex(uint16_t index, PasswordEntry* out) const;

// In PasswordStore.cpp
bool PasswordStore::getEntryByIndex(uint16_t index, PasswordEntry* out) const {
    if (!out || !hasSlotRange_) return false;
    uint16_t physSlot = 0;
    if (!toPhysicalSlot(index, &physSlot)) return false;
    return readEntry(index, out);
}
```

**Option B**: Add a callback-based find-by-index that stops after finding the Nth entry:
```cpp
bool findEntryByIndex(uint16_t targetIndex, uint16_t* slotOut, const char* titleOut);
```

**Option C** (simplest): Cache the sorted list in the module (like the UI already does) and reuse it:
```cpp
static PasswordStore::EntryIndex* s_sortedList = nullptr;
static uint16_t s_sortedCount = 0;
static uint32_t s_lastRebuildTick = 0;

// Rebuild only when needed (e.g., every 100ms or on entry change)
```

## References
- PasswordStore already uses static buffers for UI: `components/mod_password/src/PasswordModule.cpp:341-344`
- Direct slot mapping exists: `PasswordStore::toPhysicalSlot()` at line 88-99
