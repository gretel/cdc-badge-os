---
title: "[LOW] FIDO2 credential lookup scans full cache for every search"
severity: LOW
domain: performance/pagination
lens: performance
labels:
  - audit:performance/pagination
---

## Summary
In `components/mod_fido2/src/fido2_storage.cpp:500-545`, the `fido2_storage_find_by_rp()` and `fido2_storage_find_by_rp_resident()` functions perform linear scans over the entire credential cache (up to `FIDO2_MAX_CREDENTIALS = 27` entries) to find matching credentials. While the dataset is small, this pattern could be optimized with indexed lookups.

**Location**: `components/mod_fido2/src/fido2_storage.cpp:500-545`

```cpp
uint8_t fido2_storage_find_by_rp(const uint8_t *rp_id_hash,
                                  uint8_t *out_slots, uint8_t max_slots) {
    uint8_t count = 0;

    uint16_t total = ecc_count();
    for (uint8_t i = 0; i < total && i < FIDO2_MAX_CREDENTIALS && count < max_slots; i++) {
        if (g_storage.creds[i].valid &&
            memcmp(g_storage.creds[i].rp_id_hash, rp_id_hash, 32) == 0) {
            out_slots[count++] = i;
        }
    }

    return count;
}
```

## Impact
- **Scalability**: O(n) search per lookup. Fine for 27 credentials but would degrade with more.
- **Multiple scans**: If searching by RP ID, user handle, and other criteria, multiple full scans occur.
- **No index**: RP ID hashes are stored but not indexed for fast lookup.

## Evidence
1. `fido2_storage_find_by_rp()` at line 500 scans all credentials
2. `fido2_storage_find_by_rp_resident()` at line 529 also scans all credentials
3. `fido2_storage_find_by_rp_user()` at line 561 does another full scan
4. Each credential lookup for authentication requires at least one full scan

## Recommended Fix
Add an index structure for common lookup patterns:

**Option A**: Hash index for RP ID lookups:
```cpp
struct rp_index_entry_t {
    uint8_t rp_id_hash[32];
    uint8_t slot;
    uint8_t next;  // For collision chaining
};

static rp_index_entry_t g_rp_index[FIDO2_MAX_CREDENTIALS];
static uint8_t g_rp_index_head[256];  // Hash table with 256 buckets

// Build index on init
// Lookup becomes O(1) average
```

**Option B**: Use a simple sorted array with binary search:
```cpp
// Keep creds sorted by rp_id_hash
// Binary search for O(log n) lookup
```

**Option C** (simplest for current scale): Cache the last RP ID search result:
```cpp
static struct {
    uint8_t rp_id_hash[32];
    uint8_t slots[27];
    uint8_t count;
    uint32_t last_access;
} g_rp_cache = {};

// Return cached result if RP ID matches and cache not stale
```

## References
- FIDO2 specification on resident keys: https://fidoalliance.org/specs/fido-v2.1-rd-20210309/fido-client-to-authenticator-protocol-v2.1-rd-20210309.html#sctn-resident-creds
- ESP32 memory constraints: ~320KB SRAM total for all use
