---
title: "[MEDIUM] PII Exposed in Debug Logs - GPG Cardholder Data Printed via ESP_LOG"
severity: MEDIUM
domain: gdpr-dsgvo
lens: compliance/gdpr-dsgvo
labels:
  - "audit:compliance/gdpr-dsgvo"
---

## Summary
The GPG module (`components/mod_gpg/src/openpgp/openpgp.cpp`) logs personal data (cardholder name, URL, login data) directly to the serial output using `ESP_LOGI`, bypassing the project's standard `cdc_log` wrapper. This exposes PII (personally identifiable information) in the debug log output, which can be accessed via USB CDC serial connection.

**Affected logging statements:**
- Line 911: `ESP_LOGI(TAG, "Cardholder name set: %s", cardholder_name);`
- Line 1017: `ESP_LOGI(TAG, "URL set: %s", cardholder_url);`
- Line 1028: `ESP_LOGI(TAG, "Login set: %s", cardholder_login);`

## Impact
**Privacy Risk**: Personal data stored in the GPG application (name, URL, login) is printed to serial output, making it visible to anyone with physical access to the device or who can monitor the serial connection.

**Data Controller Responsibility**: Under Art. 5(1)(f) DSGVO (integrity and confidentiality), personal data must be processed securely. Logging PII to serial output without masking or configuration options is a potential breach of this principle.

**Debug vs. Production**: While debug logging is useful, the code should either:
1. Use the project's `cdc_log` system with configurable levels
2. Mask or truncate sensitive fields
3. Only log at DEBUG level with compile-time flag control

**Evidence**:
```cpp
// components/mod_gpg/src/openpgp/openpgp.cpp:911
case DO_NAME:
    if (apdu->lc < sizeof(cardholder_name)) {
        memcpy(cardholder_name, apdu->data, apdu->lc);
        cardholder_name[apdu->lc] = '\0';
        save_state_to_nvs();
        ESP_LOGI(TAG, "Cardholder name set: %s", cardholder_name);  // PII exposed!
        return apdu_sw(resp, SW_OK);
    }

// components/mod_gpg/src/openpgp/openpgp.cpp:1017
case DO_URL:
    if (apdu->lc < sizeof(cardholder_url)) {
        memcpy(cardholder_url, apdu->data, apdu->lc);
        cardholder_url[apdu->lc] = '\0';
        save_state_to_nvs();
        ESP_LOGI(TAG, "URL set: %s", cardholder_url);  // PII exposed!
        return apdu_sw(resp, SW_OK);
    }

// components/mod_gpg/src/openpgp/openpgp.cpp:1028
case DO_LOGIN:
    if (apdu->lc < sizeof(cardholder_login)) {
        memcpy(cardholder_login, apdu->data, apdu->lc);
        cardholder_login[apdu->lc] = '\0';
        save_state_to_nvs();
        ESP_LOGI(TAG, "Login set: %s", cardholder_login);  // PII exposed!
        return apdu_sw(resp, SW_OK);
    }
```

## Recommended Fix

### Option 1: Use cdc_log with DEBUG Level (~30 min)
Replace `ESP_LOGI` with `LOG_D` from the cdc_log system:

```cpp
// Change from:
#include "esp_log.h"
ESP_LOGI(TAG, "Cardholder name set: %s", cardholder_name);

// To:
#include "cdc_log.h"
LOG_D(TAG, "Cardholder name set: %s", cardholder_name);
```

This ensures:
- Logs only appear when DEBUG logging is enabled
- Uses the project's unified logging system (USB CDC output)

### Option 2: Mask Sensitive Fields (~30 min)
If logging at INFO level is required, mask the actual values:

```cpp
// Log only that the field was set, not the value
ESP_LOGI(TAG, "Cardholder name set (length: %zu)", strlen(cardholder_name));

// Or show only first few characters
char masked[16];
snprintf(masked, sizeof(masked), "%.3s...", cardholder_name);
ESP_LOGI(TAG, "Cardholder name set: %s", masked);
```

### Option 3: Add Build Flag Control (~45 min)
Add a feature flag to control PII logging:

```cpp
// In feature_flags.h
#define LOG_PII_DATA 0  // Default: disabled

// In openpgp.cpp
#if LOG_PII_DATA
    ESP_LOGI(TAG, "Cardholder name set: %s", cardholder_name);
#else
    ESP_LOGI(TAG, "Cardholder name set");
#endif
```

### Recommended Implementation (Option 1 + 2)
```cpp
// components/mod_gpg/src/openpgp/openpgp.cpp

// Add at top:
#include "cdc_log.h"

// Replace lines 911, 1017, 1028:
// Before: ESP_LOGI(TAG, "Cardholder name set: %s", cardholder_name);
// After:  LOG_D(TAG, "Cardholder name set (length: %zu)", strlen(cardholder_name));

// Before: ESP_LOGI(TAG, "URL set: %s", cardholder_url);
// After:  LOG_D(TAG, "URL set (length: %zu)", strlen(cardholder_url));

// Before: ESP_LOGI(TAG, "Login set: %s", cardholder_login);
// After:  LOG_D(TAG, "Login set (length: %zu)", strlen(cardholder_login));
```

## References
- **Art. 5(1)(f) DSGVO** - Integrity and confidentiality (personal data processed securely)
- **Art. 32 DSGVO** - Security of processing (appropriate technical measures)
- **BfD Logging Guidelines** - https://www.bfdi.bund.de/
- **OWASP Logging Cheat Sheet** - https://cheatsheetseries.owasp.org/cheatsheets/Logging_Cheat_Sheet.html
- **ENISA Data Protection by Design** - https://www.enisa.europa.eu/publications/data-protection-by-design-and-by-default

</content>