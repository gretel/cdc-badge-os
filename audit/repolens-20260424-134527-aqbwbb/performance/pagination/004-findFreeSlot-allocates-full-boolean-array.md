---
title: "[LOW] findFreeSlot() allocates full boolean array for slot tracking"
severity: LOW
domain: performance/pagination
lens: pagination-streaming
labels:
  - "audit:performance/pagination"
---

## Summary
The `findFreeSlot()` function in both `PasswordStore` and `TotpStore` allocates a full boolean array sized to the complete slot capacity to track used slots. This is done for both modules with up to **362 password slots** and **100 TOTP slots**.

**File**: `components/mod_password/src/PasswordStore.cpp:158-199`
```cpp
bool PasswordStore::findFreeSlot(uint16_t* slotOut) const {
    // ...
    uint16_t cap = capacity();
    if (cap == 0) return false;
    auto used = std::unique_ptr<bool[]>(new (std::nothrow) bool[cap]);  // Full array allocation
    if (!used) return false;
    memset(used.get(), 0, cap * sizeof(bool));

    struct Ctx {
        bool* used;
        uint16_t base;
        uint16_t cap;
    } ctx = { used.get(), rmemStart_, cap };

    auto cb = [](uint16_t slot, const cdc::core::TropicStorage::CacheEntry&, void* user) {
        auto* c = static_cast<Ctx*>(user);
        if (slot < c->base) return;
        uint16_t idx = slot - c->base;
        if (idx < c->cap) {
            c->used[idx] = true;  // Mark used slots
        }
    };

    cdc::core::TropicStorage::instance().forEachSlot(
        moduleId_, rmemStart_, rmemEnd_, cb, &ctx);

    for (uint16_t i = 0; i < cap; i++) {
        if (!used[i]) {  // Scan for first free slot
            // ...
        }
    }
}
```

## Impact
- **Memory allocation**: For passwords, allocates 362 bytes; for TOTP, 100 bytes
- **Dynamic allocation**: Uses `new` on heap, which can fail on fragmented memory
- **Two-pass algorithm**: First pass marks all used slots, second pass finds first free
- **Unnecessary for sparse data**: If only a few slots are used, could find free slot faster

## Evidence
**File**: `components/mod_password/src/PasswordStore.cpp:158-199`
```cpp
bool PasswordStore::findFreeSlot(uint16_t* slotOut) const {
    if (!slotOut) return false;
    if (!hasSlotRange_) return false;

    uint16_t cap = capacity();
    if (cap == 0) return false;
    auto used = std::unique_ptr<bool[]>(new (std::nothrow) bool[cap]);  // Line 172
    if (!used) return false;
    memset(used.get(), 0, cap * sizeof(bool));

    struct Ctx {
        bool* used;
        uint16_t base;
        uint16_t cap;
    } ctx = { used.get(), rmemStart_, cap };

    auto cb = [](uint16_t slot, const cdc::core::TropicStorage::CacheEntry&, void* user) {
        auto* c = static_cast<Ctx*>(user);
        if (slot < c->base) return;
        uint16_t idx = slot - c->base;
        if (idx < c->cap) {
            c->used[idx] = true;
        }
    };

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

**File**: `components/mod_totp/src/TotpStore.cpp:194-233` (identical pattern)

## Recommended Fix
Use **bitset** or **linear scan** approach:

1. **Bitset approach** for dense slot ranges:
   ```cpp
   // Use 8x less memory with bitset
   uint8_t* used = (uint8_t*)heap_caps_malloc(
       (cap + 7) / 8, MALLOC_CAP_DEFAULT);
   // Set bit: used[slot / 8] |= (1 << (slot % 8));
   // Check bit: used[slot / 8] & (1 << (slot % 8))
   ```

2. **Linear scan without allocation** (simplest, no heap):
   ```cpp
   bool findFreeSlot(uint16_t* slotOut) const {
       if (!slotOut || !hasSlotRange_) return false;
       
       uint16_t cap = capacity();
       for (uint16_t i = 0; i < cap; i++) {
           uint16_t slot = static_cast<uint16_t>(rmemStart_ + i);
           // Check if slot is free without full array allocation
           CacheEntry entry = {};
           if (!getEntry(slot, &entry)) continue;
           if (!isEntryUsed(entry)) {
               *slotOut = slot;
               return true;
           }
       }
       return false;
   }
   ```

3. **Hybrid approach**: For small capacities (< 64), use stack-allocated bitfield; for larger, use heap

## References
- ESP32-S3 heap: Fragmentation can cause small `new` allocations to fail
- Bit manipulation: `(cap + 7) / 8` bytes for bitset
- TROPIC01 R-Memory: 512 slots total, modules use subsets
