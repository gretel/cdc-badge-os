---
title: "[HIGH] ESP_LOG macros used instead of cdc_log LOG_* macros in openpgp.cpp"
severity: HIGH
domain: observability
lens: structured-logging
labels:
  - "audit:observability/structured-logging"
---

## Summary
The file `components/mod_gpg/src/openpgp/openpgp.cpp` uses ESP-IDF's `ESP_LOG*` macros for logging instead of the project's standardized `cdc_log` library (`LOG_E`, `LOG_W`, `LOG_I`, `LOG_D` macros). This creates inconsistent log output that bypasses the USB CDC logging system.

**Files affected:**
- `components/mod_gpg/src/openpgp/openpgp.cpp` (47 occurrences of ESP_LOG*)

**Location:** Line 19 includes `<esp_log.h>`, lines 601-1687 contain 47 ESP_LOG calls.

## Impact
1. **Inconsistent logging**: The project documentation explicitly states "ALWAYS use the cdc_log library, NEVER use ESP_LOG directly!" - this file violates that convention.
2. **Missing log output**: ESP_LOG may not appear in the serial monitor (USB CDC) since it bypasses the cdc_log routing system.
3. **Mixed log formats**: Other files in the same module (e.g., `GpgStorage.cpp`, `GpgModule.cpp`, `ccid.cpp`) correctly use `LOG_*` macros, creating inconsistency within the same component.
4. **Debug difficulty**: During troubleshooting, log messages from `openpgp.cpp` may be harder to find if they go to a different output channel.

## Evidence
```cpp
// Line 19 - Wrong include
#include <esp_log.h>

// Line 25 - TAG defined for ESP_LOG
static const char *TAG = "OpenPGP";

// Lines 601-1687 - Examples of ESP_LOG usage (47 total)
ESP_LOGI(TAG, "AID initialized: Manufacturer=0x%02X%02X Serial=%02X%02X%02X%02X",
ESP_LOGW(TAG, "Failed to read MAC, using default AID");
ESP_LOGE(TAG, "Failed to initialize GPG/TROPIC01");
```

Compare with correct usage in `components/mod_gpg/src/GpgStorage.cpp`:
```cpp
#include "cdc_log.h"
LOG_E(TAG, "Invalid parameters for save_dec_privkey");
LOG_I(TAG, "Saved encrypted DEC private key to R-Memory slot %d", rmem_slot);
```

## Recommended Fix
1. Replace `#include <esp_log.h>` with `#include "cdc_log.h"` on line 19.
2. Replace all 47 occurrences of:
   - `ESP_LOGE(TAG, ...)` → `LOG_E(TAG, ...)`
   - `ESP_LOGW(TAG, ...)` → `LOG_W(TAG, ...)`
   - `ESP_LOGI(TAG, ...)` → `LOG_I(TAG, ...)`
   - `ESP_LOGD(TAG, ...)` → `LOG_D(TAG, ...)`
3. The `TAG` variable can remain as-is since LOG_* macros accept a tag parameter.

**Estimated effort:** ~30-45 minutes (find/replace operation with verification).

## References
- Project documentation: `CLAUDE.md` - "Logging - CRITICAL" section
- cdc_log header: `components/cdc_log/include/cdc_log.h`
- Related correct implementations: `components/mod_gpg/src/GpgStorage.cpp`, `components/mod_gpg/src/GpgModule.cpp`

</content>