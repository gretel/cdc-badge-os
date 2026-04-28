---
title: "[LOW] TOTP slot mapping functions lack boundary value tests"
severity: LOW
domain: mod_totp
lens: edge-cases
labels:
  - "audit:testing/edge-cases"
---

## Summary
The `TotpStore::toPhysicalSlot()` and `TotpStore::toLogicalSlot()` functions at `TotpStore.cpp:121-146` convert between logical and physical slot indices but lack edge case tests:

1. **Logical index = 0** - First valid slot
2. **Logical index = capacity()** - Exactly at boundary (should fail)
3. **Logical index = capacity() - 1** - Last valid slot
4. **Logical index = UINT16_MAX** - Maximum value
5. **Physical slot = rmemStart_** - First valid physical slot
6. **Physical slot = rmemEnd_** - Last valid physical slot
7. **Physical slot = rmemStart_ - 1** - Just before range
8. **Physical slot = rmemEnd_ + 1** - Just after range
9. **Slot range not configured** - `hasSlotRange_ = false`
10. **Null output pointer** - `slotOut = nullptr` or `logicalIndexOut = nullptr`

**Evidence** (file:line):
- `TotpStore.cpp:121-132` - `toPhysicalSlot()` function
- `TotpStore.cpp:133-142` - `toLogicalSlot()` function
- `TotpStore.cpp:90-101` - `setSlotRange()` function

```cpp
// toPhysicalSlot at TotpStore.cpp:121-132
bool TotpStore::toPhysicalSlot(uint16_t logicalIndex, uint16_t* slotOut) const {
    if (!slotOut) return false;
    if (!hasSlotRange_) return false;
    uint32_t slot = static_cast<uint32_t>(rmemStart_) + logicalIndex;
    if (slot > rmemEnd_) return false;
    *slotOut = static_cast<uint16_t>(slot);
    return true;
}
```

The cast `static_cast<uint32_t>(rmemStart_) + logicalIndex` could overflow if not bounded.

## Impact
- **Integer overflow**: `rmemStart_ + logicalIndex` could wrap if not properly bounded
- **Off-by-one errors**: Boundary slots might be incorrectly accepted/rejected
- **Silent failures**: Null pointer checks exist but aren't tested

## Evidence
No tests exist for slot mapping edge cases. Usage in `TotpStore.cpp:152-165`:
```cpp
bool TotpStore::readAccount(uint16_t slot, TotpAccount* out) {
    if (!out) return false;
    if (!hasSlotRange_) return false;
    uint16_t physSlot = 0;
    if (!toPhysicalSlot(slot, &physSlot)) return false;
    // ...
}
```

## Recommended Fix
Add edge case tests:

```cpp
void test_totp_toPhysicalSlot_boundary() {
    TotpStore store;
    store.setSlotRange(100, 109, 1); // 10 slots (100-109)
    
    uint16_t physical;
    
    // Test first valid logical index
    bool result = store.toPhysicalSlot(0, &physical);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(100, physical);
    
    // Test last valid logical index
    result = store.toPhysicalSlot(9, &physical);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(109, physical);
    
    // Test at boundary (should fail)
    result = store.toPhysicalSlot(10, &physical);
    TEST_ASSERT_FALSE(result);
    
    // Test beyond boundary
    result = store.toPhysicalSlot(11, &physical);
    TEST_ASSERT_FALSE(result);
    
    // Test UINT16_MAX
    result = store.toPhysicalSlot(UINT16_MAX, &physical);
    TEST_ASSERT_FALSE(result);
}

void test_totp_toPhysicalSlot_null_output() {
    TotpStore store;
    store.setSlotRange(100, 109, 1);
    
    bool result = store.toPhysicalSlot(0, nullptr);
    TEST_ASSERT_FALSE(result);
}

void test_totp_toPhysicalSlot_no_range() {
    TotpStore store;
    // Don't set slot range
    
    uint16_t physical;
    bool result = store.toPhysicalSlot(0, &physical);
    TEST_ASSERT_FALSE(result);
}

void test_totp_toLogicalSlot_boundary() {
    TotpStore store;
    store.setSlotRange(100, 109, 1);
    
    uint16_t logical;
    
    // Test first valid physical slot
    bool result = store.toLogicalSlot(100, &logical);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(0, logical);
    
    // Test last valid physical slot
    result = store.toLogicalSlot(109, &logical);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(9, logical);
    
    // Test before range
    result = store.toLogicalSlot(99, &logical);
    TEST_ASSERT_FALSE(result);
    
    // Test after range
    result = store.toLogicalSlot(110, &logical);
    TEST_ASSERT_FALSE(result);
}

void test_totp_toLogicalSlot_null_output() {
    TotpStore store;
    store.setSlotRange(100, 109, 1);
    
    bool result = store.toLogicalSlot(100, nullptr);
    TEST_ASSERT_FALSE(result);
}

void test_totp_setSlotRange_invalid() {
    TotpStore store;
    
    // Test start > end
    store.setSlotRange(110, 100, 1);
    TEST_ASSERT_FALSE(store.hasSlotRange());
    
    // Test start = 0
    store.setSlotRange(0, 10, 1);
    TEST_ASSERT_FALSE(store.hasSlotRange());
    
    // Test end = 0
    store.setSlotRange(10, 0, 1);
    TEST_ASSERT_FALSE(store.hasSlotRange());
}
```

## References
- Integer overflow edge cases
- Array bounds testing
- Slot mapping validation
