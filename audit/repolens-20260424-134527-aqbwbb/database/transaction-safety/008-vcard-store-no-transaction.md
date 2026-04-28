---
title: "[MEDIUM] vCard store lacks transaction coordination between NVS and metadata cache"
severity: MEDIUM
domain: database/transaction-safety
lens: transaction-safety
labels:
  - "audit:database/transaction-safety"
---

## Summary

In `components/mod_vcard/src/vcard_store.cpp`, the `vcard_store_add()` and `vcard_store_delete()` functions update NVS storage and the in-memory metadata cache (`g_cards[]`) separately without atomic transaction guarantees. If power fails between NVS write and cache update, the cache and storage become inconsistent.

**Affected locations:**
- `vcard_store_add()` (lines 529-589): NVS write at lines 574-577, cache update at lines 583-587
- `vcard_store_delete()` (lines 596-616): NVS erase at lines 607-611, cache update at lines 613-614

## Impact

**Data inconsistency scenarios:**

1. **Add operation:**
   - NVS write succeeds (vCard stored)
   - Power fails before cache update (lines 583-587)
   - Result: `vcard_store_count()` returns stale count, new vCard not visible until next reboot

2. **Delete operation:**
   - NVS erase succeeds (line 607-611)
   - Power fails before cache update (lines 613-614)
   - Result: Cache still shows slot as used, but NVS returns empty on next read

3. **Partial metadata update:**
   - `g_cards[free_slot].used = true` (line 585)
   - `g_cards[free_slot].hash = hash` (line 586)
   - `g_card_count++` (line 587)
   - If power fails after hash but before count, `vcard_store_count()` is off by one

## Evidence

**vcard_store_add()** (lines 574-589):
```cpp
esp_err_t ret = nvs_set_str(nvs, key, tmp);
if (ret == ESP_OK) {
    ret = nvs_commit(nvs);  // NVS write
}
nvs_close(nvs);
if (ret != ESP_OK) {
    set_err(err, err_len, "NVS write failed");
    return false;  // What if cache was already updated?
}

// Cache update happens AFTER NVS commit
vcard_parse_names(tmp, g_cards[free_slot].last_name, sizeof(g_cards[free_slot].last_name),
                  g_cards[free_slot].display, sizeof(g_cards[free_slot].display));
g_cards[free_slot].used = true;  // First cache update
g_cards[free_slot].hash = hash;  // Second cache update
g_card_count++;  // Third cache update
return true;
```

**vcard_store_delete()** (lines 607-616):
```cpp
esp_err_t ret = nvs_erase_key(nvs, key);
if (ret == ESP_OK) {
    ret = nvs_commit(nvs);  // NVS erase
}
nvs_close(nvs);
if (ret != ESP_OK) return false;  // NVS succeeded, but what if power fails here?

g_cards[slot].used = false;  // First cache update
g_card_count--;  // Second cache update
return true;
```

The cache update happens AFTER the NVS operation, so a failure between them leaves inconsistent state.

## Recommended Fix

**Option 1: Update cache first, then NVS**
This ensures that if NVS fails, the cache is still correct (reflects what's actually stored):
```cpp
bool vcard_store_add(const char* vcard, size_t len, char* err, size_t err_len) {
    // ... validation, duplicate check ...

    // Update cache FIRST
    vcard_parse_names(tmp, g_cards[free_slot].last_name, sizeof(g_cards[free_slot].last_name),
                      g_cards[free_slot].display, sizeof(g_cards[free_slot].display));
    g_cards[free_slot].used = true;
    g_cards[free_slot].hash = hash;
    g_card_count++;

    // Then NVS
    nvs_handle_t nvs;
    if (nvs_open(VCARD_NAMESPACE, NVS_READWRITE, &nvs) != NVS_OK) {
        // Rollback cache
        g_cards[free_slot].used = false;
        g_card_count--;
        set_err(err, err_len, "NVS open failed");
        return false;
    }

    esp_err_t ret = nvs_set_str(nvs, key, tmp);
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs);
    }
    nvs_close(nvs);

    if (ret != ESP_OK) {
        // Rollback cache
        g_cards[free_slot].used = false;
        g_card_count--;
        set_err(err, err_len, "NVS write failed");
        return false;
    }

    return true;
}
```

**Option 2: Rebuild cache on load**
Ensure `vcard_store_init()` validates cache against NVS:
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
        
        // Check if key exists
        if (nvs_get_str(nvs, key, nullptr, &len) != ESP_OK || len == 0 || len > VCARD_MAX_LEN) {
            g_cards[slot].used = false;  // Ensure cache matches NVS
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
        } else {
            g_cards[slot].used = false;  // NVS read failed, clear cache
        }
    }
    nvs_close(nvs);
    g_cards_loaded = true;
}
```

## References

- ESP32 NVS API: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
- In-memory cache invalidation patterns
- Write-through vs write-back caching strategies