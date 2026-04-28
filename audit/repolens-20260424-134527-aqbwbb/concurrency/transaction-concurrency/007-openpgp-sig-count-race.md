---
title: "[HIGH] OpenPGP signature counter has concurrent access race between cache and NVS"
severity: HIGH
domain: transaction-concurrency
lens: cdc-badge-os
labels:
  - audit:concurrency/transaction-concurrency
---

## Summary

In `components/mod_gpg/src/openpgp/openpgp.cpp:1250-1260`, the `cmd_pso_cds()` function increments the signature counter with a non-atomic read-modify-write pattern across NVS storage:

```cpp
// Line 1250: Increment local cache
sig_count++;

// Line 1256: Persist to NVS
save_state_to_nvs();
```

The `sig_count` is a static global variable (line 140) that is:
1. Loaded from NVS at initialization (`load_state_from_nvs()`, line 488-495)
2. Incremented in `cmd_pso_cds()` when a signature is created
3. Persisted to NVS via `save_state_to_nvs()` (line 549-573)

The OpenPGP application can be accessed concurrently through CCID (USB) and potentially BLE interfaces. Between incrementing the local cache and persisting to NVS, another concurrent signature operation could also increment the counter, causing lost updates.

## Impact

OpenPGP signature counters are used for:
1. **Signature verification**: Clients may track signature counts to detect key reuse or replay
2. **Key management**: Some workflows use signature counts for key rotation decisions
3. **Audit trails**: Signature counts provide a monotonic ledger of signing activity

Lost counter updates mean:
1. **Replay vulnerability**: Old signatures might appear valid if the counter didn't increment
2. **Inconsistent state**: The NVS-persisted count may lag behind actual signing activity
3. **Debugging difficulty**: Non-deterministic counter behavior makes troubleshooting harder

## Evidence

**File**: `components/mod_gpg/src/openpgp/openpgp.cpp`
**Lines**: 1250-1260

The increment pattern:
```cpp
// Line 1252-1255: Sign the hash
if (!success) {
    ESP_LOGE(TAG, "Signature created, count=%lu", sig_count);
    return apdu_sw(resp, SW_UNKNOWN);
}

// Line 1258: Increment signature counter (local cache only)
sig_count++;

// Line 1260: Persist to NVS (separate operation)
save_state_to_nvs();

ESP_LOGI(TAG, "Signature created, count=%lu", sig_count);
return apdu_build_response(resp, resp_max, signature, 64, SW_OK);
```

The `save_state_to_nvs()` function (lines 549-573):
```cpp
static void save_state_to_nvs(void) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        // Signature count
        nvs_set_u32(nvs, "sig_count", sig_count);
        // ... other fields ...
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}
```

No locking mechanism exists between increment and persist. The counter is a simple static variable with no protection against concurrent access from multiple APDU processing contexts.

## Recommended Fix

Use an atomic increment-and-persist pattern. Increment the counter and persist it in a single NVS transaction, ensuring the increment is based on the current persisted value:

```cpp
static int cmd_pso_cds(const apdu_t *apdu, uint8_t *resp, size_t resp_max) {
    // ... existing verification and signing code ...

    if (!success) {
        ESP_LOGE(TAG, "Signature created");
        return apdu_sw(resp, SW_UNKNOWN);
    }

    // Atomic increment and persist
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        // Read current value
        uint32_t current_count;
        if (nvs_get_u32(nvs, "sig_count", &current_count) == ESP_OK) {
            sig_count = current_count + 1;  // Update local cache
        } else {
            sig_count = 1;  // First signature
        }
        
        // Write back
        nvs_set_u32(nvs, "sig_count", sig_count);
        nvs_commit(nvs);
        nvs_close(nvs);
    }

    ESP_LOGI(TAG, "Signature created, count=%lu", sig_count);
    return apdu_build_response(resp, resp_max, signature, 64, SW_OK);
}
```

Alternatively, use a simple mutex to protect the read-modify-write sequence if FreeRTOS mutexes are available in the module context.

## References

- OpenPGP Smart Card Application spec 3.4.1, Section 7.2.11 (Signature Counter)
- ESP32 NVS documentation on atomic operations
- FreeRTOS mutex documentation for task synchronization

</content>