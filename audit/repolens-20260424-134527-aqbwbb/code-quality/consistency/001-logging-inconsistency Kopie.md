---
title: "[HIGH] Logging API Inconsistency: ESP_LOG vs cdc_log"
severity: HIGH
domain: logging
lens: code-consistency
labels:
  - "audit:code-quality/consistency"
---

## Summary
The codebase uses two different logging APIs inconsistently:
1. **cdc_log** (custom wrapper): Used by most components (cdc_core, cdc_hal, cdc_views, modules)
2. **ESP_LOG** (ESP-IDF native): Used by CalEPD component and some other files

This creates a fragmented logging approach where similar operations use different logging mechanisms.

### Files Using ESP_LOG (should use cdc_log):
- `components/CalEPD/` - 100+ files (entire component)
- `components/mod_gpg/src/openpgp/openpgp.cpp`
- `main/main.cpp` (still includes esp_log.h)

### Evidence

**CalEPD component (using ESP_LOG):**
```cpp
// components/CalEPD/models/wave12i48.cpp
#include "esp_log.h"
static const char* TAG = "WAVE12I48";
ESP_LOGI(TAG, "_waitBusyM1 for %s", message);
```

**cdc_log usage (correct pattern):**
```cpp
// components/mod_totp/src/TotpModule.cpp
#include "cdc_log.h"
static const char* TAG = "TOTP";
LOG_I(TAG, "Initializing TOTP module");
```

**main.cpp (mixed usage):**
```cpp
// main/main.cpp
#include "esp_log.h"  // Included but not used for LOG_I, LOG_E, etc.
#include "cdc_log.h"
// Uses LOG_I, LOG_E, LOG_W, LOG_D (cdc_log style)
```

## Impact
- **Maintenance burden**: Developers need to know which logging API to use where
- **Inconsistent output**: ESP_LOG may not route through USB CDC properly
- **Configuration complexity**: Two logging systems to configure
- **According to project docs**: "ALWAYS use the cdc_log library, NEVER use ESP_LOG directly!"

## Recommended Fix
1. Replace all `ESP_LOG*` calls in CalEPD with `LOG_*` macros from cdc_log
2. Change `#include "esp_log.h"` to `#include "cdc_log.h"` in affected files
3. Remove `#include "esp_log.h"` from `main/main.cpp` (already uses cdc_log macros)
4. Verify logging output appears correctly in serial monitor after changes

### Specific replacements needed:
- `ESP_LOGI(TAG, "...")` → `LOG_I(TAG, "...")`
- `ESP_LOGE(TAG, "...")` → `LOG_E(TAG, "...")`
- `ESP_LOGW(TAG, "...")` → `LOG_W(TAG, "...")`
- `ESP_LOGD(TAG, "...")` → `LOG_D(TAG, "...")`

## References
- Project docs: "ALWAYS use the cdc_log library, NEVER use ESP_LOG directly!"
- `components/cdc_log/src/cdc_log.cpp` - Logging implementation
- `components/cdc_log/include/cdc_log.h` - API header
