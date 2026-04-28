---
title: "[MEDIUM] Error Handling Pattern Inconsistency"
severity: MEDIUM
domain: error-handling
lens: code-consistency
labels:
  - "audit:code-quality/consistency"
---

## Summary
The codebase uses multiple error handling patterns without a consistent approach:
1. **esp_err_t**: ESP-IDF style (used in cdc_hal)
2. **SeResult**: Custom enum for secure element
3. **bool return**: Success/failure (used in many places)
4. **ESP_ERROR_CHECK**: Macro that aborts on error (used in CalEPD)

### Evidence

**esp_err_t usage:**
```cpp
// components/cdc_hal/include/cdc_hal/II2cBus.h
virtual esp_err_t addDevice(uint8_t addr, I2cDeviceHandle* out_dev) = 0;
virtual esp_err_t writeReg(I2cDeviceHandle dev, uint8_t reg, ...);
```

**SeResult usage:**
```cpp
// components/cdc_hal/include/cdc_hal/ISecureElement.h
enum class SeResult : uint8_t {
    OK, ERROR, SESSION_REQUIRED, ...
};
virtual SeResult eccGenerate(uint8_t slot, EccCurve curve) = 0;
```

**bool return:**
```cpp
// components/cdc_hal/include/cdc_hal/ISecureElement.h
virtual bool sessionStart() = 0;
virtual bool sessionEnd() = 0;
```

**ESP_ERROR_CHECK (aborts on error):**
```cpp
// components/main/main.cpp
ESP_ERROR_CHECK(nvs_flash_erase());
ESP_ERROR_CHECK(ret);

// components/CalEPD/epd4spi.cpp
ESP_ERROR_CHECK(ret);
```

**Mixed patterns in same file:**
```cpp
// components/mod_gpg/src/openpgp/openpgp.cpp
#include <esp_log.h>  // ESP-IDF style
// Uses esp_err_t, LOG_* macros, custom patterns
```

## Impact
- **Error handling overhead**: Developers must remember multiple error code systems
- **Conversion complexity**: Need to convert between esp_err_t, SeResult, bool
- **Inconsistent error propagation**: Some functions abort, others return errors
- **Testing difficulty**: Different patterns require different test approaches

## Recommended Fix
1. **Establish primary error handling strategy**:
   - Use custom result enums (like SeResult) for domain-specific operations
   - Use esp_err_t only for ESP-IDF interop
   - Use bool only for simple success/failure
2. **Create conversion helpers** between error types
3. **Document error handling policy** for each component

### Priority:
- `main/main.cpp`: Use LOG_E instead of ESP_ERROR_CHECK for non-fatal errors
- CalEPD: Replace ESP_ERROR_CHECK with proper error handling

## References
- ESP-IDF Programming Guide: Error handling
- Project docs: Custom SeResult enum design
