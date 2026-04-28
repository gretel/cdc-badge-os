---
title: "[LOW] TOTP findFreeSlot() lacks tests for full and nearly-full slot scenarios"
severity: LOW
domain: mod_totp
lens: edge-cases
labels:
  - "audit:testing/edge-cases"
---

## Summary
The `TotpStore::findFreeSlot()` function at `TotpStore.cpp:190-232` finds free slots but lacks edge case tests:

1. **All slots full** - No free slot available
2. **Only first slot free** - Slot 0 available
3. **Only last slot free** - Last slot available
4. **Slot range not configured** - `hasSlotRange_ = false`
5. **Capacity = 0** - Empty slot range
6. **Null output pointer** - `slotOut = nullptr`
7. **Single slot range** - Only one slot available
8. **TropicStorage cache mismatch** - Cache doesn't match actual state

**Evidence** (file:line):
- `TotpStore.cpp:190-232` - `findFreeSlot()` function
- `TotpStore.cpp:90-101` - `setSlotRange()` function

```cpp
// findFreeSlot at TotpStore.cpp:190-232
bool TotpStore::findFreeSlot(uint16_t* slotOut) {
    if (!slotOut) return false;
    if (!hasSlotRange_) return false;

    uint16_t cap = capacity();
    if (cap == 0) return false;
    auto used = std::unique_ptr<bool[]>(new (std::nothrow) bool[cap]);
    if (!used) return false;
    memset(used.get(), 0, cap * sizeof(bool));
    // ...
    
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

The function uses dynamic allocation which could fail, and the loop has edge cases at boundaries.

## Impact
- **Allocation failure**: `new (std::nothrow) bool[cap]` could return nullptr for large cap
- **Off-by-one**: The check `candidate <= rmemEnd_` might be wrong if range is large
- **Silent failure**: No error indication when all slots are full

## Evidence
No tests exist for `findFreeSlot()` edge cases. Used in `addAccount()` at line 253.

## Recommended Fix
Add edge case tests:

```cpp
void test_totp_findFreeSlot_all_full() {
    TotpStore store;
    store.setSlotRange(100, 109, 1); // 10 slots
    
    // Simulate all slots filled (mock TropicStorage)
    // Then test findFreeSlot returns false
    uint16_t slot;
    bool result = store.findFreeSlot(&slot);
    TEST_ASSERT_FALSE(result);
}

void test_totp_findFreeSlot_first_slot() {
    TotpStore store;
    store.setSlotRange(100, 109, 1);
    
    // All slots filled except first
    uint16_t slot;
    bool result = store.findFreeSlot(&slot);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(100, slot);
}

void test_totp_findFreeSlot_last_slot() {
    TotpStore store;
    store.setSlotRange(100, 109, 1);
    
    // All slots filled except last
    uint16_t slot;
    bool result = store.findFreeSlot(&slot);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(109, slot);
}

void test_totp_findFreeSlot_null_output() {
    TotpStore store;
    store.setSlotRange(100, 109, 1);
    
    bool result = store.findFreeSlot(nullptr);
    TEST_ASSERT_FALSE(result);
}

void test_totp_findFreeSlot_no_range() {
    TotpStore store;
    // Don't set slot range
    
    uint16_t slot;
    bool result = store.findFreeSlot(&slot);
    TEST_ASSERT_FALSE(result);
}

void test_totp_findFreeSlot_capacity_zero() {
    TotpStore store;
    store.setSlotRange(100, 100, 1); // Single slot
    
    uint16_t slot;
    bool result = store.findFreeSlot(&slot);
    // Should work for single slot
    TEST_ASSERT_TRUE(result);
}

void test_totp_findFreeSlot_single_slot_range() {
    TotpStore store;
    store.setSlotRange(100, 100, 1); // Exactly one slot
    
    uint16_t slot;
    bool result = store.findFreeSlot(&slot);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(100, slot);
}

void test_totp_findFreeSlot_large_range() {
    TotpStore store;
    store.setSlotRange(100, 199, 1); // 100 slots
    
    uint16_t slot;
    bool result = store.findFreeSlot(&slot);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(slot >= 100 && slot <= 199);
}
```

## References
- Memory allocation edge cases
- Slot management algorithms
