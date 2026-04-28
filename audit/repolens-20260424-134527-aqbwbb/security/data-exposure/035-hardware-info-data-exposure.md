---
title: "[LOW] HardwareInfo exposes detailed system metrics including heap, PSRAM, and NVS statistics"
severity: LOW
domain: cdc_os_ui
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The `HardwareInfo` UI component exposes detailed system metrics including:
- Free and used heap memory
- PSRAM statistics
- NVS used/total entries
- Battery percentage
- Temperature
- Uptime

This information is displayed via the `showHardwareInfo()` function and can be useful for side-channel attacks and system fingerprinting.

**Location:** `components/cdc_os_ui/src/HardwareInfo.cpp`

## Impact
- **Side-channel analysis**: Memory usage patterns can reveal which modules are active
- **System fingerprinting**: Exact heap/PSRAM sizes reveal hardware configuration
- **NVS occupancy**: Used entries count can indicate how much data is stored
- **Battery tracking**: Battery percentage reveals usage patterns
- **Uptime correlation**: Precise uptime helps correlate activities with specific boot cycles

## Evidence
File: `components/cdc_os_ui/src/HardwareInfo.cpp:97-143`
```cpp
size_t freeHeap = esp_get_free_heap_size();
size_t totalHeap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
size_t usedHeap = (totalHeap > freeHeap) ? (totalHeap - freeHeap) : 0;
append("%s: %lu/%lu KB\n",
       tr(StringId::HW_HEAP),
       (unsigned long)(usedHeap / 1024),
       (unsigned long)(totalHeap / 1024));

size_t psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
size_t psramTotal = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
size_t psramUsed = (psramTotal > psramFree) ? (totalHeap - freeHeap) : 0;
append("%s: %lu/%lu KB\n",
       tr(StringId::HW_PSRAM),
       (unsigned long)(psramUsed / 1024),
       (unsigned long)(totalHeap / 1024));

nvs_stats_t nvsStats;
if (nvs_get_stats(nullptr, &nvsStats) == ESP_OK) {
    append("%s: %lu/%lu %s\n",
           tr(StringId::HW_NVS),
           (unsigned long)nvsStats.used_entries,
           (unsigned long)nvsStats.total_entries,
           tr(StringId::HW_ENTRIES));
}
```

## Recommended Fix
1. Consider adding a build flag to compile out detailed hardware info
2. Round values to reduce precision (e.g., show heap in 100KB increments)
3. Hide NVS statistics or show only as "used" without exact counts
4. Document that this screen should not be accessible in production

Example fix:
```cpp
// Round heap to nearest 100KB
size_t usedHeapKB = (usedHeap / 100) * 100;
append("%s: ~%lu KB used\n", tr(StringId::HW_HEAP), (unsigned long)(usedHeapKB / 1024));
```

## References
- Side-channel memory attacks: https://en.wikipedia.org/wiki/Side-channel_attack
- ESP32 memory architecture: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/memory/
- Related to issue #23 (heap info via STATUS command)
- Related to issue #25 (uptime exposure)
