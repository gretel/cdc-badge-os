---
title: "[HIGH] Logging Inconsistency: Mixed ESP_LOG and cdc_log usage across components"
severity: HIGH
domain: logging
lens: consistency
labels:
  - "audit:code-quality/consistency"
  - "logging"
  - "cdc_log"
---

## Summary

The codebase uses two different logging systems inconsistently:
1. **cdc_log library** (recommended): Uses `LOG_I()`, `LOG_E()`, `LOG_W()`, `LOG_D()` macros defined in `cdc_log.h`
2. **ESP_LOG directly** (bypasses CDC): Uses `ESP_LOGI()`, `ESP_LOGE()`, etc. from `esp_log.h`

**Affected files:**
- **CalEPD component**: 121 files include `esp_log.h` and use `ESP_LOG*` macros
- **mod_gpg component**: `openpgp.cpp` includes `<esp_log.h>` and uses `ESP_LOGI()`
- **mod_gpg component**: `openpgp/ccid.cpp` defines custom `CCID_LOG*` macros (3rd logging style)

**Evidence:**
- CalEPD has 50+ files in `components/CalEPD/models/` using `ESP_LOGI()`, `ESP_LOGE()`
- `components/mod_gpg/src/openpgp/openpgp.cpp` line ~100: `ESP_LOGI(TAG, "AID initialized...")`
- `components/mod_gpg/src/openpgp/ccid.cpp`: Defines `CCID_LOG()`, `CCID_LOG_E()`, `CCID_LOG_W()` macros

## Impact

1. **Debugging issues**: ESP_LOG output may not appear in USB CDC serial monitor, only in RTT/ITM
2. **Inconsistent log format**: Different prefixes, colors, and formatting between logging systems
3. **Error log buffer**: cdc_log's error log ring buffer only captures LOG_E/LOG_W from cdc_log, not ESP_LOG
4. **Build size**: Both logging systems increase binary size
5. **Maintenance friction**: Developers need to know which logging style to use where

## Evidence

**CalEPD component files using ESP_LOG (sample):**
```
components/CalEPD/epd.cpp
components/CalEPD/epdspi.cpp
components/CalEPD/include/color/wave5i7Color.h
components/CalEPD/models/wave12i48.cpp
components/CalEPD/models/gdem029E97.cpp
... (90+ more files)
```

**Code examples:**
```cpp
// CalEPD/models/wave12i48.cpp - Using ESP_LOG
#include "esp_log.h"
static const char* TAG = "Epd";
ESP_LOGI(TAG, "_waitBusyM1 for %s", message);

// mod_gpg/src/openpgp/openpgp.cpp - Using ESP_LOG
#include <esp_log.h>
static const char *TAG = "OpenPGP";
ESP_LOGI(TAG, "AID initialized: Manufacturer=0x%02X%02X...", ...);

// mod_gpg/src/openpgp/ccid.cpp - Using custom CCID_LOG
#define CCID_LOG(tag, fmt, ...) LOG_I(tag, fmt, ##__VA_ARGS__)
#define CCID_LOG_E(tag, fmt, ...) LOG_E(tag, fmt, ##__VA_ARGS__)
CCID_LOG(TAG, "CCID initialized");
```

**cdc_log.h documentation states:**
> "The cdc_log library routes all output through USB CDC for proper serial logging. ESP_LOG bypasses this and may not appear in the serial monitor."

## Recommended Fix

**Phase 1: CalEPD Component (can be split into sub-tasks by model category)**

1. Create `components/CalEPD/include/cdcl_log_wrapper.h`:
   ```cpp
   #pragma once
   #include "cdc_log.h"
   #define ESP_LOGI(tag, fmt, ...) LOG_I(tag, fmt, ##__VA_ARGS__)
   #define ESP_LOGE(tag, fmt, ...) LOG_E(tag, fmt, ##__VA_ARGS__)
   #define ESP_LOGW(tag, fmt, ...) LOG_W(tag, fmt, ##__VA_ARGS__)
   #define ESP_LOGD(tag, fmt, ...) LOG_D(tag, fmt, ##__VA_ARGS__)
   ```

2. Replace all `#include "esp_log.h"` in CalEPD with `#include "cdc_log_wrapper.h"`

3. Add `cdc_log` to CalEPD's CMakeLists.txt REQUIRES

**Phase 2: mod_gpg Component**

1. Remove custom `CCID_LOG*` macros from `ccid.cpp`
2. Replace `#include <esp_log.h>` with `#include "cdc_log.h"`
3. Replace all `CCID_LOG*` calls with standard `LOG_*` macros
4. Add `cdc_log` to mod_gpg's CMakeLists.txt REQUIRES

**Phase 3: Verification**

1. Build and verify all logging appears in USB CDC serial monitor
2. Verify error log buffer captures all ERROR/WARN messages
3. Run serial command `ERROR_LOG` to verify captures

## References

- Project documentation: `components/cdc_log/include/cdc_log.h`
- CLAUDE.md logging section: "ALWAYS use the cdc_log library, NEVER use ESP_LOG directly!"
- CalEPD component: `components/CalEPD/`
