---
title: "[LOW] vcard_store uses FNV-1a hash for duplicate detection but doesn't handle collisions"
severity: LOW
domain: mod_vcard/vcard_store
lens: edge-case-testing
labels:
  - "audit:testing/edge-cases"
---

## Summary
In `vcard_store.cpp` (file: `components/mod_vcard/src/vcard_store.cpp:43-50`), the FNV-1a hash is used for tracking duplicate vCards, but the hash is stored and compared without handling potential collisions.

Lines 43-50:
```cpp
static uint32_t fnv1a_hash(const char* data, size_t len) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        hash ^= static_cast<uint8_t>(data[i]);
        hash *= 16777619u;
    }
    return hash;
}
```

The hash is stored in `g_cards[slot].hash` and used in `vcard_is_duplicate()` at lines 499-519, but collision detection relies on full string comparison, which is correct but the hash itself is not used for fast lookup.

## Impact
- **Performance**: Hash doesn't provide O(1) lookup; full string comparison is always done
- **Collision handling**: If two different vCards have the same hash, the comparison at line 514-516 will correctly identify them as different, but this is not explicit in the code

## Evidence
File: `components/mod_vcard/src/vcard_store.cpp`, lines 499-519

```cpp
static bool vcard_is_duplicate(nvs_handle_t nvs, const char* vcard, size_t len, uint32_t hash) {
    for (uint16_t slot = 0; slot < VCARD_MAX_CARDS; slot++) {
        char key[8];
        vcard_key_for_slot(key, sizeof(key), slot);
        size_t vlen = 0;
        if (nvs_get_str(nvs, key, nullptr, &vlen) != ESP_OK || vlen == 0 || vlen > VCARD_MAX_LEN) {
            continue;
        }
        char tmp[VCARD_MAX_LEN + 1];
        if (nvs_get_str(nvs, key, tmp, &len) == ESP_OK) {  // Note: uses len, not vlen!
            tmp[VCARD_MAX_LEN] = '\0';
            if (strlen(tmp) == len && memcmp(tmp, vcard, len) == 0) {  // Line 514-516
                return true;
            }
        }
        (void)hash;  // Hash is unused!
    }
    return false;
}
```

The `hash` parameter at line 500 is never used (marked with `(void)hash;` at line 518). The function always does a full string comparison.

## Recommended Fix
Use the hash for fast pre-filtering before expensive string comparison:

```cpp
static bool vcard_is_duplicate(nvs_handle_t nvs, const char* vcard, size_t len, uint32_t hash) {
    for (uint16_t slot = 0; slot < VCARD_MAX_CARDS; slot++) {
        char key[8];
        vcard_key_for_slot(key, sizeof(key), slot);
        size_t vlen = 0;
        if (nvs_get_str(nvs, key, nullptr, &vlen) != ESP_OK || vlen == 0 || vlen > VCARD_MAX_LEN) {
            continue;
        }
        char tmp[VCARD_MAX_LEN + 1];
        if (nvs_get_str(nvs, key, tmp, &vlen) == ESP_OK) {
            tmp[VCARD_MAX_LEN] = '\0';
            
            // Fast hash check first
            uint32_t stored_hash = fnv1a_hash(tmp, vlen);
            if (stored_hash != hash) {
                continue;  // Different hash, skip string comparison
            }
            
            // Hash matches - do full comparison
            if (vlen == len && memcmp(tmp, vcard, len) == 0) {
                return true;
            }
        }
    }
    return false;
}
```

Also update the `vcard_meta_t` struct to store the hash correctly:
```cpp
typedef struct {
    bool used;
    uint32_t hash;
    char last_name[32];
    char display[64];
} vcard_meta_t;
```

And when loading cards, compute and store the hash:
```cpp
void vcard_store_init(void) {
    // ...
    for (uint16_t slot = 0; slot < VCARD_MAX_CARDS; slot++) {
        // ...
        if (nvs_get_str(nvs, key, tmp, &len) == ESP_OK) {
            tmp[VCARD_MAX_LEN] = '\0';
            vcard_parse_names(tmp, g_cards[slot].last_name, sizeof(g_cards[slot].last_name),
                              g_cards[slot].display, sizeof(g_cards[slot].display));
            g_cards[slot].used = true;
            g_cards[slot].hash = fnv1a_hash(tmp, strnlen(tmp, VCARD_MAX_LEN));
            g_card_count++;
        }
    }
    // ...
}
```

## References
- FNV-1a hash algorithm: http://www.isthe.com/chongo/tech/comp/fnv/
- CWE-131: Incorrect Calculation of Multi-Byte String Length
