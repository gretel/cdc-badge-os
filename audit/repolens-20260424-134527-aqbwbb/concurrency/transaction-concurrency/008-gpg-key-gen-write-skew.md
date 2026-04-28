---
title: "[MEDIUM] GPG metadata update has potential write-skew between keys and NVS"
severity: MEDIUM
domain: transaction-concurrency
lens: cdc-badge-os
labels:
  - audit:concurrency/transaction-concurrency
---

## Summary

In `components/mod_gpg/src/gpg.cpp:354-423`, the `gpg_generate_key()` function performs a multi-step transaction that generates three ECC keys (SIG, DEC, AUT) and persists metadata to NVS. The function reads existing user ID from metadata, generates new keys, and then overwrites the metadata:

```cpp
// Line 394-395: Read existing user ID
char existing_user_id[GPG_USER_ID_MAX] = {};
strncpy(existing_user_id, s_metadata.user_id, sizeof(existing_user_id) - 1);

// Lines 362-364: Generate three keys sequentially
if (!se_generate_key(sig_slot, curve)) return false;
if (!se_generate_key(aut_slot, curve)) return false;
if (!se_generate_key(dec_slot, CDC_CURVE_P256)) return false;

// Lines 396-410: Overwrite metadata
memset(&s_metadata, 0, sizeof(s_metadata));
s_metadata.magic = GPG_METADATA_MAGIC;
// ... set new values ...

// Line 412: Persist to NVS
if (!save_metadata()) {
    return false;
}
```

The issue is that if the function is interrupted or called concurrently (e.g., from USB and BLE interfaces), the state between key generation and metadata persistence can become inconsistent. Specifically:
1. Keys are generated in the secure element
2. Metadata is updated in memory
3. Metadata is persisted to NVS

If a second concurrent call to `gpg_generate_key()` happens between steps 2 and 3, both operations may complete successfully but with different key/metadata pairings.

## Impact

This write-skew pattern can lead to:
1. **Key/metadata mismatch**: The NVS metadata may describe keys that don't exist in the secure element
2. **User ID confusion**: If user ID is read from existing metadata and used for new key generation, concurrent calls may produce keys with incorrect user ID associations
3. **Recovery complexity**: If the metadata shows keys exist but the secure element has different keys, the user may need to manually reset and regenerate

## Evidence

**File**: `components/mod_gpg/src/gpg.cpp`
**Lines**: 354-423

The transaction sequence:
```cpp
bool gpg_generate_key(uint8_t curve) {
    // ... validation ...

    // Step 1: Generate keys (3 separate writes to secure element)
    if (!se_generate_key(sig_slot, curve)) return false;    // Line 362
    if (!se_generate_key(aut_slot, curve)) return false;    // Line 363
    if (!se_generate_key(dec_slot, CDC_CURVE_P256)) return false; // Line 364

    // Step 2: Read existing user ID (from potentially stale cache)
    char existing_user_id[GPG_USER_ID_MAX] = {};
    strncpy(existing_user_id, s_metadata.user_id, sizeof(existing_user_id) - 1); // Line 395

    // Step 3: Compute fingerprints based on newly generated keys
    // ... fingerprint calculations ...

    // Step 4: Overwrite metadata in memory
    memset(&s_metadata, 0, sizeof(s_metadata)); // Line 396
    // ... set new metadata ...

    // Step 5: Persist to NVS
    if (!save_metadata()) { // Line 412
        return false;
    }
    // ... update other state ...
}
```

The `s_metadata` is a static global variable (line 18) that is:
- Loaded from NVS at initialization (`load_metadata()`)
- Read in `gpg_generate_key()` to preserve user ID
- Overwritten completely during key generation
- Persisted via `save_metadata()`

There is no locking or atomicity guarantee between the key generation and metadata persistence.

## Recommended Fix

Add a simple mutex or flag to prevent concurrent key generation. The fix should:
1. Prevent `gpg_generate_key()` from running concurrently with itself
2. Optionally persist metadata atomically with the last key generation

Example using a simple flag:
```cpp
static bool s_key_generation_in_progress = false;

bool gpg_generate_key(uint8_t curve) {
    if (!gpg_storage_ready()) return false;
    if (!gpg_has_pending_user_id() && !s_initialized) return false;
    
    // Prevent concurrent key generation
    if (__atomic_exchange_n(&s_key_generation_in_progress, true, __ATOMIC_SEQ_CST)) {
        LOG_W(TAG, "Key generation already in progress");
        return false;
    }
    
    // ... existing key generation code ...
    
    // At the end, before returning:
    __atomic_store_n(&s_key_generation_in_progress, false, __ATOMIC_SEQ_CST);
    return true;
}
```

Alternatively, use FreeRTOS mutex if available:
```cpp
static SemaphoreHandle_t s_key_gen_mutex = NULL;

// In init():
s_key_gen_mutex = xSemaphoreCreateMutex();

// In gpg_generate_key():
if (xSemaphoreTake(s_key_gen_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
    LOG_W(TAG, "Failed to acquire key gen mutex");
    return false;
}
// ... key generation code ...
xSemaphoreGive(s_key_gen_mutex);
```

## References

- OpenPGP Smart Card Application spec 3.4.1, Section 6.1 (Key Generation)
- ESP32 FreeRTOS mutex documentation
- Write skew anomaly: https://www.cockroachlabs.com/docs/stable/Serializable-Snapshot-Isolation-and-Write-Skew.html

</content>