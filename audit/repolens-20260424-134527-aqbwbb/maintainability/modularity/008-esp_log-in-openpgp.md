---
title: "[MEDIUM] openpgp.cpp uses ESP_LOG instead of cdc_log"
severity: MEDIUM
domain: modularity
lens: modularity
labels:
  - "audit:maintainability/modularity"
---

## Summary
The `openpgp.cpp` file uses `ESP_LOGI`, `ESP_LOGW`, `ESP_LOGE` directly instead of the project's `cdc_log` library. This breaks the modular logging abstraction and means output may not appear in the USB CDC serial monitor consistently.

**Evidence:**
- File: `components/mod_gpg/src/openpgp/openpgp.cpp`
- Lines 601, 606, 623, 632, 670, 675, 694, 885, 911, 940+ use `ESP_LOG*` macros
- Should use: `LOG_I(TAG, "...")`, `LOG_W(TAG, "...")`, `LOG_E(TAG, "...")`

## Impact
1. **Logging inconsistency**: ESP_LOG bypasses USB CDC routing, may not appear in serial monitor
2. **Debug difficulty**: Harder to trace issues when logs come from different sources
3. **Build dependency**: Ties code to ESP-IDF directly instead of abstraction
4. **Pattern violation**: Project standard (per `CLAUDE.md`) is to always use `cdc_log`

## Evidence
**openpgp.cpp** (lines 601-632):
```cpp
#include "mod_gpg/openpgp/openpgp.h"
#include "mod_gpg/openpgp/apdu.h"
// ... includes ...
#include <esp_log.h>  // Wrong! Should use cdc_log.h

static const char *TAG = "OpenPGP";

// ... code ...

ESP_LOGI(TAG, "OpenPGP application initialized, sig_count=%lu", sig_count);  // Wrong!
```

**Correct pattern (from GroveLedModule.cpp):**
```cpp
#include "cdc_log.h"
static const char* TAG = "GroveLED";
// ...
LOG_I(TAG, "Registered i18n strings (base=%d)", s_strIdBase);
LOG_E(TAG, "Failed to register i18n strings");
```

## Recommended Fix
1. Replace `#include <esp_log.h>` with `#include "cdc_log.h"` (2 minutes)
2. Replace all `ESP_LOGI(TAG, ...)` with `LOG_I(TAG, ...)` (5 minutes)
3. Replace all `ESP_LOGW(TAG, ...)` with `LOG_W(TAG, ...)` (5 minutes)
4. Replace all `ESP_LOGE(TAG, ...)` with `LOG_E(TAG, ...)` (5 minutes)
5. Verify compilation and test logging output (5 minutes)

**Total estimated time: ~25 minutes**

## References
- Project documentation: `CLAUDE.md` - "ALWAYS use the cdc_log library, NEVER use ESP_LOG directly!"
- Similar issue in other files should also be checked and fixed
