---
title: "[LOW] vcard_store_delete() slot boundary and error return edge cases"
severity: LOW
domain: mod_vcard
lens: edge-cases
labels:
  - "audit:testing/edge-cases"
---

## Summary
The `vcard_store_delete()` function at `vcard_store.cpp:596-616` has edge cases around slot validation and error handling:

1. **Slot = VCARD_MAX_CARDS - 1** - Maximum valid slot index
2. **Slot = VCARD_MAX_CARDS** - Exactly at boundary (should fail)
3. **Slot = VCARD_MAX_CARDS + 1** - Beyond boundary
4. **Slot = UINT16_MAX** - Maximum uint16_t value (overflow check)
5. **Delete already deleted slot** - Slot not used
6. **NVS open failure** - What happens when NVS returns error?

**Evidence** (file:line):
- `vcard_store.cpp:596-616` - `vcard_store_delete()` function
- `vcard_store.h:6-7` - `#define VCARD_MAX_CARDS 100`

```cpp
// vcard_store_delete at vcard_store.cpp:596-600
bool vcard_store_delete(uint16_t slot) {
    vcard_store_init();
    if (slot >= VCARD_MAX_CARDS || !g_cards[slot].used) {
        return false;
    }
```

The check `slot >= VCARD_MAX_CARDS` handles the boundary but doesn't test:
- `slot = 99` (last valid)
- `slot = 100` (first invalid)
- `slot = 65535` (UINT16_MAX)

## Impact
- **Silent failures**: Incorrect slot indices silently return false
- **Potential buffer overflow**: If check fails, could access `g_cards[UINT16_MAX]`

## Evidence
No tests exist for `vcard_store_delete()` edge cases. Current usage in tests is minimal.

## Recommended Fix
Add edge case tests:

```cpp
void test_vcard_delete_boundary_slots() {
    // Test deleting at maximum valid slot
    bool result = vcard_store_delete(99); // VCARD_MAX_CARDS - 1
    // Should return false if slot not used
    
    // Test deleting at boundary
    result = vcard_store_delete(100); // VCARD_MAX_CARDS
    TEST_ASSERT_FALSE(result);
    
    // Test deleting beyond boundary
    result = vcard_store_delete(101);
    TEST_ASSERT_FALSE(result);
    
    // Test deleting at UINT16_MAX
    result = vcard_store_delete(UINT16_MAX);
    TEST_ASSERT_FALSE(result);
}

void test_vcard_delete_unused_slot() {
    // Delete slot that was never used
    bool result = vcard_store_delete(50);
    TEST_ASSERT_FALSE(result);
}

void test_vcard_delete_same_slot_twice() {
    // Add a vCard, delete it, then delete again
    char err[64];
    const char* vcard = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
    
    // First delete should work if slot is used
    bool first = vcard_store_delete(0);
    
    // Second delete should fail (already deleted)
    bool second = vcard_store_delete(0);
    TEST_ASSERT_FALSE(second);
}
```

## References
- Array boundary testing
- C/C++ integer overflow edge cases
