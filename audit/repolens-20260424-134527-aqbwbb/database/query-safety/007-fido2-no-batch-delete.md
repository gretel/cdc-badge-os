---
title: "[LOW] FIDO2 credential deletion lacks batch-operation safety"
severity: LOW
domain: database
lens: query-safety
labels:
  - "audit:database/query-safety"
---

## Summary
The FIDO2 storage `delete_credential` function at `components/mod_fido2/src/fido2_storage.cpp:891-908` deletes credentials one at a time with no batch-operation support or bulk-delete safety checks. If a developer wants to clear all credentials, they must call the function in a loop, which could lead to partial deletion if an error occurs mid-batch.

## Impact
- **Partial Deletion Risk**: Bulk delete operations could leave credentials in an inconsistent state
- **No Atomicity**: No way to delete multiple credentials atomically
- **No Pre-flight Check**: Can't verify how many credentials will be affected before deletion

## Evidence
File: `components/mod_fido2/src/fido2_storage.cpp`

Lines 891-908 (fido2_storage_delete_credential):
```cpp
bool fido2_storage_delete_credential(uint8_t slot) {
    if (!slot_logical_valid(slot) || !g_storage.creds[slot].valid) {
        return false;
    }

    LOG_I("FIDO2", "Deleting credential in slot %d", slot);

    // Erase ECC key and R-Memory
    erase_slot_data(slot);

    // Update local cache
    g_storage.creds[slot].valid = false;
    g_storage.cred_count--;

    LOG_I("FIDO2", "Deleted credential in slot %d", slot);
    return true;
}
```

Helper function `erase_slot_data` at lines 238-249:
```cpp
static void erase_slot_data(uint8_t logical_slot) {
    auto* se = get_se();
    if (!se) return;

    uint8_t phys_slot = ecc_slot_for_logical(logical_slot);
    se->eccDelete(phys_slot);  // Deletes ECC key

    uint16_t rmem_slot = rmem_slot_for_logical(logical_slot);
    se->rmemErase(rmem_slot);  // Deletes R-Memory
}
```

Note: Function returns `bool` but `erase_slot_data` is `void` and doesn't report errors!

## Recommended Fix
1. **Add batch delete function**:
```cpp
/**
 * \brief Deletes multiple credentials atomically.
 * \param slots Array of logical slot indices.
 * \param count Number of slots to delete.
 * \return `true` if all deletions succeeded.
 */
bool fido2_storage_delete_credentials(const uint8_t* slots, uint8_t count) {
    if (!slots || count == 0) return false;
    
    auto* se = get_se();
    if (!se) return false;
    
    // Pre-flight check: verify all slots are valid
    for (uint8_t i = 0; i < count; i++) {
        if (!slot_logical_valid(slots[i]) || !g_storage.creds[slots[i]].valid) {
            LOG_E("FIDO2", "Slot %d not valid for deletion", slots[i]);
            return false;
        }
    }
    
    // Atomic deletion
    for (uint8_t i = 0; i < count; i++) {
        uint8_t slot = slots[i];
        erase_slot_data(slot);
        g_storage.creds[slot].valid = false;
    }
    g_storage.cred_count -= count;
    
    return true;
}
```

2. **Add return value to erase_slot_data**:
```cpp
static bool erase_slot_data(uint8_t logical_slot) {
    auto* se = get_se();
    if (!se) return false;

    uint8_t phys_slot = ecc_slot_for_logical(logical_slot);
    if (se->eccDelete(phys_slot) != cdc::hal::SeResult::OK) {
        return false;
    }

    uint16_t rmem_slot = rmem_slot_for_logical(logical_slot);
    return se->rmemErase(rmem_slot) == cdc::hal::SeResult::OK;
}
```

3. **Add delete-all function** with confirmation:
```cpp
bool fido2_storage_delete_all(void) {
    uint8_t slots[FIDO2_MAX_CREDENTIALS];
    uint8_t count = 0;
    
    for (uint8_t i = 0; i < ecc_count() && count < FIDO2_MAX_CREDENTIALS; i++) {
        if (g_storage.creds[i].valid) {
            slots[count++] = i;
        }
    }
    
    return fido2_storage_delete_credentials(slots, count);
}
```

## References
- FIDO2 storage: `components/mod_fido2/src/fido2_storage.cpp`
- Secure element HAL: `components/cdc_hal/ISecureElement.h`
