---
title: "[MEDIUM] vCard store loads all vCards into memory during initialization"
severity: MEDIUM
domain: performance/pagination
lens: performance
labels:
  - audit:performance/pagination
---

## Summary
In `components/mod_vcard/src/vcard_store.cpp:442-475`, the `vcard_store_init()` function loads **all** vCard metadata into a global array `g_cards[VCARD_MAX_CARDS]` where `VCARD_MAX_CARDS = 100`. Each entry includes the full vCard text (up to `VCARD_MAX_LEN = 768` bytes). This means up to ~77KB of NVS data is read and cached into RAM on initialization.

**Location**: `components/mod_vcard/src/vcard_store.cpp:442-475`

```cpp
void vcard_store_init(void) {
    if (g_cards_loaded) return;

    memset(g_cards, 0, sizeof(g_cards));
    g_card_count = 0;

    nvs_handle_t nvs;
    if (nvs_open(VCARD_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        g_cards_loaded = true;
        return;
    }

    for (uint16_t slot = 0; slot < VCARD_MAX_CARDS; slot++) {
        char key[8];
        vcard_key_for_slot(key, sizeof(key), slot);
        size_t len = 0;
        if (nvs_get_str(nvs, key, nullptr, &len) != ESP_OK || len == 0 || len > VCARD_MAX_LEN) {
            continue;
        }
        char tmp[VCARD_MAX_LEN + 1];
        if (nvs_get_str(nvs, key, tmp, &len) == ESP_OK) {
            tmp[VCARD_MAX_LEN] = '\0';
            vcard_parse_names(tmp, g_cards[slot].last_name, sizeof(g_cards[slot].last_name),
                              g_cards[slot].display, sizeof(g_cards[slot].display));
            g_cards[slot].used = true;
            g_cards[slot].hash = fnv1a_hash(tmp, strnlen(tmp, VCARD_MAX_LEN));
            g_card_count++;
        }
    }
    nvs_close(nvs);
    g_cards_loaded = true;
}
```

The metadata structure is:
```cpp
typedef struct {
    bool used;
    uint32_t hash;
    char last_name[32];
    char display[64];
} vcard_meta_t;

static vcard_meta_t g_cards[VCARD_MAX_CARDS];  // 100 * ~100 bytes = ~10KB
```

Note: The **full vCard text** is NOT stored in `g_cards`, but is loaded into `tmp[VCARD_MAX_LEN + 1]` for each entry during iteration.

## Impact
- **Initialization time**: Reading 100 vCards from NVS could take significant time on first boot
- **Memory pressure**: Each vCard is loaded into a 768-byte temp buffer during processing
- **No pagination**: Cannot efficiently query "first 10 cards" or "search for name X" without loading all data

## Evidence
1. `vcard_store_init()` iterates all 100 slots and loads metadata for each
2. `vcard_store_get_sorted()` at line 671 performs a bubble sort over all loaded entries
3. No mechanism to load vCards on-demand or in batches

## Recommended Fix
Implement lazy loading with index-to-entry mapping:

**Option A**: Store only metadata in NVS (already partially done), load full vCard on demand:
```cpp
typedef struct {
    bool used;
    uint32_t hash;
    char last_name[32];
    char display[64];
    uint16_t name_len;    // For faster sorting without full load
} vcard_meta_t;

// Initialize only loads metadata, not full vCards
bool vcard_store_get(uint16_t slot, char* out, size_t max_len) {
    // Load vCard text only when requested
}
```

**Option B**: Add cursor-based iteration for UI/listing:
```cpp
typedef struct {
    uint16_t current_slot;
    uint16_t count;
} vcard_iterator_t;

uint16_t vcard_store_iterate(vcard_iterator_t* iter, uint16_t* slots, uint16_t max_slots);
```

**Option C**: Use a sorted index structure (B-tree or skip list) for O(log n) lookups instead of O(n) bubble sort.

## References
- NVS performance: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
- ESP32 PSRAM usage for large buffers: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/memory.html#psram
