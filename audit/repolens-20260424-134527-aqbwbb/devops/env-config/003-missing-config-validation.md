---
title: "[LOW] Missing startup validation for required configuration"
severity: LOW
domain: environment configuration
lens: env-config
labels:
  - "audit:devops/env-config"
---

## Summary
The firmware does not validate critical configuration at startup. There is no centralized validation step that checks if all required settings are present and within acceptable ranges before the system starts.

Key configuration that should be validated but isn't:
- Feature flags (DEBUG_MODE, FEATURE_SECURE_SERIAL)
- Build-time constants (PIN constraints, slot allocations)
- Hardware configuration (pin numbers, I2C addresses)

## Impact
- Configuration errors may cause cryptic runtime failures deep in the call stack
- No early warning if DEBUG_MODE is left enabled in production
- Missing validation of slot allocation ranges in `tropic_slot_map.h`
- Hard-to-debug issues when configuration is inconsistent

## Evidence
File: `main/main.cpp` - Boot sequence at lines 39-100 shows initialization but no config validation:
```cpp
void app_main(void)
{
    // === STAGE 0: Hardware Minimum ===
    esp_err_t ret = nvs_flash_init();
    // ... no config validation before proceeding
}
```

File: `components/cdc_core/include/cdc_core/feature_flags.h` - Feature flags defined but never validated:
```cpp
#ifndef DEBUG_MODE
#define DEBUG_MODE 1
#endif
```

No code checks if `DEBUG_MODE` is set to 1 during production builds.

## Recommended Fix
Add a configuration validation function in `components/cdc_core`:

1. Create `components/cdc_core/include/cdc_core/ConfigValidator.h`:
   ```cpp
   class ConfigValidator {
   public:
       static bool validate();  // Returns true if all config valid
       static void printConfig();  // Dump current config for debugging
   };
   ```

2. Implement checks:
   - Warn if DEBUG_MODE=1 in production
   - Validate slot ranges don't overlap in tropic_slot_map.h
   - Check PIN constraint consistency (min < max, reasonable values)

3. Call in `main.cpp` early boot:
   ```cpp
   if (!ConfigValidator::validate()) {
       LOG_E(TAG, "Configuration validation failed!");
       return;  // Fail fast
   }
   ```

4. Add to `CONFIGURATION.md` documentation

## References
- [Fail-fast principle](https://en.wikipedia.org/wiki/Fail-fast)
- [ESP-IDF startup sequence](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/startup.html)
