---
title: "[LOW] Unnecessary dynamic allocation in TotpStore::findFreeSlot() - heap allocation for simple iteration"
severity: LOW
domain: mod_totp
lens: algorithm-efficiency
labels:
  - "memory-allocation"
  - "unnecessary-copy"
---

## Summary
In `components/mod_totp/src/TotpStore.cpp`, the `findFreeSlot()` function (lines 195-235) uses **dynamic heap allocation** to create a `bool[]` array tracking used slots, then iterates through it. This requires:
1. `new (std::nothrow) bool[cap]` - heap allocation
2. `memset()` to zero the array
3. Callback-based population from `TropicStorage::forEachSlot()`
4. Linear scan of the `used[]` array

For a typical TOTP configuration with 100 slots, this allocates 100 bytes on the heap and performs **two passes** over the data: one via callback, one via linear scan.

## Impact
- **Memory**: Dynamic allocation on ESP32 with limited heap (typically 200-300KB available)
- **Fragmentation**: Repeated allocations/deallocations can fragment heap
- **Latency**: Heap allocation can take 10-50us depending on allocator state
- **Complexity**: Two-pass algorithm when single-pass would suffice

## Evidence
**File: `components/mod_totp/src/TotpStore.cpp`**

Lines 195-235:
```cpp
bool TotpStore::findFreeSlot(uint16_t* slotOut) {
    if (!slotOut) return false;
    if (!hasSlotRange_) return false;

    uint16_t cap = capacity();
    if (cap == 0) return false;
    auto used = std::unique_ptr<bool[]>(new (std::nothrow) bool[cap]);  // Heap alloc
    if (!used) return false;
    memset(used.get(), 0, cap * sizeof(bool));  // Zero initialization
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
        moduleId_, rmemStart_, rmemEnd_, cb, &ctx);  // First pass via callback

    for (uint16_t i = 0; i < cap; i++) {  // Second pass - find first free
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

## Recommended Fix
**Option 1: Single-pass algorithm with early exit**
If `TropicStorage::forEachSlot()` supports early termination, use it directly:
```cpp
bool TotpStore::findFreeSlot(uint16_t* slotOut) {
    if (!slotOut || !hasSlotRange_) return false;

    uint16_t cap = capacity();
    if (cap == 0) return false;

    // Single pass: check each slot as we iterate
    uint16_t freeSlot = 0;
    bool found = false;
    
    auto cb = [](uint16_t slot, const cdc::core::TropicStorage::CacheEntry&, void* user) -> bool {
        auto* ctx = static_cast<std::pair<uint16_t, bool>*>(user);
        if (ctx->second) return true;  // Already found
        
        // Check if this is a consecutive sequence starting from base
        if (ctx->first == 0) {
            ctx->first = slot;
            ctx->second = true;
        }
        return false;
    };
    
    // ... depends on TropicStorage API
}
```

**Option 2: Stack-allocated bitfield for small ranges**
If `cap` is bounded (e.g., max 100 TOTP slots), use a fixed stack array:
```cpp
bool TotpStore::findFreeSlot(uint16_t* slotOut) {
    if (!slotOut || !hasSlotRange_) return false;

    uint16_t cap = capacity();
    if (cap == 0) return false;
    
    // Stack allocation - no heap overhead
    static constexpr uint16_t MAX_TOTP_SLOTS = 100;
    if (cap > MAX_TOTP_SLOTS) return false;  // Sanity check
    
    bool used[MAX_TOTP_SLOTS] = {};  // Stack, zero-initialized
    
    // ... rest same as original
}
```

**Option 3: Bitfield instead of bool array**
Use bit-packed storage for better cache efficiency:
```cpp
bool TotpStore::findFreeSlot(uint16_t* slotOut) {
    if (!slotOut || !hasSlotRange_) return false;

    uint16_t cap = capacity();
    if (cap == 0) return false;
    
    // Bitfield: 100 slots = 13 bytes instead of 100 bytes
    static constexpr uint16_t MAX_TOTP_SLOTS = 100;
    uint8_t used_bits[(MAX_TOTP_SLOTS + 7) / 8] = {};
    
    // ... populate bits
    // Check: (used_bits[i / 8] >> (i % 8)) & 1
}
```

**Option 4: Simplify if slot numbering is dense**
If slots are always allocated sequentially (0, 1, 2, ...), track `next_free_slot`:
```cpp
struct TotpStore {
    uint16_t next_free_slot_;  // Cached hint
    
    bool findFreeSlot(uint16_t* slotOut) {
        if (!slotOut || !hasSlotRange_) return false;
        
        // Start search from last known free slot
        uint16_t start = next_free_slot_;
        for (uint16_t i = 0; i < capacity(); i++) {
            uint16_t slot = (start + i) % capacity();
            if (!slotUsed(slot)) {
                next_free_slot_ = (slot + 1) % capacity();
                *slotOut = slot;
                return true;
            }
        }
        return false;
    }
}
```

## References
- **Time Complexity**: O(n) with 2 passes → O(n) with 1 pass
- **Space Complexity**: O(n) heap allocation → O(n) stack or O(n/8) bitfield
- **Pattern**: "Unnecessary heap allocation" - use stack for small fixed-size arrays
- **ESP32**: Heap fragmentation is a real concern with small, frequent allocations

---

</content>