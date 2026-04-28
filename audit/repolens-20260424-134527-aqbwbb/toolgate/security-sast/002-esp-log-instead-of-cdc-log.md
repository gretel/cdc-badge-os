---
title: "[MEDIUM] Use of ESP_LOG instead of cdc_log in multiple files"
severity: MEDIUM
domain: security
lens: sast
labels:
  - "logging"
  - "consistency"
  - "cdc_log"
---

## Summary
Multiple source files use `ESP_LOG` macros (from `esp_log.h`) instead of the project's required `cdc_log` library. The project documentation explicitly states that `ESP_LOG` should never be used directly as it bypasses USB CDC logging.

## Impact
- **Debuggability**: Logs from `ESP_LOG` may not appear in the serial monitor, making debugging harder
- **Consistency**: Mixed logging styles make log parsing and analysis inconsistent
- **Security**: Security-relevant logs may be missed during incident analysis if they use ESP_LOG

## Evidence
**Files with `esp_log.h` includes in CalEPD component (headers):**
- `components/CalEPD/include/epdParallel.h:12`
- `components/CalEPD/include/gdep015OC1.h:9`
- `components/CalEPD/include/gdew075HD.h:10`
- `components/CalEPD/include/parallel/ED060SC4.h:11`
- `components/CalEPD/include/parallel/ED047TC1touch.h:11`
- `components/CalEPD/include/parallel/ED047TC1.h:12`
- `components/CalEPD/include/gdew0213i5f.h:9`
- `components/CalEPD/include/gdew042t2Grays.h:10`
- `components/CalEPD/include/wave12i48.h:10`
- `components/CalEPD/include/gdew027w3.h:9`
- `components/CalEPD/include/gdeh0154d67.h:10`
- `components/CalEPD/include/plasticlogic.h:10`
- `components/CalEPD/include/heltec0151.h:9`
- `components/CalEPD/include/color/gdeh042Z21.h:10`
- `components/CalEPD/include/color/gdeh042Z98.h:10`
- `components/CalEPD/include/color/gdew0583z83.h:10`
- `components/CalEPD/include/color/gdew075z09.h:10`
- `components/CalEPD/include/color/gdew027c44.h:9`
- `components/CalEPD/include/color/wave5i7Color.h:10`
- `components/CalEPD/include/color/wave4i7Color.h:10`

**Files with `esp_log.h` includes in CalEPD (sources):**
- `components/CalEPD/models/wave12i48.cpp:5`
- `components/CalEPD/models/gdem029E97.cpp:4`
- `components/CalEPD/models/parallel/ED047TC1touch.cpp:7`
- `components/CalEPD/models/parallel/ED047TC1.cpp:7`
- `components/CalEPD/models/parallel/ED060SC4.cpp:7`
- `components/CalEPD/models/gdew075HD.cpp:4`
- `components/CalEPD/models/gdeh0154d67.cpp:4`
- `components/CalEPD/models/gdew075T7Grays.cpp:4`
- And many more color model files...

**Files with actual ESP_LOG calls:**
- `components/mod_gpg/src/openpgp/openpgp.cpp:19` - includes `esp_log.h`
- `components/CalEPD/epd.cpp:110,130` - uses `ESP_LOGE`
- `components/CalEPD/epdspi.cpp:67,76,80,94,115,147,149,178,180` - uses `ESP_LOGE`, `ESP_LOGI`
- And 200+ more ESP_LOG calls across the codebase

## Recommended Fix
1. **For CalEPD component**: Replace `#include "esp_log.h"` with `#include "cdc_log.h"` in all header and source files
2. **Replace ESP_LOG macros**:
   - `ESP_LOGE(TAG, "...")` → `LOG_E(TAG, "...")`
   - `ESP_LOGI(TAG, "...")` → `LOG_I(TAG, "...")`
   - `ESP_LOGW(TAG, "...")` → `LOG_W(TAG, "...")`
   - `ESP_LOGD(TAG, "...")` → `LOG_D(TAG, "...")`

3. **Add cdc_log dependency**: Update CalEPD component's CMakeLists.txt to include `cdc_log` in REQUIRES list

## References
- Project documentation: `CLAUDE.md` section "Logging - CRITICAL"
- CWE-481: Improper definition or formatting of log message
