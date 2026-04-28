---
title: "[HIGH] FIDO2 sign count update has lost-update race condition in fido2_storage_increment_sign_count()"
severity: HIGH
domain: transaction-concurrency
lens: transaction-concurrency
labels:
  - audit:concurrency/transaction-concurrency
---

## Summary

In `components/mod_fido2/src/fido2_storage.cpp`, the `fido2_storage_increment_sign_count()` method (lines 913-933) performs a classic read-modify-write pattern across two separate storage systems (RAM cache and R-Memory) without atomicity. When multiple authentication operations occur concurrently (e.g., USB and BLE simultaneously), the per-credential sign count can be lost, causing the FIDO2 counter to lag behind actual usage.

**Location:** `components/mod_fido2/src/fido2_storage.cpp:913-933`

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

    return new_count;
}
```

## Impact

**Lost Updates on Sign Count:**

The FIDO2 spec requires that the sign count monotonically increases and is used by relying parties to detect credential cloning/replay attacks. A lost update means:

1. Task A (USB) calls `increment_sign_count(slot 0)` - reads `sign_count = 10`, increments to 11, writes 11
2. Task B (BLE) calls `increment_sign_count(slot 0)` - reads `sign_count = 10` (from cache), increments to 11, writes 11
3. Result: Both operations return 11, but actual count should be 12

**Security Impact:**
- **Replay attack detection weakened**: Relying parties use sign count to detect old credentials
- **Credential cloning detection fails**: If sign count doesn't increase, host may not detect cloned credential
- **FIDO2 compliance**: May not meet FIDO2 certification requirements for counter accuracy

**Additional Issues:**
- The method reads from R-Memory on each call (line 927), but the cache is updated first (line 918)
- If two tasks call this within the same millisecond, they may both read the same cached value
- The NVS global auth counter (`fido2_storage_counter_increment()`) has the same issue (lines 391-418)

## Evidence

**fido2_storage_increment_sign_count() - Non-atomic increment:**
```cpp
// Line 913-933: components/mod_fido2/src/fido2_storage.cpp
uint32_t fido2_storage_increment_sign_count(uint8_t slot) {
    if (!slot_logical_valid(slot) || !g_storage.creds[slot].valid) {
        return 0;
    }

    // Increment local cache
    g_storage.creds[slot].sign_count++;  // Line 918 - READ-MODIFY-WRITE on cache
    uint32_t new_count = g_storage.creds[slot].sign_count;

    // Read current stored data from TROPIC01
    fido2_stored_cred_t stored;
    if (read_rmem_credential(slot, &stored)) {  // Line 927 - Reads OLD value from SE
        stored.sign_count = new_count;  // Line 928 - Overwrites with stale increment
        if (!write_rmem_credential(slot, &stored)) {
            LOG_E("FIDO2", "CRITICAL: Failed to persist sign count for slot %d!", slot);
        }
    }

    return new_count;
}
```

**fido2_storage_counter_increment() - Same pattern for global counter:**
```cpp
// Line 391-418: components/mod_fido2/src/fido2_storage.cpp
bool fido2_storage_counter_increment(void) {
    if (!g_storage.counter_loaded) {
        fido2_storage_counter_load();
    }
    g_storage.auth_counter++;  // Line 400 - Non-atomic increment

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        LOG_E("FIDO2", "Failed to open NVS for counter write: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_u32(nvs, NVS_KEY_COUNTER, g_storage.auth_counter);  // Line 408
    // ...
}
```

**Global storage structure - No synchronization:**
```cpp
// Line 56-76: components/mod_fido2/src/fido2_storage.cpp
static struct {
    bool initialized;
    uint32_t auth_counter;
    bool counter_loaded;

    // Cached credential info
    struct {
        bool valid;
        uint8_t rp_id_hash[32];
        char rp_id[FIDO2_RP_ID_MAX_LEN];
        char user_name[FIDO2_USER_NAME_MAX_LEN];
        uint8_t user_id[FIDO2_USER_ID_MAX_LEN];
        uint8_t user_id_len;
        uint32_t sign_count;  // Shared mutable state
        bool resident;
        uint8_t cred_protect;
        uint8_t curve;
    } creds[FIDO2_MAX_CREDENTIALS];

    uint8_t cred_count;
} g_storage = {};
```

**Concurrent callers:**
- USB HID task (CCID interface for FIDO2)
- BLE task (FIDO2 over GATT)
- Serial command handler

## Recommended Fix

**Option 1: Add mutex protection for sign count updates**

```cpp
// Add mutex to storage state
static SemaphoreHandle_t s_storageMutex = nullptr;

// In fido2_storage_init():
if (!s_storageMutex) {
    s_storageMutex = xSemaphoreCreateMutex();
}

// In fido2_storage_increment_sign_count():
uint32_t fido2_storage_increment_sign_count(uint8_t slot) {
    if (!s_storageMutex || !slot_logical_valid(slot) || !g_storage.creds[slot].valid) {
        return 0;
    }

    xSemaphoreTake(s_storageMutex, portMAX_DELAY);

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

    xSemaphoreGive(s_storageMutex);

    return new_count;
}
```

**Option 2: Use atomic increment for sign count**

```cpp
#include "esp_atomic.h"

// Change sign_count in cache to atomic
typedef struct {
    // ...
    volatile uint32_t sign_count;  // Atomic
    // ...
} fido2_cred_cache_t;

// In fido2_storage_increment_sign_count():
uint32_t fido2_storage_increment_sign_count(uint8_t slot) {
    if (!slot_logical_valid(slot) || !g_storage.creds[slot].valid) {
        return 0;
    }

    // Atomic increment
    uint32_t oldVal, newVal;
    do {
        oldVal = g_storage.creds[slot].sign_count;
        newVal = oldVal + 1;
    } while (!atomic_compare_exchange(&g_storage.creds[slot].sign_count, &oldVal, newVal));

    // Persist to R-Memory
    fido2_stored_cred_t stored;
    if (read_rmem_credential(slot, &stored)) {
        stored.sign_count = newVal;
        write_rmem_credential(slot, &stored);
    }

    return newVal;
}
```

**Option 3: Increment directly in secure element (if supported)**

Some secure elements support counter increment as an atomic operation. If TROPIC01 supports this, use it instead of read-modify-write.

## References

1. **FIDO2 WebAuthn Spec** - [Sign Count](https://www.w3.org/TR/webauthn-1/#sign-count)
2. **FIDO2 CTAP2 Spec** - [Authenticator Data](https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#authenticator-data)
3. **ESP-IDF Atomics** - [atomic_compare_exchange](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/atomics.html)
4. **TROPIC01 Datasheet** - ECC slot counter support
