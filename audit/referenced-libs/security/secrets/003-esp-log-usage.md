---
title: "[MEDIUM] ESP_LOG Used Instead of cdc_log Library in CalEPD Component"
severity: MEDIUM
domain: secrets
lens: logging
labels:
  - "logging"
  - "esp32"
---

## Summary
The CalEPD component uses `ESP_LOG` macros directly instead of the project's `cdc_log` library. This bypasses the centralized logging system that routes output through USB CDC for proper serial logging. While not a direct secrets exposure issue, this can lead to inconsistent logging and potential leakage of sensitive information through unexpected channels.

Files affected (from `components/CalEPD/`):
- `models/wave12i48.cpp` - Multiple `ESP_LOGI` calls
- `models/gdem029E97.cpp` - Multiple `ESP_LOGI`, `ESP_LOGE` calls
- `models/gdew075HD.cpp` - Multiple `ESP_LOGI` calls
- `models/gdeh0154d67.cpp` - Multiple `ESP_LOGI` calls
- `models/gdew075T7Grays.cpp` - Multiple `ESP_LOGI` calls
- `models/color/dke075z83.cpp` - Multiple `ESP_LOGI` calls
- `models/color/gdew075z09.cpp` - Multiple `ESP_LOGI` calls
- `models/color/gdeh042Z98.cpp` - Multiple `ESP_LOGI` calls
- `models/color/wave5i7Color.cpp` - Multiple `ESP_LOGI` calls
- `include/color/wave5i7Color.h` - `ESP_LOGE` call

Example from `models/wave12i48.cpp`:
```cpp
ESP_LOGI(TAG, "_waitBusyM1 for %s", message);
if (debug_enabled) ESP_LOGI(TAG, "Busy Timeout");
```

## Impact
**Potential for inconsistent logging and secrets leakage:**

1. **Bypasses centralized logging**: `ESP_LOG` outputs directly to ESP-IDF's log system, which may not be routed through USB CDC, making it harder to track all output
2. **Inconsistent log levels**: Different components may use different logging systems, making it harder to control verbosity
3. **Debug information leakage**: Debug logs may accidentally include sensitive data (e.g., passwords, keys) that would be harder to filter with a centralized system
4. **Maintenance burden**: Developers may not know which logging system to use, leading to more inconsistencies

## Evidence
From `components/CalEPD/include/esp_log.h` includes:
```cpp
#include "esp_log.h"  // Direct ESP-IDF logging
```

From `components/CalEPD/models/wave12i48.cpp`:
```cpp
ESP_LOGI(TAG, "_waitBusyM1 for %s", message);
if (debug_enabled) ESP_LOGI(TAG, "Busy Timeout");
```

From `components/CalEPD/include/color/wave5i7Color.h`:
```cpp
ESP_LOGE("Epd::printerf", "max_buffer out of range. Increase max_buffer!");
```

The project guidelines state:
```
**ALWAYS use the cdc_log library, NEVER use ESP_LOG directly!**
```

## Recommended Fix
1. **Replace `ESP_LOG` with `cdc_log` macros**:

   ```cpp
   // Instead of:
   #include "esp_log.h"
   ESP_LOGI(TAG, "Message");
   
   // Use:
   #include "cdc_log.h"
   LOG_I(TAG, "Message");
   ```

2. **Update each affected file**:
   - Replace `#include "esp_log.h"` with `#include "cdc_log.h"`
   - Replace `ESP_LOGI(TAG, ...)` with `LOG_I(TAG, ...)`
   - Replace `ESP_LOGE(TAG, ...)` with `LOG_E(TAG, ...)`
   - Replace `ESP_LOGW(TAG, ...)` with `LOG_W(TAG, ...)`
   - Replace `ESP_LOGD(TAG, ...)` with `LOG_D(TAG, ...)`

3. **Update CMakeLists.txt** to include `cdc_log` in REQUIRES:
   ```cmake
   idf_component_register(
       ...
       REQUIRES
           cdc_log
           ...
   )
   ```

4. **Add lint rule** to catch future `ESP_LOG` usage:
   ```bash
   grep -r "ESP_LOG" components/CalEPD/ --include="*.c" --include="*.cpp" --include="*.h"
   ```

## References
- [Project Logging Guidelines](../../CLAUDE.md#logging---critical)
- [ESP-IDF Logging](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/log.html)
- [CDC Log Library](../../components/cdc_log/README.md)
