---
title: "[MEDIUM] Direct ESP_LOG usage in CalEPD component bypasses CDC logging"
severity: MEDIUM
domain: dead-code
lens: dead-code
labels:
  - "audit:code-quality/dead-code"
---

## Summary
Multiple source files in the `CalEPD` component use `ESP_LOG` macros directly from `esp_log.h` instead of the project's `cdc_log.h` logging library. This bypasses the USB CDC logging system and may cause logs to not appear in the serial monitor.

**Location:** `/input/20260423-132359-oj8ayc/cdc-badge-os/components/CalEPD/`

The CalEPD library is a third-party display driver library that wasn't updated to use the project's logging conventions.

## Impact
- **Debugging difficulty**: Logs may not appear in serial monitor during runtime
- **Inconsistent logging**: Mixed logging styles across the codebase
- **Feature loss**: CDC log library provides routing through USB CDC for proper serial logging

## Evidence
**Files using ESP_LOG directly:**

1. **wave12i48.cpp** (line 5, 197-264):
```cpp
#include "esp_log.h"
...
ESP_LOGI(TAG, "_waitBusyM1 for %s", message);
ESP_LOGI(TAG, "Busy Timeout");
```

2. **gdem029E97.cpp** (line 146, 172-181):
```cpp
ESP_LOGI("PARTIAL", "partial %d x:%d y:%d\n", partials, (int)x, (int)y);
ESP_LOGI(TAG, "_waitBusy for %s", message);
```

3. **gdew075HD.cpp** (line 170-183):
```cpp
ESP_LOGI(TAG, "_waitBusy for %s", message);
ESP_LOGI(TAG, "Busy Timeout");
```

4. **gdeh0154d67.cpp** (line 262-293):
```cpp
ESP_LOGI(TAG, "_waitBusy for %s", message);
ESP_LOGI(TAG, "_waitBusy for %s", message);
```

5. **wave5i7Color.h** (line 49):
```cpp
ESP_LOGE("Epd::printerf", "max_buffer out of range. Increase max_buffer!");
```

6. **gdem029E97.cpp** (line 388):
```cpp
ESP_LOGE("Gdem029E97", "This epaper has no 4 gray mode");
```

**Total files affected:** ~20+ files in CalEPD component use `#include "esp_log.h"` and `ESP_LOG*` macros.

## Recommended Fix
Replace ESP_LOG usage with cdc_log macros in critical CalEPD files:

**Option 1: Replace includes and macros**
In each CalEPD source file:
```cpp
// Before:
#include "esp_log.h"
ESP_LOGI(TAG, "Message");

// After:
#include "cdc_log.h"
static const char* TAG = "CalEPD";
LOG_I(TAG, "Message");
```

**Option 2: Create a compatibility header**
Create `components/CalEPD/include/CalEPD/cdc_log_compat.h`:
```cpp
#pragma once
#include "cdc_log.h"

// Map ESP_LOG to CDC_LOG for CalEPD
#define ESP_LOGI(tag, fmt, ...) LOG_I(tag, fmt, ##__VA_ARGS__)
#define ESP_LOGE(tag, fmt, ...) LOG_E(tag, fmt, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) LOG_W(tag, fmt, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) LOG_D(tag, fmt, ##__VA_ARGS__)
```

Then update CalEPD CMakeLists.txt to include this header.

## References
- CDC Log documentation: `components/cdc_log/include/cdc_log.h`
- Project logging guidelines: See `CLAUDE.md` "Logging - CRITICAL" section
- ESP-IDF ESP_LOG: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/log.html
