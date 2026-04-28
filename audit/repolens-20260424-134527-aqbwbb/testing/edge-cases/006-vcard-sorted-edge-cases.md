---
title: "[LOW] vcard_store_get_sorted() empty array and single-element edge cases"
severity: LOW
domain: mod_vcard
lens: edge-cases
labels:
  - "audit:testing/edge-cases"
---

## Summary
The `vcard_store_get_sorted()` function at `vcard_store.cpp:672-699` sorts vCard slots by last name but lacks edge case tests for:

1. **Empty vCard list** - `g_card_count = 0`, no cards stored
2. **Single vCard** - Only one card in storage
3. **Two vCards** - Minimum for sorting comparison
4. **Same last name** - Multiple cards with identical last names
5. **max_slots = 0** - Zero capacity output array
6. **max_slots = 1** - Single-element output array
7. **max_slots < card_count** - Output array smaller than data

**Evidence** (file:line):
- `vcard_store.cpp:672-699` - `vcard_store_get_sorted()` function
- `vcard_store.cpp:674` - Checks `!out_slots || max_slots == 0`

```cpp
// vcard_store_get_sorted at vcard_store.cpp:672-699
uint16_t vcard_store_get_sorted(uint16_t* out_slots, uint16_t max_slots) {
    if (!out_slots || max_slots == 0) return 0;
    vcard_store_init();

    uint16_t count = 0;
    for (uint16_t i = 0; i < VCARD_MAX_CARDS && count < max_slots; i++) {
        if (g_cards[i].used) {
            out_slots[count++] = i;
        }
    }

    // Bubble sort by last name
    for (uint16_t i = 0; i < count; i++) {
        for (uint16_t j = i + 1; j < count; j++) {
            uint16_t a = out_slots[i];
            uint16_t b = out_slots[j];
            if (strcasecmp(g_cards[a].last_name, g_cards[b].last_name) > 0) {
                uint16_t tmp = out_slots[i];
                out_slots[i] = out_slots[j];
                out_slots[j] = tmp;
            }
        }
    }
    return count;
}
```

The bubble sort at lines 688-696 has classic edge cases not tested.

## Impact
- **Sort instability**: Same last names might produce unpredictable order
- **Array bounds**: If `count > max_slots`, could overflow output array
- **Empty list handling**: Returns 0 but behavior with empty list should be verified

## Evidence
No tests exist for `vcard_store_get_sorted()` edge cases.

## Recommended Fix
Add edge case tests:

```cpp
void test_vcard_sorted_empty_list() {
    uint16_t slots[10];
    uint16_t count = vcard_store_get_sorted(slots, 10);
    TEST_ASSERT_EQUAL(0, count);
}

void test_vcard_sorted_single_card() {
    // Add one vCard
    char err[64];
    const char* vcard = "BEGIN:VCARD\nVERSION:4.0\nN:Smith;;\nFN:John Smith\nEND:VCARD\n";
    vcard_store_add(vcard, strlen(vcard), err, sizeof(err));
    
    uint16_t slots[10];
    uint16_t count = vcard_store_get_sorted(slots, 10);
    TEST_ASSERT_EQUAL(1, count);
    TEST_ASSERT_EQUAL(0, slots[0]); // Assuming first slot
}

void test_vcard_sorted_same_last_name() {
    // Add two vCards with same last name
    char err[64];
    const char* vcard1 = "BEGIN:VCARD\nVERSION:4.0\nN:Smith;John;;\nFN:John Smith\nEND:VCARD\n";
    const char* vcard2 = "BEGIN:VCARD\nVERSION:4.0\nN:Smith;Jane;;\nFN:Jane Smith\nEND:VCARD\n";
    
    vcard_store_add(vcard1, strlen(vcard1), err, sizeof(err));
    vcard_store_add(vcard2, strlen(vcard2), err, sizeof(err));
    
    uint16_t slots[10];
    uint16_t count = vcard_store_get_sorted(slots, 10);
    TEST_ASSERT_EQUAL(2, count);
    // Order should be stable for same last name
}

void test_vcard_sorted_max_slots_zero() {
    uint16_t slots[10];
    uint16_t count = vcard_store_get_sorted(slots, 0);
    TEST_ASSERT_EQUAL(0, count);
}

void test_vcard_sorted_max_slots_one() {
    // Add multiple vCards
    char err[64];
    const char* vcard1 = "BEGIN:VCARD\nVERSION:4.0\nN:Smith;;\nFN:A\nEND:VCARD\n";
    const char* vcard2 = "BEGIN:VCARD\nVERSION:4.0\nN:Jones;;\nFN:B\nEND:VCARD\n";
    vcard_store_add(vcard1, strlen(vcard1), err, sizeof(err));
    vcard_store_add(vcard2, strlen(vcard2), err, sizeof(err));
    
    uint16_t slots[1];
    uint16_t count = vcard_store_get_sorted(slots, 1);
    TEST_ASSERT_EQUAL(1, count); // Should return 1, not overflow
}

void test_vcard_sorted_null_output() {
    uint16_t count = vcard_store_get_sorted(nullptr, 10);
    TEST_ASSERT_EQUAL(0, count);
}
```

## References
- Sorting algorithm edge cases
- Bubble sort boundary conditions
- Array bounds testing
