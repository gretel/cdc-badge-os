---
title: "[MEDIUM] Inconsistent logging configuration - mixed esp_log.h and cdc_log.h usage"
severity: MEDIUM
domain: environment configuration
lens: env-config
labels:
  - "audit:devops/env-config"
---

## Summary
The codebase uses two different logging systems inconsistently. The project documentation states to use `cdc_log.h` but several third-party components (CalEPD) include `esp_log.h` directly. This creates configuration inconsistency where log routing depends on which header is included.

## Impact
- ESP_LOG may not route through USB CDC properly (bypasses cdc_log)
- Log output may be inconsistent across modules
- Configuration of log levels may not apply uniformly
- Harder to filter/logs when multiple systems are used

## Evidence
File: `components/cdc_log/include/cdc_log.h` - Project's recommended logging system:
```cpp
// Convenience macros
#define LOG_E(tag, fmt, ...) log_write(CDC_LOG_LEVEL_ERROR,   tag, fmt, ##__VA_ARGS__)
#define LOG_W(tag, fmt, ...) log_write(CDC_LOG_LEVEL_WARN,    tag, fmt, ##__VA_ARGS__)
#define LOG_I(tag, fmt, ...) log_write(CDC_LOG_LEVEL_INFO,    tag, fmt, ##__VA_ARGS__)
```

File: `components/CalEPD/include/epdParallel.h` line 12 - Uses ESP-IDF logging:
```cpp
#include "esp_log.h"
```

Found 30+ CalEPD header files using `esp_log.h`:
- `components/CalEPD/include/epdParallel.h`
- `components/CalEPD/include/gdew075HD.h`
- `components/CalEPD/include/gdep015OC1.h`
- (and 27 more...)

## Recommended Fix
Two options:

**Option A (Recommended): Create a cdc_log wrapper for CalEPD**
1. Create `components/CalEPD/include/cdc_log_compat.h`:
   ```cpp
   #pragma once
   #include "cdc_log.h"
   // Map ESP_LOG macros to cdc_log
   #define ESP_LOGE(tag, fmt, ...) LOG_E(tag, fmt, ##__VA_ARGS__)
   #define ESP_LOGW(tag, fmt, ...) LOG_W(tag, fmt, ##__VA_ARGS__)
   #define ESP_LOGI(tag, fmt, ...) LOG_I(tag, fmt, ##__VA_ARGS__)
   #define ESP_LOGD(tag, fmt, ...) LOG_D(tag, fmt, ##__VA_ARGS__)
   ```

2. Update CalEPD component's `CMakeLists.txt` to include this header first

**Option B: Document the hybrid approach**
1. Add to `CONFIGURATION.md` that third-party components use esp_log
2. Ensure build configuration routes esp_log to USB CDC
3. Document log level configuration for both systems

## References
- [ESP-IDF Logging](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/logging.html)
- Project docs: `docs/README.md` mentions cdc_log usage
