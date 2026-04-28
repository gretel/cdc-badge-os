---
title: "[MEDIUM] CalEPD library uses ESP_LOG which bypasses cdc_log routing"
severity: MEDIUM
domain: CalEPD
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The CalEPD library uses `ESP_LOG*` macros directly instead of the project's `cdc_log` library. This means:
1. Log output bypasses the CDC USB serial routing
2. Logs go directly to UART (visible via USB-Serial-JTAG)
3. Cannot be controlled via `log_set_level()` 
4. Consistent with issue #10 but specific to third-party library

**Locations found:**
- `components/CalEPD/include/color/wave5i7Color.h:61` - ESP_LOGE for max_buffer
- `components/CalEPD/models/wave12i48.cpp:5` - Includes esp_log.h
- `components/CalEPD/models/gdem029E97.cpp` - Multiple ESP_LOGI calls
- 30+ locations across CalEPD models

Example from `wave12i48.cpp:129`:
```cpp
ESP_LOGI(TAG, "_waitBusyM1 for %s", message);
```

## Impact
- **Bypasses log control**: Cannot filter ESP_LOG output via cdc_log API
- **Multiple output paths**: Logs appear on both UART and USB CDC (duplicate output)
- **Debug info exposure**: ESP_LOG output not subject to DEBUG_MODE checks
- **Consistency issue**: Same problem as issue #10 but in third-party code

## Evidence
File: `components/CalEPD/models/wave12i48.cpp:5,129`
```cpp
#include "esp_log.h"
...
ESP_LOGI(TAG, "_waitBusyM1 for %s", message);
```

File: `components/CalEPD/include/color/wave5i7Color.h:61`
```cpp
ESP_LOGE("Epd::printerf", "max_buffer out of range. Increase max_buffer!");
```

## Recommended Fix
1. Create a cdc_log wrapper that can be used as ESP_LOG replacement
2. Modify CalEPD to use cdc_log macros instead of ESP_LOG
3. Add a build flag to suppress ESP_LOG output in production
4. Document that CalEPD debug output cannot be controlled via standard log level

Example wrapper:
```cpp
// In CalEPD models, replace:
#include "esp_log.h"
#define LOG_TAG "CalEPD"

// With:
#include "cdc_log.h"
#define TAG "CalEPD"

// Then replace ESP_LOGI(TAG, ...) with LOG_I(TAG, ...)
```

## References
- Related to issue #10 (ESP_LOG bypasses cdc_log)
- Project logging standard: `components/cdc_log/include/cdc_log.h`
- CalEPD third-party library: `components/CalEPD/`
