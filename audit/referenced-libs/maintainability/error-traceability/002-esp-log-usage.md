---
title: "[MEDIUM] ESP_LOG used instead of cdc_log in CalEPD components"
severity: MEDIUM
domain: Error Traceability
lens: error-traceability
labels:
  - "audit:maintainability/error-traceability"
---

## Summary
The CalEPD display driver components use ESP_LOG macros directly instead of the project's cdc_log library. This bypasses the centralized logging system and may cause errors to be missed in serial output.

## Impact
- **Inconsistent logging**: Errors may not appear on USB CDC serial monitor (the primary debug interface)
- **Logging fragmentation**: Different components use different logging systems
- **Error log missing**: ESP_LOG messages are not captured in the PSRAM-backed error log ring buffer
- **Harder debugging**: Serial output may be incomplete or inconsistent

## Evidence

### Files using ESP_LOG instead of cdc_log:

`components/CalEPD/include/color/wave5i7Color.h`:
```cpp
ESP_LOGE("Epd::printerf", "max_buffer out of range. Increase max_buffer!");
```

`components/CalEPD/models/wave12i48.cpp`:
```cpp
ESP_LOGI(TAG, "_waitBusyM1 for %s", message);
if (debug_enabled) ESP_LOGI(TAG, "Busy Timeout");
ESP_LOGI(TAG, "_waitBusyM2 for %s", message);
// ... multiple similar occurrences
```

`components/CalEPD/models/gdem029E97.cpp`:
```cpp
ESP_LOGI("PARTIAL", "partial %d x:%d y:%d\n", partials, (int)x, (int)y);
ESP_LOGI(TAG, "_waitBusy for %s", message);
if (debug_enabled) ESP_LOGI(TAG, "Busy Timeout");
ESP_LOGE("Gdem029E97", "This epaper has no 4 gray mode");
```

`components/CalEPD/models/gdew075HD.cpp`:
```cpp
ESP_LOGI(TAG, "_waitBusy for %s", message);
if (debug_enabled) ESP_LOGI(TAG, "Busy Timeout");
```

`components/CalEPD/models/gdeh0154d67.cpp`:
```cpp
ESP_LOGI(TAG, "_waitBusy for %s", message);
if (debug_enabled) ESP_LOGI(TAG, "Busy Timeout");
```

### Total occurrences: ~20+ ESP_LOG calls across CalEPD components

## Recommended Fix

1. **Replace ESP_LOG with cdc_log macros** in all CalEPD files:

   ```cpp
   // Replace:
   #include "esp_log.h"
   ESP_LOGE("TAG", "Error message");
   
   // With:
   #include "cdc_log.h"
   LOG_E("TAG", "Error message");
   ```

2. **Define TAG consistently** in each file:
   ```cpp
   static const char* TAG = "EPD_WAVE12I48";
   ```

3. **Update CMakeLists.txt** for CalEPD components to include cdc_log:
   ```cmake
   target_link_libraries(CalEPD PRIVATE cdc_log)
   ```

4. **Prioritize critical files**:
   - `wave12i48.cpp` - Most occurrences, includes timeout errors
   - `gdem029E97.cpp` - Includes error logging
   - `gdew075HD.cpp` - Busy wait errors
   - `gdeh0154d67.cpp` - Display initialization errors

## References
- Project documentation: `CLAUDE.md` - "ALWAYS use the cdc_log library, NEVER use ESP_LOG directly!"
- CDC Log library: `components/cdc_log/include/cdc_log.h`
