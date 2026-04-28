---
title: "[MEDIUM] Unbounded memory allocation for list buffers in Password and TOTP modules"
severity: MEDIUM
domain: performance/pagination
lens: pagination-streaming
labels:
  - "audit:performance/pagination"
---

## Summary
The Password and TOTP modules allocate full in-memory buffers for all entries when rendering lists. In `PasswordModule.cpp:378-400` and `TotpModule.cpp` (similar pattern), the `ensureListBuffers()` function allocates arrays sized to the **full capacity** of the store:

```cpp
// PasswordModule.cpp:378-400
s_listItems = new (std::nothrow) ui::ListItem[cap + 1];
s_entries = new (std::nothrow) PasswordStore::EntryIndex[cap];
```

Where `cap` can be up to **362 password entries** (slots 150-511) or **100 TOTP accounts** (slots 32-131).

## Impact
- **Memory overhead**: For passwords, this allocates ~5KB for `ListItem` array + ~7KB for `EntryIndex` array = ~12KB total
- **ESP32-S3 has ~320KB RAM** available; while this fits, it's inefficient for a device that typically holds far fewer entries
- **No lazy loading**: Entire dataset is loaded into memory even though only ~4 items are visible on the display at a time
- **Re-allocation on resize**: Buffers are reallocated whenever capacity changes

## Evidence
**File**: `components/mod_password/src/PasswordModule.cpp:378-400`
```cpp
static bool ensureListBuffers() {
    uint16_t cap = PasswordStore::instance().capacity();
    if (cap == 0) return false;
    if (cap == s_capacity && s_listItems && s_entries) return true;

    delete[] s_listItems;
    delete[] s_entries;
    s_listItems = new (std::nothrow) ui::ListItem[cap + 1];
    s_entries = new (std::nothrow) PasswordStore::EntryIndex[cap];
```

**File**: `components/mod_password/include/mod_password/PasswordStore.h:38`
```cpp
static constexpr size_t PAYLOAD_MAX = PASSWORD_PAYLOAD_MAX;
```
With `TITLE_LEN = 24`, `USERNAME_LEN = 16`, `PASSWORD_LEN = 64`, `URL_LEN = 64`, `NOTES_LEN` = remaining space.

**File**: `components/cdc_views/include/cdc_views/ListView.h:33`
```cpp
static constexpr uint16_t MAX_ITEMS = 512;  // Support large lists (e.g., password vault)
```

## Recommended Fix
Implement **lazy pagination** for the list view:

1. **Limit initial buffer allocation** to a reasonable maximum (e.g., 50 entries) instead of full capacity
2. **Add pagination support** to `ListView` with page size of ~20-30 items
3. **Load entries on-demand** as the user scrolls through the list
4. **Update `listEntriesSorted()`** to support offset/limit parameters:

```cpp
// New signature with pagination
bool listEntriesSorted(EntryIndex* entries, uint16_t maxEntries, 
                       uint16_t offset, uint16_t* countOut) const;
```

5. **Track current page** in the module state and reload buffer when navigating beyond current page

This reduces peak memory usage from ~12KB to ~2KB for typical use cases while maintaining acceptable UX.

## References
- ESP32-S3 memory architecture: ~320KB SRAM + optional PSRAM
- Display resolution: 296x128 pixels, showing ~4 visible items at a time
- Current `ListView` already supports scrolling; pagination just needs to load data in chunks
