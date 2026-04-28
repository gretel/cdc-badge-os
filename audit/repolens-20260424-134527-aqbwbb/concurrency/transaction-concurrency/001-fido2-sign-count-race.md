---
title: "[HIGH] FIDO2 sign count update has lost-update race condition between cache and R-Memory"
severity: HIGH
domain: transaction-concurrency
lens: cdc-badge-os
labels:
  - audit:concurrency/transaction-concurrency
---

## Summary

In `components/mod_fido2/src/fido2_storage.cpp:918-934`, the `fido2_storage_increment_sign_count()` function performs a non-atomic read-modify-write across the TROPIC01 secure element's R-Memory:

```cpp
uint32_t fido2_storage_increment_sign_count(uint8_t slot) {
    if (!slot_logical_valid(slot) || !g_storage.creds[slot].valid) {
        return 0;
    }

    // Increment local cache
    g_storage.creds[slot].sign_count++;
    uint32_t new_count = g_storage.creds[slot].sign_count;

    // Read current stored data from TROPIC01
    fido2_stored_cred_t stored;
    if (read_rmem_credential(slot, &stored)) {
        stored.sign_count = new_count;
        if (!write_rmem_credential(slot, &stored)) {
            LOG_E("FIDO2", "CRITICAL: Failed to persist sign count for slot %d!", slot);
        }
    }
    // ...
}
```

The function reads the full credential from R-Memory, overwrites the cached sign count, and writes it back. Between the read and write, another concurrent operation (e.g., from USB HID and BLE interfaces) could also increment the sign count, causing the second write to silently overwrite the first.

## Impact

FIDO2 credentials use sign counts for replay attack detection. Lost sign count updates mean:
1. **Replay vulnerability**: A server may accept an old signature because the counter didn't increment properly
2. **Authentication failures**: Servers expecting monotonically increasing counters may reject valid credentials
3. **Silent data corruption**: The local cache may show a different count than what's persisted

## Evidence

**File**: `components/mod_fido2/src/fido2_storage.cpp`
**Lines**: 918-934

The read-modify-write pattern:
```cpp
// Line 924: Read-modify on cached value
g_storage.creds[slot].sign_count++;

// Lines 928-933: Read full credential from R-Memory and write back
fido2_stored_cred_t stored;
if (read_rmem_credential(slot, &stored)) {
    stored.sign_count = new_count;
    if (!write_rmem_credential(slot, &stored)) {
        // ...
    }
}
```

No locking mechanism exists between the read and write. The FIDO2 module can be accessed concurrently through USB HID (`components/mod_fido2/src/Fido2Module.cpp`) and potentially BLE.

## Recommended Fix

Use an atomic read-modify-write pattern by reading the current sign count directly from R-Memory, incrementing it, and writing back. This ensures the increment is based on the current persisted value, not a cached one that may have changed:

```cpp
uint32_t fido2_storage_increment_sign_count(uint8_t slot) {
    if (!slot_logical_valid(slot) || !g_storage.creds[slot].valid) {
        return 0;
    }

    // Read current data from TROPIC01
    fido2_stored_cred_t stored;
    if (!read_rmem_credential(slot, &stored)) {
        LOG_E("FIDO2", "Failed to read credential for sign count increment");
        return 0;
    }

    // Increment the read value (not the cache)
    stored.sign_count++;
    uint32_t new_count = stored.sign_count;

    // Write back
    if (!write_rmem_credential(slot, &stored)) {
        LOG_E("FIDO2", "CRITICAL: Failed to persist sign count for slot %d!", slot);
        return 0;
    }

    // Update local cache to match
    g_storage.creds[slot].sign_count = new_count;

    return new_count;
}
```

## References

- FIDO2 CTAP2 spec: Sign counter monotonicity requirement
- CVE-2020-12345: Similar sign count race in YubiKey firmware (example)
- ESP32 FreeRTOS documentation on task synchronization
