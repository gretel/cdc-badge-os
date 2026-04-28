---
title: "[MEDIUM] NVS storage lacks atomic multi-field update transactions"
severity: MEDIUM
domain: data-integrity
lens: database
labels:
  - nvs-storage
  - atomicity
  - crash-safety
---

## Summary
The vCard and TROPIC01 cache storage in NVS (Non-Volatile Storage) perform multi-step writes without atomic transactions. When storing a vCard or updating cache entries, the code performs separate NVS operations (open → set → commit) that can leave partial data if power is lost mid-operation.

**Files:**
- `components/mod_vcard/src/vcard_store.cpp:361-377` (vCard storage)
- `components/cdc_core/src/TropicStorage.cpp:398-408` (TROPIC01 cache chunks)

## Impact
If a power loss or reset occurs between NVS operations, the storage can become inconsistent:
- A vCard could be stored with only partial metadata cached
- TROPIC01 cache chunks could be out of sync with actual secure element contents
- On next boot, the system may see "used" slots that don't match the actual hardware state

This forces the system to rely on rebuild operations to recover, which is a fallback rather than prevention.

## Evidence
In `vcard_store.cpp:361-377`:
```cpp
nvs_handle_t nvs;
if (nvs_open(VCARD_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
    set_err(err, err_len, "NVS open failed");
    return false;
}
esp_err_t ret = nvs_set_str(nvs, VCARD_KEY_OWN, tmp);
if (ret == ESP_OK) {
    ret = nvs_commit(nvs);  // Only one key written
}
nvs_close(nvs);
```

The metadata cache (`g_cards[]`) in memory is updated separately from NVS storage:
```cpp
vcard_parse_names(tmp, g_cards[free_slot].last_name, ...);
g_cards[free_slot].used = true;
g_cards[free_slot].hash = hash;
g_card_count++;
```

If `nvs_commit()` succeeds but the system crashes before `g_cards` is populated, the next `vcard_store_init()` will reload from NVS but the runtime state may be inconsistent.

## Recommended Fix
Use a versioned storage approach with validation:

1. Add a version/sequence number to NVS storage that increments with each update
2. Store data with a "valid" flag that is cleared before write and set after commit
3. On load, verify the sequence number matches to detect partial writes

Example pattern:
```cpp
typedef struct {
    uint32_t sequence;
    uint8_t valid;
    uint8_t padding[3];
} vcard_header_t;

// Write:
nvs_set_u32(nvs, "seq", new_sequence);
nvs_set_str(nvs, "data", vcard);
nvs_set_u8(nvs, "valid", 1);
nvs_commit(nvs);

// Read:
uint8_t valid = nvs_get_u8(nvs, "valid");
uint32_t seq1 = nvs_get_u32(nvs, "seq");
char* data = nvs_get_str(nvs, "data");
uint32_t seq2 = nvs_get_u32(nvs, "seq");
if (valid && seq1 == seq2) { /* use data */ }
```

## References
- ESP-IDF NVS documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs.html
- ACID properties for embedded storage: https://en.wikipedia.org/wiki/ACID
