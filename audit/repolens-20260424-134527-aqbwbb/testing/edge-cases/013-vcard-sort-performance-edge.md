---
title: "[MEDIUM] Bubble sort O(n²) complexity in vcard_store_get_sorted for large card counts"
severity: MEDIUM
domain: vcard
lens: edge-cases
labels:
  - "audit:testing/edge-cases"
---

## Summary
The `vcard_store_get_sorted` function in `components/mod_vcard/src/vcard_store.cpp:676-699` uses a bubble sort algorithm with O(n²) time complexity. With `VCARD_MAX_CARDS` potentially being 32, this results in up to ~1000 comparisons in the worst case, which could cause noticeable UI delays on the ESP32-S3 when the vCard list is large.

## Impact
- **Performance**: For n=32 cards, bubble sort performs ~500 comparisons (n*(n-1)/2). On ESP32-S3 at 240MHz with string comparisons, this could take 10-50ms, causing visible UI lag.
- **Scalability**: If `VCARD_MAX_CARDS` is increased in the future, performance degrades quadratically.
- **User Experience**: Sorting happens synchronously during `vcard_store_get_sorted`, blocking the main thread.

## Evidence
File: `components/mod_vcard/src/vcard_store.cpp:676-699`

```cpp
uint16_t vcard_store_get_sorted(uint16_t* out_slots, uint16_t max_slots) {
    if (!out_slots || max_slots == 0) return 0;
    vcard_store_init();

    uint16_t count = 0;
    for (uint16_t i = 0; i < VCARD_MAX_CARDS && count < max_slots; i++) {
        if (g_cards[i].used) {
            out_slots[count++] = i;
        }
    }

    // Bubble sort - O(n²) complexity
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

The nested loop structure performs `count * (count-1) / 2` comparisons. For 32 cards, this is 496 comparisons, each involving a `strcasecmp` call.

## Recommended Fix
Replace the bubble sort with a more efficient sorting algorithm. Since this is C++17 and the codebase uses standard library headers, use `std::sort` with a custom comparator:

```cpp
uint16_t vcard_store_get_sorted(uint16_t* out_slots, uint16_t max_slots) {
    if (!out_slots || max_slots == 0) return 0;
    vcard_store_init();

    uint16_t count = 0;
    for (uint16_t i = 0; i < VCARD_MAX_CARDS && count < max_slots; i++) {
        if (g_cards[i].used) {
            out_slots[count++] = i;
        }
    }

    // Use std::sort for O(n log n) complexity
    std::sort(out_slots, out_slots + count, [](uint16_t a, uint16_t b) {
        return strcasecmp(g_cards[a].last_name, g_cards[b].last_name) < 0;
    });

    return count;
}
```

If C++17 `<algorithm>` is not available, implement a simple insertion sort for small arrays (n < 32), which is O(n²) but has better constant factors for small n:

```cpp
// Insertion sort - better for small arrays
for (uint16_t i = 1; i < count; i++) {
    uint16_t key = out_slots[i];
    uint16_t j = i - 1;
    while (j >= 0 && strcasecmp(g_cards[out_slots[j]].last_name, 
           g_cards[key].last_name) > 0) {
        out_slots[j + 1] = out_slots[j];
        j--;
    }
    out_slots[j + 1] = key;
}
```

## References
- [Bubble Sort vs Insertion Sort](https://en.wikipedia.org/wiki/Insertion_sort) - Insertion sort has ~2x fewer swaps on average
- [std::sort complexity](https://en.cppreference.com/w/cpp/algorithm/sort) - O(n log n) average case
- ESP32-S3 performance: 240MHz dual-core, but string comparisons are CPU-intensive
