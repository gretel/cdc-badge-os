---
title: "[MEDIUM] ESP_LOG used instead of cdc_log in CalEPD components"
severity: MEDIUM
domain: observability
lens: health-monitoring
labels:
  - "audit:observability/health-monitoring"
---

## Summary

The CalEPD display driver components use ESP-IDF's `esp_log.h` instead of the project's `cdc_log.h` library. This means logs from the display drivers won't be routed through USB CDC properly, making them invisible in the serial monitor.

**Components using esp_log.h** (`components/CalEPD/`):
- `include/epdParallel.h`
- `include/gdep015OC1.h`
- `include/gdew075HD.h`
- `include/parallel/ED060SC4.h`
- `include/parallel/ED047TC1touch.h`
- `include/parallel/ED047TC1.h`
- `include/gdew0213i5f.h`
- `include/gdew042t2Grays.h`
- `include/wave12i48.h`
- `include/gdew027w3.h`

**Example** (`components/CalEPD/include/epdParallel.h:12`):
```cpp
#include "esp_log.h"  // Wrong! Should use cdc_log.h

static const char* TAG = "EPD";
...
ESP_LOGI(TAG, "Display initialized");  // Won't appear in USB CDC!
```

**Correct approach** (used in main codebase, e.g., `main/main.cpp:13`):
```cpp
#include "cdc_log.h"

static const char* TAG = "BOOT";
...
LOG_I(TAG, "Display ready");  // Goes through USB CDC
```

## Impact

1. **Missing display diagnostics**: Errors and debug info from the E-Paper display won't appear in the serial monitor
2. **Debugging difficulty**: Display issues (initialization failures, refresh problems) are hard to diagnose
3. **Inconsistent logging**: Display logs are scattered (some via UART only, some via USB CDC)
4. **Lost error context**: Critical display errors might be missed during development or field debugging

## Evidence

**Files using esp_log.h** (found via grep):
```bash
grep -r "esp_log.h" components/CalEPD/
# Returns 10+ header files
```

**Display initialization code** (`components/cdc_hal/src/EpaperDisplay.cpp`):
- Uses `cdc_log.h` correctly
- But the underlying CalEPD library uses `esp_log.h`
- Logs from CalEPD will appear on UART but not USB CDC

**Correct logging in main code** (`main/main.cpp:13`):
```cpp
#include "cdc_log.h"
LOG_I(TAG, "Display ready (%ux%u)", display->getWidth(), display->getHeight());
```

**CalEPD logging** (`components/CalEPD/include/epdParallel.h:12`):
```cpp
#include "esp_log.h"
// ESP_LOG macros used throughout CalEPD
```

## Recommended Fix

Replace `esp_log.h` with `cdc_log.h` in all CalEPD headers:

**Step 1: Update header files** (e.g., `components/CalEPD/include/epdParallel.h`):
```cpp
// Before
#include "esp_log.h"

// After
#include "cdc_log.h"
```

**Step 2: Replace ESP_LOG macros with cdc_log macros** (e.g., `components/CalEPD/src/epdParallel.cpp`):
```cpp
// Before
ESP_LOGI(TAG, "Display initialized");
ESP_LOGE(TAG, "Init failed");
ESP_LOGD(TAG, "Debug info");

// After
LOG_I(TAG, "Display initialized");
LOG_E(TAG, "Init failed");
LOG_D(TAG, "Debug info");
```

**Step 3: Update CMakeLists.txt** to include cdc_log dependency:
```cmake
# components/CalEPD/CMakeLists.txt
target_link_libraries(cal_epd
    PRIVATE
    cdc_log  # Add this
    # ... other deps
)
```

**Step 4: Verify logging works**:
```bash
# Build and flash
pio run -t upload

# Check serial output
pio device monitor
# Display logs should now appear
```

**Expected behavior after fix**:
```
[BOOT] Display ready (800x480)
[I][EPD] Display initialized
[D][EPD] Refresh complete
```

## References

- CDC Log documentation (project convention): `components/cdc_log/include/cdc_log.h`
- ESP32 logging: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/log.html
- Display driver: CalEPD (Calibrated E-Paper Display library)
