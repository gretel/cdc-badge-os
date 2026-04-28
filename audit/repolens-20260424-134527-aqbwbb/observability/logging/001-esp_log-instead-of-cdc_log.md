---
title: "[HIGH] ESP_LOG used instead of cdc_log in multiple source files"
severity: HIGH
domain: logging
lens: logging-coverage
labels:
  - "audit:observability/logging"
---

## Summary
Multiple source files use `ESP_LOG` macros (ESP-IDF native logging) instead of the project's `cdc_log` library. This causes log messages to bypass the USB CDC output and potentially not appear in the serial monitor at all.

**Files affected:**
- `components/mod_gpg/src/openpgp/openpgp.cpp` - 214 occurrences of ESP_LOG
- `components/CalEPD/src/epd.cpp` - 2 occurrences
- `components/CalEPD/src/epdspi.cpp` - 9 occurrences
- `components/CalEPD/models/*.cpp` - Multiple occurrences across models

Example from `components/mod_gpg/src/openpgp/openpgp.cpp:601`:
```cpp
ESP_LOGI(TAG, "AID initialized: Manufacturer=0x%02X%02X Serial=%02X%02X%02X%02X", ...);
```

Example from `components/CalEPD/src/epd.cpp:110`:
```cpp
ESP_LOGE("Epd::printerf", "max_buffer out of range. Increase max_buffer!");
```

## Impact
- **Observability loss**: Error messages may not appear in the USB CDC serial monitor, making debugging difficult
- **Inconsistent logging**: Some components use `cdc_log` (via `LOG_I`, `LOG_E`) while others use `ESP_LOG`, creating inconsistent output formats
- **Missing error context**: Critical errors in GPG and display drivers may be invisible to developers and users

## Evidence
Per the project documentation in `CLAUDE.md`:
```
**ALWAYS use the cdc_log library, NEVER use ESP_LOG directly!**

// CORRECT:
#include "cdc_log.h"
LOG_I(TAG, "Message");
LOG_E(TAG, "Error: %s", msg);

// WRONG - DO NOT USE:
#include "esp_log.h"  // NO!
ESP_LOGI(TAG, "...");  // NO!
```

The `mod_gpg` component has 214 occurrences of ESP_LOG macros (LOGI, LOGE, LOGW, LOGD) that bypass the cdc_log system.

## Recommended Fix
1. Replace all `#include "esp_log.h"` with `#include "cdc_log.h"` in affected files
2. Replace all `ESP_LOGI(TAG, ...)` with `LOG_I(TAG, ...)`
3. Replace all `ESP_LOGE(TAG, ...)` with `LOG_E(TAG, ...)`
4. Replace all `ESP_LOGW(TAG, ...)` with `LOG_W(TAG, ...)`
5. Replace all `ESP_LOGD(TAG, ...)` with `LOG_D(TAG, ...)`
6. Update CMakeLists.txt REQUIRES to include `cdc_log` for each affected component

Start with `components/mod_gpg/src/openpgp/openpgp.cpp` as it has the most occurrences and is a critical security component.

## References
- Project documentation: `CLAUDE.md` - "Logging - CRITICAL" section
- `components/cdc_log/include/cdc_log.h` - Logging API definition
