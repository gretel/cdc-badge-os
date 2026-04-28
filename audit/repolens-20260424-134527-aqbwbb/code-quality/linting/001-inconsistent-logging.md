---
title: "[HIGH] Inconsistent logging: ESP_LOG used instead of cdc_log in core modules"
severity: HIGH
domain: linting
lens: code-quality/linting
labels:
  - "audit:code-quality/linting"
---

## Summary
The project defines and documents the use of `cdc_log` as the primary logging library (with macros `LOG_I`, `LOG_E`, `LOG_W`, `LOG_D`), but several modules still use ESP-IDF's native `ESP_LOG*` macros directly.

**Affected files:**
- `components/mod_gpg/src/openpgp/openpgp.cpp` - 30+ uses of `ESP_LOG*`
- `components/CalEPD/` - Multiple files with `ESP_LOG*` usage
- `main/main.cpp` - Includes `esp_log.h` (though primarily uses `cdc_log`)

**Evidence:**
```cpp
// components/mod_gpg/src/openpgp/openpgp.cpp (line 18)
#include <esp_log.h>

// Usage examples (lines 50-100):
ESP_LOGI(TAG, "AID initialized: Manufacturer=0x%02X%02X...");
ESP_LOGW(TAG, "Failed to read MAC, using default AID");
ESP_LOGE(TAG, "Failed to initialize GPG/TROPIC01");
```

```cpp
// main/main.cpp (line 12)
#include "esp_log.h"  // Included but not needed if using cdc_log
```

```cpp
// components/CalEPD/include/epdParallel.h
#include "esp_log.h"
```

## Impact
1. **Bypasses USB CDC logging**: `ESP_LOG` outputs to UART only, missing USB CDC output which is the primary debug interface per project documentation.
2. **Inconsistent log format**: `ESP_LOG` uses different formatting (`[level][TAG]`) vs `cdc_log` format, making log parsing harder.
3. **Documentation violation**: Project docs explicitly state "ALWAYS use the cdc_log library, NEVER use ESP_LOG directly!"
4. **Error log bypass**: `cdc_log` has a PSRAM-backed error log ring buffer for ERROR/WARN messages; `ESP_LOG` bypasses this feature.

## Evidence
- `components/mod_gpg/src/openpgp/openpgp.cpp`: 30+ `ESP_LOG*` calls
- `components/CalEPD/`: 121 files with `#include "esp_log.h"`
- `main/main.cpp`: Line 12 includes `esp_log.h`
- `components/cdc_log/include/cdc_log.h`: Defines `LOG_I`, `LOG_E`, `LOG_W`, `LOG_D` macros

## Recommended Fix
1. **For mod_gpg:**
   - Replace `#include <esp_log.h>` with `#include "cdc_log.h"`
   - Replace all `ESP_LOGI(TAG, ...)` with `LOG_I(TAG, ...)`
   - Replace all `ESP_LOGW(TAG, ...)` with `LOG_W(TAG, ...)`
   - Replace all `ESP_LOGE(TAG, ...)` with `LOG_E(TAG, ...)`
   - Update `components/mod_gpg/CMakeLists.txt` to include `cdc_log` in REQUIRES

2. **For CalEPD (third-party library):**
   - Either: Create a wrapper/header that redirects `ESP_LOG` to `LOG_` macros
   - Or: Accept that third-party code will use ESP_LOG (document this exception)
   - Consider patching CalEPD to use cdc_log if actively maintained

3. **For main.cpp:**
   - Remove `#include "esp_log.h"` if not directly used (only needed if ESP_ERROR_CHECK is used)

## References
- Project documentation: `CLAUDE.md` - "Logging - CRITICAL" section
- `components/cdc_log/include/cdc_log.h` - Logging API definition
- ESP-IDF documentation on logging: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/log.html
