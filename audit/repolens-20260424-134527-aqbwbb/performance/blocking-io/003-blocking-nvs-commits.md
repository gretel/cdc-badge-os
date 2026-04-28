---
title: "[MEDIUM] Blocking NVS commit operations in storage modules"
severity: MEDIUM
domain: performance/blocking-io
lens: blocking-io
labels:
  - "audit:performance/blocking-io"
---

## Summary
Multiple storage modules use synchronous `nvs_commit()` operations which block until flash write completes. This occurs in:

1. `components/cdc_core/src/AttestationKeyService.cpp` - Badge hash storage
2. `components/cdc_core/src/ModuleRegistry.cpp` - Module configuration
3. `components/cdc_core/src/TropicStorage.cpp` - TROPIC01 slot metadata
4. `components/cdc_hal/src/EpaperDisplay.cpp` - Backlight persistence (line 96)

**Evidence:**
```cpp
// AttestationKeyService.cpp
err = nvs_commit(nvs);  // Flash write blocks here

// ModuleRegistry.cpp
nvs_commit(modHandle);  // Flash write blocks here

// EpaperDisplay.cpp:96
nvs_commit(nvs);  // Flash write blocks here
```

## Impact
- **Flash latency**: NVS commit can take 5-20ms depending on flash state and fragmentation
- **Blocking writes**: All NVS commits are synchronous, blocking the calling task
- **Frequent small writes**: Backlight persistence on every change (line 96) causes unnecessary flash wear and blocking
- **No batching**: Module configuration changes commit individually instead of batching

## Recommended Fix
1. Defer NVS commits to a dedicated storage task that batches writes
2. Use `nvs_set_blob()` with `nvs_commit()` less frequently (e.g., batch module changes)
3. For backlight, debounce the save operation (save only after 1-2 seconds of inactivity)
4. Consider using NVS "lazy commit" pattern where possible

## References
- ESP-IDF NVS: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/storage/nvs.html
