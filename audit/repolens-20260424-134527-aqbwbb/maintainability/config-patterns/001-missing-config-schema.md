---
title: "[MEDIUM] Missing centralized configuration schema and validation"
severity: MEDIUM
domain: maintainability/config-patterns
lens: config-patterns
labels:
  - "audit:maintainability/config-patterns"
---

## Summary
The codebase lacks a centralized configuration schema that documents, validates, and exports all settings. Configuration values are scattered across multiple files with no single source of truth or validation at startup.

Key locations where configuration is defined:
- `components/cdc_core/include/cdc_core/feature_flags.h` - Feature flags (3 flags)
- `components/cdc_hal/include/cdc_hal/hw_config.h` - Hardware pin definitions
- `platformio.ini` - Build flags and TinyUSB configuration
- `sdkconfig.defaults` - ESP-IDF configuration
- `main/tropic_slot_map.h` - TROPIC01 slot allocation

## Impact
1. **No validation at startup**: Application can start with missing or malformed configuration and only fail later at runtime
2. **Documentation gap**: New developers must search multiple files to understand available configuration options
3. **Drift risk**: Related values (e.g., timeout values, buffer sizes) defined in different places can drift out of sync
4. **No type safety**: String environment variables used directly without type coercion

## Evidence
**Scattered configuration locations:**
- `feature_flags.h:30-31`: `#define DEBUG_MODE 1` - No validation
- `hw_config.h:10-11`: `#define I2C0_SDA_PIN GPIO_NUM_17` - Hardware config
- `platformio.ini:25-28`: Build flags for TinyUSB buffers
- `sdkconfig.defaults:3`: `CONFIG_ESP_MAIN_TASK_STACK_SIZE=16384` - Stack config

**No schema documentation:**
- No `config.h` or `Settings.h` that aggregates all settings
- No validation function that checks all configuration values at startup
- No `.env.example` or config documentation listing all available options

**Hardcoded values that should be configurable:**
- `components/cdc_os_ui/src/AppUi.cpp:42`: `INACTIVITY_TIMEOUT_MS = 5 * 60 * 1000` (5 minutes)
- `components/cdc_hal/src/I2cBus.cpp:18`: `I2C_FREQ_HZ = 100000` (100kHz)
- `components/cdc_core/src/PinManager.cpp:23`: `DEFAULT_ITERATIONS = 100000` (KDF iterations)

## Recommended Fix
Create a centralized configuration module:

1. **Create `components/cdc_core/include/cdc_core/Config.h`**:
   ```cpp
   /**
    * Centralized configuration schema with validation.
    * All configuration values should be accessible from here.
    */
   #pragma once
   #include <cstdint>
   
   namespace cdc::config {
   
   // Feature Flags
   constexpr bool FEATURE_SECURE_SERIAL = 
   #ifdef CONFIG_SECURE_SERIAL
       true;
   #else
       false;
   #endif
   
   constexpr bool FEATURE_NVS_EDIT = 
   #ifndef FEATURE_NVS_EDIT
       false;
   #else
       (FEATURE_NVS_EDIT != 0);
   #endif
   
   constexpr bool DEBUG_MODE = 
   #ifndef DEBUG_MODE
       true;
   #else
       (DEBUG_MODE != 0);
   #endif
   
   // Timeout Configuration (in milliseconds)
   constexpr uint32_t INACTIVITY_TIMEOUT_MS = 5 * 60 * 1000;
   constexpr uint32_t AUTH_TIMEOUT_MS = 5 * 60 * 1000;
   constexpr uint32_t I2C_TIMEOUT_MS = 100;
   
   // KDF Configuration
   constexpr uint32_t DEFAULT_KDF_ITERATIONS = 100000;
   
   // I2C Configuration
   constexpr uint32_t I2C_FREQ_HZ = 100000;
   
   // Validate all configuration at startup
   bool validate();
   
   } // namespace cdc::config
   ```

2. **Create `components/cdc_core/src/Config.cpp`**:
   ```cpp
   #include "cdc_core/Config.h"
   #include "cdc_log.h"
   
   namespace cdc::config {
   
   bool validate() {
       bool ok = true;
       
       // Validate timeouts
       if (INACTIVITY_TIMEOUT_MS < 60000) {
           LOG_W("Config", "INACTIVITY_TIMEOUT_MS < 60s may cause premature sleep");
       }
       
       // Validate KDF iterations (security)
       if (DEFAULT_KDF_ITERATIONS < 50000) {
           LOG_W("Config", "KDF iterations < 50000 may be weak for brute-force protection");
       }
       
       // Validate I2C frequency
       if (I2C_FREQ_HZ > 400000) {
           LOG_W("Config", "I2C frequency > 400kHz may cause communication errors");
       }
       
       return ok;
   }
   
   } // namespace cdc::config
   ```

3. **Call validation in `main.cpp` after initialization**:
   ```cpp
   if (!cdc::config::validate()) {
       LOG_E("App", "Configuration validation failed, continuing with warnings...");
   }
   ```

4. **Update existing code to use centralized config**:
   Replace hardcoded values with `cdc::config::INACTIVITY_TIMEOUT_MS`, etc.

## References
- [ESP-IDF Configuration Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/kconfig.html)
- [Configuration Management Best Practices](https://12factor.net/config)
- [C++ Configuration Patterns](https://en.cppreference.com/w/cpp/language/constexpr)
