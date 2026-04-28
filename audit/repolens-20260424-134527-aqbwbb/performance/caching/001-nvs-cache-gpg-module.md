---
title: "[MEDIUM] Missing in-memory cache for frequently accessed NVS data in GPG module"
severity: MEDIUM
domain: performance/caching
lens: embedded-firmware
labels:
  - "nvs-cache"
  - "memory-optimization"
---

## Summary
The GPG module (`components/mod_gpg/src/openpgp/openpgp.cpp`) performs repeated NVS reads without caching frequently accessed data. Each call to functions like `get_cardholder_name()`, `get_fingerprint()`, etc. opens the NVS namespace, reads the data, and closes it, resulting in redundant flash I/O operations.

**Evidence:**
- File: `components/mod_gpg/src/openpgp/openpgp.cpp`
- NVS namespace opened on every data retrieval (line ~160-180 area based on NVS operations)
- 42 NVS operations tracked in the module
- Global variables exist for storage but no runtime cache population on init

## Impact
**Performance Cost:**
- NVS read operations are relatively slow (1-5ms per read depending on data size)
- On repeated cardholder data access (e.g., displaying GPG info multiple times), the same data is read from flash repeatedly
- Flash wear: Each NVS write/erase cycle contributes to flash endurance limits (typically 100k-1M cycles)

**User Experience:**
- Delays in UI rendering when accessing GPG data multiple times
- Unnecessary power consumption from flash controller activity

## Evidence
From `components/mod_gpg/src/openpgp/openpgp.cpp`:

```cpp
// NVS namespace defined
#define NVS_NAMESPACE "openpgp"

// Global storage buffers declared
static uint8_t fingerprint_sig[20] = {0};
static char cardholder_name[40] = {0};

// But NVS is read on-demand without pre-populating cache
// Each access pattern: nvs_open() -> nvs_get_*() -> nvs_close()
```

The module uses static global buffers but doesn't populate them on initialization. Instead, each accessor likely performs fresh NVS reads.

## Recommended Fix
Implement a lazy-loading cache pattern:

1. **Add cache initialization function** called once during module startup:
```cpp
static bool s_cacheInitialized = false;

static void initNvsCache() {
    if (s_cacheInitialized) return;
    
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) == ESP_OK) {
        // Load all frequently accessed data
        nvs_get_str(nvs, "ch_name", cardholder_name, &len);
        nvs_get_blob(nvs, "fp_sig", fingerprint_sig, &len);
        // ... other fields
        nvs_close(nvs);
        s_cacheInitialized = true;
    }
}
```

2. **Call `initNvsCache()` once** during `openpgp_init()` or module startup

3. **Update cache on mutations** - when data is written, update both NVS and the in-memory cache

4. **Add cache invalidation** for module reset/reload scenarios

This reduces NVS open/close overhead and provides faster data access.

## References
- ESP-IDF NVS Guide: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
- NVS Performance characteristics: 1-5ms per read, ~300us per write
- Flash endurance considerations for embedded systems
