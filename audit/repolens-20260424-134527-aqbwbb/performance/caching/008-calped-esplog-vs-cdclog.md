---
title: "[LOW] CalEPD uses ESP_LOG directly instead of cdc_log, missing unified log buffering"
severity: LOW
domain: performance/caching
lens: embedded-firmware
labels:
  - "logging-cache"
  - "cdc_log"
---

## Summary
The CalEPD display library (`components/CalEPD/`) uses `ESP_LOG` macros directly instead of the project's `cdc_log` library. This bypasses the unified logging layer which provides buffering and routing through USB CDC for proper serial logging.

**Evidence:**
- File: Multiple files in `components/CalEPD/`
- Count: 167 ESP_LOG calls in CalEPD component
- Example: `components/CalEPD/models/wave12i48.cpp`, `components/CalEPD/models/gdey029T94.cpp`
- Project standard: `cdc_log.h` with `LOG_I()`, `LOG_E()`, `LOG_W()`, `LOG_D()` macros

## Impact
**Performance Cost:**
- ESP_LOG writes directly to UART without buffering optimization
- cdc_log provides USB CDC routing with batching for efficient serial output
- Each ESP_LOG call is a separate UART write operation
- During heavy logging (e.g., display updates with debug enabled), this can cause UART flooding

**Logging Consistency:**
- ESP_LOG output may not appear in USB CDC serial monitor
- Mixed logging systems make debugging harder
- Log levels not unified across the codebase

## Evidence
From `components/CalEPD/models/wave12i48.cpp` and other CalEPD files:

```cpp
// Using ESP_LOG directly (bypasses cdc_log)
ESP_LOGI(TAG, "_waitBusyM1 for %s", message);
ESP_LOGI(TAG, "Busy Timeout");
ESP_LOGE("Epd::printerf", "max_buffer out of range. Increase max_buffer!");
```

From `components/cdc_hal/src/EpaperDisplay.cpp` (correct usage):

```cpp
// Using cdc_log (project standard)
#include "cdc_log.h"
LOG_I(TAG, "Initializing E-Paper display...");
LOG_E(TAG, "Failed to init display");
```

CalEPD has 167 ESP_LOG calls vs cdc_log's buffered USB CDC routing.

## Recommended Fix
Replace ESP_LOG with cdc_log in CalEPD:

1. **Add cdc_log dependency to CalEPD CMakeLists.txt**:
```cmake
# components/CalEPD/CMakeLists.txt
idf_component_register(
    SRCS "src/epd.cpp" "src/epdspi.cpp"
    INCLUDE_DIRS "include"
    REQUIRES cdc_log  # Add this
)
```

2. **Replace ESP_LOG includes with cdc_log**:
```cpp
// Change from:
#include "esp_log.h"

// To:
#include "cdc_log.h"
```

3. **Replace ESP_LOG macros**:
```cpp
// Change from:
ESP_LOGI(TAG, "message");
ESP_LOGE(TAG, "error");
ESP_LOGW(TAG, "warning");
ESP_LOGD(TAG, "debug");

// To:
LOG_I(TAG, "message");
LOG_E(TAG, "error");
LOG_W(TAG, "warning");
LOG_D(TAG, "debug");
```

4. **Consider conditional compilation for debug logs**:
```cpp
#ifdef CONFIG_CDC_DEBUG
    LOG_D(TAG, "debug message");
#endif
```

## References
- cdc_log library: Project's unified logging system
- ESP_LOG vs cdc_log: cdc_log routes through USB CDC with buffering
- CalEPD files affected: ~20 model files + core library
- ESP-IDF logging: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/log.html
