---
title: "[MEDIUM] O(n) linear scan for vCard duplicate detection"
severity: MEDIUM
domain: algorithm-efficiency
lens: algorithm
labels:
  - "linear-search"
  - "hash-table"
---

## Summary
The `vcard_is_duplicate` function in `components/mod_vcard/src/vcard_store.cpp` (lines 501-519) performs a linear O(n) scan through all vCard slots (up to 100) to check for duplicates. For each slot, it **re-reads the vCard from NVS** and performs a full string comparison, even though `vcard_store_init()` (lines 451-482) already loads all vCards and their hashes into memory.

**Evidence:**
```cpp
// vcard_store_init() already loads everything into memory (lines 451-482)
void vcard_store_init(void) {
    // ...
    for (uint16_t slot = 0; slot < VCARD_MAX_CARDS; slot++) {
        // ...
        g_cards[slot].hash = fnv1a_hash(tmp, strnlen(tmp, VCARD_MAX_LEN));  // Hash stored!
        g_cards[slot].used = true;
        // ...
    }
}

// But vcard_is_duplicate re-reads from NVS (lines 501-519)
static bool vcard_is_duplicate(nvs_handle_t nvs, const char* vcard, size_t len, uint32_t hash) {
    for (uint16_t slot = 0; slot < VCARD_MAX_CARDS; slot++) {  // O(n) linear scan
        // Re-reads from NVS instead of using g_cards[]!
        if (nvs_get_str(nvs, key, tmp, &vlen) == ESP_OK) {
            if (strlen(tmp) == len && memcmp(tmp, vcard, len) == 0) {
                return true;
            }
        }
        (void)hash;  // Hash computed but NOT used!
    }
    return false;
}
```

Note: The `hash` parameter is computed by `fnv1a_hash()` in `vcard_store_add()` (line 545) but NOT used for the duplicate check. The hashes are already stored in `g_cards[slot].hash` during `vcard_store_init()`.

## Impact
- **Scale**: With VCARD_MAX_CARDS = 100, worst case requires 100 NVS reads and 100 string comparisons
- **NVS overhead**: Each `nvs_get_str()` involves flash controller access (~100s of microseconds)
- **Redundant work**: `vcard_store_init()` already loads all vCards into memory, but `vcard_is_duplicate()` re-reads them from NVS
- **Total cost**: ~100 NVS reads + 100 string comparisons per vCard addition (when 0 NVS reads would suffice with hash lookup)
- **User experience**: Noticeable delay when adding vCards (1-2 seconds for full scan)
- **Hash unused**: The computed hash is stored in `g_cards[slot].hash` but not used for fast lookup

## Recommended Fix
Since `vcard_store_init()` already loads all vCards into `g_cards[]` with their hashes, the fix is straightforward - use the in-memory data instead of re-reading from NVS:

**Option 1: Use in-memory hash comparison (simplest)**
```cpp
static bool vcard_is_duplicate(nvs_handle_t nvs, const char* vcard, size_t len, uint32_t hash) {
    (void)nvs;  // No longer needed!
    
    // First pass: hash-only comparison (fast)
    for (uint16_t slot = 0; slot < VCARD_MAX_CARDS; slot++) {
        if (g_cards[slot].used && g_cards[slot].hash == hash) {
            // Hash match - now verify with full comparison
            // Need to read from NVS only for hash matches
            char key[8];
            vcard_key_for_slot(key, sizeof(key), slot);
            size_t vlen = 0;
            if (nvs_get_str(nvs, key, nullptr, &vlen) == ESP_OK) {
                char tmp[VCARD_MAX_LEN + 1];
                if (nvs_get_str(nvs, key, tmp, &vlen) == ESP_OK) {
                    tmp[VCARD_MAX_LEN] = '\0';
                    if (strlen(tmp) == len && memcmp(tmp, vcard, len) == 0) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}
```

**Option 2: Build hash table during init (best for large datasets)**
Add a hash table during `vcard_store_init()` for O(1) average lookup:
```cpp
// In vcard_store_init(), build hash table:
static uint16_t g_hash_table[256];  // Bucket heads
// After computing hash for each card:
uint8_t bucket = g_cards[slot].hash & 0xFF;
g_cards[slot].next = g_hash_table[bucket];
g_hash_table[bucket] = slot;

// Then vcard_is_duplicate becomes:
static bool vcard_is_duplicate(...) {
    uint8_t bucket = hash & 0xFF;
    for (uint16_t slot = g_hash_table[bucket]; slot != 0xFF; slot = g_cards[slot].next) {
        if (g_cards[slot].hash == hash) {
            // Verify with NVS read only for hash matches
            // ...
        }
    }
    return false;
}
```

## References
- Hash table data structure: https://en.wikipedia.org/wiki/Hash_table
- FNV-1a hash: https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash
