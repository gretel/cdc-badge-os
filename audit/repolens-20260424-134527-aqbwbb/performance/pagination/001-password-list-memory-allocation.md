---
title: "[MEDIUM] Unbounded memory allocation for password list in serial command handler"
severity: MEDIUM
domain: performance/pagination
lens: performance
labels:
  - audit:performance/pagination
---

## Summary
In `components/mod_password/src/PasswordModule.cpp:193-213`, the `cmd_password_list()` function allocates a dynamic array of `PasswordStore::EntryIndex[]` sized to the full capacity of the password store (potentially 160 entries based on slot range allocation). While this is acceptable for small embedded datasets, the pattern of allocating memory proportional to the entire dataset on every command invocation could be improved for consistency with the module's own UI list implementation.

**Location**: `components/mod_password/src/PasswordModule.cpp:193-213`

```cpp
static void cmd_password_list(const char* args) {
    (void)args;
    auto& store = PasswordStore::instance();
    if (!store.hasSlotRange()) {
        cdc::serial::Console::printf("ERROR: slot map not configured\r\n");
        return;
    }
    uint16_t cap = store.capacity();
    if (cap == 0) {
        cdc::serial::Console::printf("(no entries)\r\n");
        return;
    }
    auto list = std::unique_ptr<PasswordStore::EntryIndex[]>(new (std::nothrow) PasswordStore::EntryIndex[cap]);
    if (!list) {
        cdc::serial::Console::printf("ERROR: out of memory\r\n");
        return;
    }
    uint16_t count = 0;
    if (!store.listEntriesSorted(list.get(), cap, &count)) {
        cdc::serial::Console::printf("ERROR: list failed\r\n");
        return;
    }
    // ... prints all entries
}
```

## Impact
- **Memory efficiency**: Allocates ~32 bytes × 160 = ~5KB on heap for each list command invocation. While not critical for this embedded system, this is a pattern that could scale poorly if the dataset grows.
- **Consistency**: The UI list implementation (`rebuildList()` at line 422) uses static buffers allocated once, while the serial command allocates fresh memory on each call.
- **Fragmentation risk**: Repeated allocation/deallocation of similar-sized arrays could contribute to heap fragmentation over time on long-running embedded systems.

## Evidence
1. `cmd_password_list()` at line 193 allocates `EntryIndex[cap]` where `cap` can be up to 160 entries
2. Similar pattern in `cmd_password_get()` at line 172, which also allocates the full list to resolve a single index
3. The UI list implementation at line 422 uses pre-allocated static buffers (`s_entries`, `s_listItems`)

## Recommended Fix
Consider using a callback-based iteration pattern similar to `TropicStorage::forEachSlot()` to avoid allocating the full list in memory:

```cpp
static void cmd_password_list(const char* args) {
    (void)args;
    auto& store = PasswordStore::instance();
    if (!store.hasSlotRange()) {
        cdc::serial::Console::printf("ERROR: slot map not configured\r\n");
        return;
    }
    
    struct Ctx {
        uint16_t count;
        uint16_t max;
        PasswordStore::EntryIndex entries[160]; // Fixed-size stack or static buffer
    } ctx = {0, 160};
    
    // Use existing listEntriesSorted with bounded buffer
    if (!store.listEntriesSorted(ctx.entries, ctx.max, &ctx.count)) {
        cdc::serial::Console::printf("ERROR: list failed\r\n");
        return;
    }
    
    if (ctx.count == 0) {
        cdc::serial::Console::printf("(no entries)\r\n");
        return;
    }
    
    for (uint16_t i = 0; i < ctx.count; i++) {
        cdc::serial::Console::printf("%u: %s (slot %u)\r\n", 
                                     i, ctx.entries[i].title, ctx.entries[i].slot);
    }
}
```

Alternatively, consider adding a streaming callback API to `PasswordStore` that allows iterating entries without loading them all into an intermediate array, especially if sorting can be done incrementally or if sorted order is not required for the serial command output.

## References
- Similar callback pattern already used in `TropicStorage::forEachSlot()` (components/cdc_core/include/cdc_core/TropicStorage.h:37-39)
- ESP32 memory management: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/memory.html
