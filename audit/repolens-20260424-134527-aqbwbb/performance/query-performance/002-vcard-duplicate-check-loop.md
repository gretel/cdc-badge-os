---
title: "[MEDIUM] vCard duplicate check performs O(n) NVS reads in tight loop"
severity: MEDIUM
domain: embedded-storage
lens: query-performance
labels:
  - "nvs-lookup"
  - "duplicate-check"
  - "vcard"
---

## Summary

In `components/mod_vcard/src/vcard_store.cpp`, the `vcard_is_duplicate()` function iterates through all 100 vCard slots (VCARD_MAX_CARDS) and performs an NVS read (`nvs_get_str`) for each slot to check for duplicates. This results in up to 100 NVS reads per `vcard_store_add()` call.

**Location**: `components/mod_vcard/src/vcard_store.cpp:501-519`

## Impact

**Performance Cost**:
- Worst case: 100 NVS reads for each vCard addition
- Each NVS read involves flash access (~1ms latency)
- Total worst-case delay: ~100ms per vCard add operation
- The duplicate check reads the FULL vCard content for comparison, not just metadata

**Memory Bandwidth**:
- Each NVS read fetches up to VCARD_MAX_LEN (2048 bytes) of data
- In worst case with 100 cards, up to 200KB of flash data read per add operation

**Scalability**:
- As vCard list grows, duplicate check time increases linearly
- No early exit optimization when duplicate is found (actually does break, but still reads all slots in worst case)

## Evidence

**Inefficient pattern in `vcard_is_duplicate()` (lines 501-519)**:
```cpp
static bool vcard_is_duplicate(nvs_handle_t nvs, const char* vcard, size_t len, uint3_t hash) {
    for (uint16_t slot = 0; slot < VCARD_MAX_CARDS; slot++) {
        char key[8];
        vcard_key_for_slot(key, sizeof(key), slot);
        size_t vlen = 0;
        if (nvs_get_str(nvs, key, nullptr, &vlen) != ESP_OK || vlen == 0 || vlen > VCARD_MAX_LEN) {
            continue;
        }
        char tmp[VCARD_MAX_LEN + 1];
        if (nvs_get_str(nvs, key, tmp, &vlen) == ESP_OK) {  // SECOND read for same key!
            tmp[VCARD_MAX_LEN] = '\0';
            if (strlen(tmp) == len && memcmp(tmp, vcard, len) == 0) {
                return true;
            }
        }
        (void)hash;  // Hash pre-computed but not used for comparison!
    }
    return false;
}
```

**Called from `vcard_store_add()` (lines 529-589)**:
```cpp
bool vcard_store_add(const char* vcard, size_t len, char* err, size_t err_len) {
    // ... validation ...
    vcard_store_init();  // Loads metadata cache
    // ...

    uint32_t hash = fnv1a_hash(vcard, len);
    if (vcard_is_duplicate(nvs, vcard, len, hash)) {  // Triggers up to 100 NVS reads!
        // ...
    }
    // ...
}
```

**Metadata already cached but not used**:
- `vcard_store_init()` (lines 451-482) already loads vCard metadata including hash into `g_cards[]`
- Each cached entry has `g_cards[slot].hash` and `g_cards[slot].used`
- The duplicate check ignores this cache and reads from NVS again

## Recommended Fix

**Use the cached metadata for duplicate check**:

The `vcard_store_init()` function already computes and caches FNV-1a hashes for all vCards. Use this cache instead of re-reading from NVS:

```cpp
static bool vcard_is_duplicate_cached(const char* vcard, size_t len, uint32_t hash) {
    // First, quick check using cached hashes (O(1) comparison)
    for (uint16_t slot = 0; slot < VCARD_MAX_CARDS; slot++) {
        if (!g_cards[slot].used) continue;
        
        // Fast hash comparison first
        if (g_cards[slot].hash == hash) {
            // Hash match - do full comparison using cached data if available
            // or read from NVS only for potential matches
            char tmp[VCARD_MAX_LEN + 1];
            size_t vlen = strnlen(g_cards[slot].display, sizeof(g_cards[slot].display));
            // For full accuracy, read NVS only for hash-matching entries
            // (typically much fewer than 100)
        }
    }
    return false;
}
```

**Even better**: Store the full vCard hash in the cache and use it for exact duplicate detection. Since FNV-1a has very low collision probability for typical vCard sizes, this provides near-perfect duplicate detection with zero NVS reads.

**Implementation steps** (~1 hour):
1. Modify `vcard_is_duplicate()` to use `g_cards[slot].hash` for fast pre-filtering
2. Only read NVS for slots where hash matches (typically 0-2 slots)
3. Keep the full content comparison as a fallback for hash collisions

## References

- [FNV-1a Hash](https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function) - FNV-1a has collision rate of ~2^-32 for same-length inputs
- [NVS Flash Characteristics](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html) - NVS reads are synchronous and block execution
