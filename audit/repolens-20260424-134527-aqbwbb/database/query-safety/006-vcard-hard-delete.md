---
title: "[LOW] vCard storage uses hard delete with no recovery mechanism"
severity: LOW
domain: database
lens: query-safety
labels:
  - "audit:database/query-safety"
---

## Summary
The vCard storage in `components/mod_vcard/src/vcard_store.cpp` uses hard delete (`nvs_erase_key`) for peer vCards at line 598-609. Once deleted, vCard data is permanently lost with no soft-delete flag or recovery mechanism.

## Impact
- **Permanent Data Loss**: Deleted vCards cannot be recovered
- **No Audit Trail**: No way to track when vCards were deleted
- **User Experience**: Accidental deletion means re-entry of all vCard data

## Evidence
File: `components/mod_vcard/src/vcard_store.cpp`

Lines 596-614 (vcard_store_delete):
```cpp
bool vcard_store_delete(uint16_t slot) {
    vcard_store_init();
    if (slot >= VCARD_MAX_CARDS || !g_cards[slot].used) {
        return false;
    }
    nvs_handle_t nvs;
    if (nvs_open(VCARD_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        return false;
    }
    char key[8];
    vcard_key_for_slot(key, sizeof(key), slot);
    esp_err_t ret = nvs_erase_key(nvs, key);  // Hard delete!
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs);
    }
    nvs_close(nvs);
    if (ret != ESP_OK) return false;
    g_cards[slot].used = false;
    g_card_count--;
    return true;
}
```

The vCard structure (lines 23-29):
```cpp
typedef struct {
    bool used;
    uint32_t hash;
    char last_name[32];
    char display[64];
} vcard_meta_t;
```

No `deleted_at` or similar field exists for soft-delete support.

## Recommended Fix
1. **Add soft-delete field** to metadata:
```cpp
typedef struct {
    bool used;
    uint32_t hash;
    char last_name[32];
    char display[64];
    uint32_t deleted_at;  // Unix timestamp, 0 = not deleted
} vcard_meta_t;
```

2. **Update delete function**:
```cpp
bool vcard_store_delete(uint1_t slot) {
    // ...
    // Instead of nvs_erase_key, mark as deleted
    g_cards[slot].used = true;  // Keep slot allocated
    g_cards[slot].deleted_at = time(nullptr);  // Mark deletion time
    // Update NVS with new metadata
    // ...
}
```

3. **Filter deleted cards** from queries:
```cpp
uint16_t vcard_store_count(void) {
    vcard_store_init();
    uint16_t count = 0;
    for (uint16_t i = 0; i < VCARD_MAX_CARDS; i++) {
        if (g_cards[i].used && g_cards[i].deleted_at == 0) {
            count++;
        }
    }
    return count;
}
```

4. **Add compact function** to permanently remove old deleted cards:
```cpp
bool vcard_store_compact(void) {
    // Permanently delete cards deleted more than X days ago
    // Reclaim NVS space
}
```

## References
- vCard storage: `components/mod_vcard/src/vcard_store.cpp`
- NVS API: ESP-IDF documentation
