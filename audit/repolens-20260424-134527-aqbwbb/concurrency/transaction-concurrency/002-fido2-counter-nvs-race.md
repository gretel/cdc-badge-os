---
title: "[MEDIUM] FIDO2 global auth counter NVS update has lost-update race condition"
severity: MEDIUM
domain: transaction-concurrency
lens: cdc-badge-os
labels:
  - audit:concurrency/transaction-concurrency
---

## Summary

In `components/mod_fido2/src/fido2_storage.cpp:388-418`, the `fido2_storage_counter_increment()` function performs a read-modify-write on NVS:

```cpp
bool fido2_storage_counter_increment(void) {
    if (!g_storage.counter_loaded) {
        fido2_storage_counter_load();
    }
    g_storage.auth_counter++;

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        LOG_E("FIDO2", "Failed to open NVS for counter write: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_u32(nvs, NVS_KEY_COUNTER, g_storage.auth_counter);
    // ...
}
```

The function increments a cached counter and writes it to NVS. If two threads call this function concurrently (e.g., FIDO2 authentication via USB and BLE), both may read the same cached value, increment it, and write back, causing one increment to be lost.

## Impact

The global authentication counter is used for FIDO2 attestation. Lost updates can cause:
1. **Counter inconsistency**: The persisted counter may not reflect the actual number of authentications
2. **Server-side verification issues**: Some servers verify counter monotonicity across authentications
3. **Debugging difficulty**: Non-reproducible counter values make troubleshooting harder

## Evidence

**File**: `components/mod_fido2/src/fido2_storage.cpp`
**Lines**: 388-418

The static storage structure at lines 57-77:
```cpp
static struct {
    bool initialized;
    uint32_t auth_counter;
    bool counter_loaded;
    // ...
} g_storage = {};
```

This global variable is accessed without synchronization from multiple potential entry points.

## Recommended Fix

Add a simple mutex to protect the counter update:

```cpp
// Add to file: static SemaphoreHandle_t s_counter_mutex = nullptr;

// In fido2_storage_counter_increment():
static void ensure_counter_mutex() {
    if (!s_counter_mutex) {
        s_counter_mutex = xSemaphoreCreateMutex();
    }
}

bool fido2_storage_counter_increment(void) {
    ensure_counter_mutex();
    
    if (!g_storage.counter_loaded) {
        fido2_storage_counter_load();
    }
    
    xSemaphoreTake(s_counter_mutex, portMAX_DELAY);
    g_storage.auth_counter++;
    uint32_t new_value = g_storage.auth_counter;
    xSemaphoreGive(s_counter_mutex);

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        return false;
    }

    err = nvs_set_u32(nvs, NVS_KEY_COUNTER, new_value);
    // ...
}
```

## References

- ESP-IDF NVS documentation on concurrent access
- FreeRTOS mutex documentation
