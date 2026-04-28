---
title: "[MEDIUM] ESP_LOG macros used instead of cdc_log in mod_gpg component"
severity: MEDIUM
domain: maintainability
lens: tech-debt/logging
labels:
  - logging-standards
---

## Summary
The `mod_gpg` component uses `ESP_LOG*` macros directly instead of the project's standardized `cdc_log` library. This affects `/input/20260423-132359-oj8ayc/cdc-badge-os/components/mod_gpg/src/openpgp/openpgp.cpp` (47 occurrences starting at line 601).

## Impact
- **Inconsistent logging**: Bypasses the project's centralized logging system
- **Serial output issues**: ESP_LOG may not appear in USB CDC serial monitor (115200 baud)
- **Maintenance burden**: Inconsistent patterns across the codebase make debugging harder
- **Feature flag support**: Loses ability to toggle logging via cdc_log configuration

## Evidence
File: `components/mod_gpg/src/openpgp/openpgp.cpp`
- Line 19: `#include <esp_log.h>` (correct header included)
- Line 25: `static const char *TAG = "OpenPGP";` (ESP_LOG style TAG)
- Line 601: `ESP_LOGI(TAG, "AID initialized: Manufacturer=0x%02X%02X...`
- Line 606: `ESP_LOGW(TAG, "Failed to read MAC, using default AID");`
- Line 623: `ESP_LOGE(TAG, "Failed to initialize GPG/TROPIC01");`
- 44 more occurrences throughout the file

Compare with correct usage in `components/cdc_core/src/AttestationKeyService.cpp`:
```cpp
LOG_I(TAG, "Attestation key ready");
LOG_W(TAG, "Secure element not set");
LOG_E(TAG, "Failed to generate attestation key");
```

## Recommended Fix
1. Remove `#include <esp_log.h>` from line 19
2. Add `#include "cdc_log.h"` to includes
3. Replace all `ESP_LOGI` with `LOG_I`
4. Replace all `ESP_LOGW` with `LOG_W`
5. Replace all `ESP_LOGE` with `LOG_E`
6. Replace all `ESP_LOGD` with `LOG_D`
7. Ensure `cdc_log` is added to REQUIRES in CMakeLists.txt

Use sed for bulk replacement:
```bash
sed -i 's/ESP_LOGI/LOG_I/g' components/mod_gpg/src/openpgp/openpgp.cpp
sed -i 's/ESP_LOGW/LOG_W/g' components/mod_gpg/src/openpgp/openpgp.cpp
sed -i 's/ESP_LOGE/LOG_E/g' components/mod_gpg/src/openpgp/openpgp.cpp
sed -i 's/ESP_LOGD/LOG_D/g' components/mod_gpg/src/openpgp/openpgp.cpp
```

Then manually update the include statement and verify CMakeLists.txt.

## References
- Project documentation: `CLAUDE.md` section "Logging - CRITICAL"
- Correct implementation: `components/cdc_core/src/AttestationKeyService.cpp`
- cdc_log API: `components/cdc_log/include/cdc_log.h`
