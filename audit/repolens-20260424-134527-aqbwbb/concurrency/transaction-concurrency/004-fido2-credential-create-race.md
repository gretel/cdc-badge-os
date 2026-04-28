---
title: "[MEDIUM] FIDO2 credential creation has write-skew on slot allocation"
severity: MEDIUM
domain: transaction-concurrency
lens: cdc-badge-os
labels:
  - audit:concurrency/transaction-concurrency
---

## Summary

In `components/mod_fido2/src/fido2_storage.cpp:752-888`, the `fido2_storage_create_credential()` function finds a free slot and then creates a credential:

```cpp
int8_t fido2_storage_create_credential(...) {
    // FIDO2 spec: If credential with same RP ID + User ID exists, replace it
    int8_t existing_slot = fido2_storage_find_by_rp_user(rp_id_hash, user_id, user_id_len);
    int8_t slot;

    if (existing_slot >= 0) {
        slot = existing_slot;
        erase_slot_data(static_cast<uint8_t>(slot));
        g_storage.creds[slot].valid = false;
        g_storage.cred_count--;
    } else {
        slot = fido2_storage_find_free_slot();
        if (slot < 0) {
            LOG_E("FIDO2", "No free slots");
            return false;
        }
    }
    // ...
}
```

Two concurrent credential creation requests could:
1. Both read the same cache state (e.g., slot 5 is free)
2. Both decide to use slot 5
3. Both generate keys and write credentials
4. The second write overwrites the first, losing one credential

## Impact

1. **Lost credentials**: One credential silently overwrites another
2. **Cache-chip mismatch**: The cache may show different slots as used than what's in TROPIC01
3. **User confusion**: A registered credential may not work because it was overwritten

## Evidence

**File**: `components/mod_fido2/src/fido2_storage.cpp`
**Lines**: 752-888

**File**: `components/mod_fido2/src/fido2_storage.cpp`
**Lines**: 473-498 (find_free_slot)

The `find_free_slot` function only checks the cached `g_storage.creds` array, not the actual TROPIC01 state:
```cpp
int8_t fido2_storage_find_free_slot(void) {
    uint16_t count = ecc_count();
    for (uint8_t i = 0; i < count && i < FIDO2_MAX_CREDENTIALS; i++) {
        if (!g_storage.creds[i].valid) {
            return i;
        }
    }
    return -1;
}
```

## Recommended Fix

Add a simple mutex to protect the slot allocation and credential creation as a single atomic operation:

```cpp
// Add at file scope
static SemaphoreHandle_t s_cred_mutex = nullptr;

static void ensure_cred_mutex() {
    if (!s_cred_mutex) {
        s_cred_mutex = xSemaphoreCreateMutex();
    }
}

bool fido2_storage_create_credential(...) {
    ensure_cred_mutex();
    xSemaphoreTake(s_cred_mutex, portMAX_DELAY);
    
    // ... existing logic ...
    
    xSemaphoreGive(s_cred_mutex);
    return true;
}
```

## References

- FIDO2 CTAP2 credential management spec
- Write skew anomaly in database transactions
