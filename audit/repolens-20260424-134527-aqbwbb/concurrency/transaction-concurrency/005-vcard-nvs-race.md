---
title: "[LOW] vCard store NVS read-modify-write lacks atomicity for duplicate check"
severity: LOW
domain: transaction-concurrency
lens: cdc-badge-os
labels:
  - audit:concurrency/transaction-concurrency
---

## Summary

In `components/mod_vcard/src/vcard_store.cpp:529-588`, the `vcard_store_add()` function checks for duplicates and then writes:

```cpp
bool vcard_store_add(const char* vcard, size_t len, char* err, size_t err_len) {
    // ...
    vcard_store_init();
    
    nvs_handle_t nvs;
    if (nvs_open(VCARD_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        // ...
    }

    uint32_t hash = fnv1a_hash(vcard, len);
    if (vcard_is_duplicate(nvs, vcard, len, hash)) {
        nvs_close(nvs);
        set_err(err, err_len, "Duplicate vCard");
        return false;
    }

    // Find free slot
    int free_slot = -1;
    for (uint16_t i = 0; i < VCARD_MAX_CARDS; i++) {
        if (!g_cards[i].used) {
            free_slot = i;
            break;
        }
    }

    // ... write to NVS ...
}
```

The duplicate check reads from NVS, then the write happens later. If two threads call `vcard_store_add()` concurrently with the same vCard, both may pass the duplicate check and both write.

## Impact

1. **Duplicate entries**: The same vCard may be stored twice in different slots
2. **Slot exhaustion**: More duplicates than necessary consume limited slots
3. **Minor data redundancy**: Not a security issue, just inefficient storage use

## Evidence

**File**: `components/mod_vcard/src/vcard_store.cpp`
**Lines**: 529-588

The `vcard_is_duplicate` function at lines 498-518 reads from NVS but the result is not used atomically with the write.

## Recommended Fix

Since vCard storage is typically single-threaded (user adds vCards via UI), this is a low-priority issue. If needed, add a simple mutex:

```cpp
static SemaphoreHandle_t s_vcard_mutex = nullptr;

bool vcard_store_add(const char* vcard, size_t len, char* err, size_t err_len) {
    if (!s_vcard_mutex) {
        s_vcard_mutex = xSemaphoreCreateMutex();
    }
    xSemaphoreTake(s_vcard_mutex, portMAX_DELAY);
    
    // ... existing logic ...
    
    xSemaphoreGive(s_vcard_mutex);
    return true;
}
```

## References

- NVS flash wear characteristics
- vCard storage limits in the badge
