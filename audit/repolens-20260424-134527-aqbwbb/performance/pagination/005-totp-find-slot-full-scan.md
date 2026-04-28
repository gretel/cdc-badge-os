---
title: "[LOW] TOTP findFreeSlot scans all slots using callback iteration"
severity: LOW
domain: performance/pagination
lens: performance
labels:
  - audit:performance/pagination
---

## Summary
In `components/mod_totp/src/TotpStore.cpp:194-229`, the `findFreeSlot()` function uses `TropicStorage::forEachSlot()` to iterate all TOTP slots and build a boolean array tracking used slots. This callback-based approach is clean but incurs overhead of calling a callback for each slot.

**Location**: `components/mod_totp/src/TotpStore.cpp:194-229`

```cpp
bool TotpStore::findFreeSlot(uint16_t* slotOut) {
    if (!slotOut) return false;
    if (!hasSlotRange_) return false;

    uint16_t cap = capacity();
    if (cap == 0) return false;
    auto used = std::unique_ptr<bool[]>(new (std::nothrow) bool[cap]);
    if (!used) return false;
    memset(used.get(), 0, cap * sizeof(bool));
    struct Ctx {
        bool* used;
        uint16_t base;
        uint16_t cap;
    } ctx = { used.get(), 0, cap };

    auto cb = [](uint16_t slot, const cdc::core::TropicStorage::CacheEntry&, void* user) {
        auto* c = static_cast<Ctx*>(user);
        if (slot < c->base) return;
        uint16_t idx = slot - c->base;
        if (idx < c->cap) {
            c->used[idx] = true;
        }
    };

    ctx.base = rmemStart_;
    cdc::core::TropicStorage::instance().forEachSlot(
        moduleId_, rmemStart_, rmemEnd_, cb, &ctx);

    for (uint16_t i = 0; i < cap; i++) {
        if (!used[i]) {
            uint16_t candidate = static_cast<uint16_t>(rmemStart_ + i);
            if (candidate <= rmemEnd_) {
                *slotOut = candidate;
                return true;
            }
            return false;
        }
    }

    return false;
}
```

## Impact
- **Memory allocation**: Allocates dynamic array of `cap` bytes (up to 100-160 bytes)
- **Callback overhead**: Function call for each slot in range
- **Two passes**: First pass to mark used slots, second pass to find free slot

## Evidence
1. `findFreeSlot()` allocates `bool[cap]` where cap can be 100-160 entries
2. Similar pattern in `PasswordStore::findFreeSlot()` at line 156-199
3. Uses callback-based iteration through `TropicStorage::forEachSlot()`

## Recommended Fix
Use a simpler direct iteration over the slot cache without dynamic allocation:

```cpp
bool TotpStore::findFreeSlot(uint16_t* slotOut) {
    if (!slotOut) return false;
    if (!hasSlotRange_) return false;

    uint16_t cap = capacity();
    if (cap == 0) return false;

    // Use stack-allocated bitmap for small capacities
    uint8_t used_bitmap[(160 + 7) / 8] = {0};  // 20 bytes for 160 slots

    auto cb = [](uint16_t slot, const cdc::core::TropicStorage::CacheEntry&, void* user) {
        uint8_t* bitmap = static_cast<uint8_t*>(user);
        uint16_t idx = slot - rmemStart_;  // Capture rmemStart_ from outer scope
        if (idx < cap) {
            bitmap[idx / 8] |= (1 << (idx % 8));
        }
    };

    cdc::core::TropicStorage::instance().forEachSlot(
        moduleId_, rmemStart_, rmemEnd_, cb, used_bitmap);

    // Find first free slot using bit scan
    for (uint16_t i = 0; i < cap; i++) {
        if (!(used_bitmap[i / 8] & (1 << (i % 8)))) {
            *slotOut = static_cast<uint16_t>(rmemStart_ + i);
            return true;
        }
    }

    return false;
}
```

**Alternative**: Cache the free-slot bitmap and update incrementally on add/delete operations.

## References
- Bit manipulation for memory efficiency: https://en.wikipedia.org/wiki/Bit_array
- ESP32 stack size limits: Default task stack is often 4096 bytes
