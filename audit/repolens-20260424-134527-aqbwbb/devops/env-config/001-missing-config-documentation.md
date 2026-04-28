---
title: "[MEDIUM] Missing centralized configuration documentation"
severity: MEDIUM
domain: environment configuration
lens: env-config
labels:
  - "audit:devops/env-config"
---

## Summary
The CDC Badge OS firmware lacks a centralized configuration reference document. There is no `.env.example`, `config.md`, or equivalent documentation file that lists all configurable parameters, their expected values, and where they are used.

Key configuration locations found but undocumented in one place:
- `components/cdc_core/include/cdc_core/feature_flags.h` - Feature toggles (DEBUG_MODE, FEATURE_SECURE_SERIAL, FEATURE_NVS_EDIT)
- `platformio.ini` - Build flags and TinyUSB configuration
- `sdkconfig.defaults` - ESP-IDF SDK configuration
- `components/cdc_hal/include/cdc_hal/hw_config.h` - Hardware pin definitions
- `components/cdc_log/include/cdc_log.h` - Log level configuration

## Impact
- New developers must search through multiple files to understand what can be configured
- Risk of inconsistent configuration across different deployment setups
- Build configuration changes may be missed during onboarding
- No single source of truth for environment-specific settings

## Evidence
Configuration scattered across multiple files:
- `components/cdc_core/include/cdc_core/feature_flags.h` lines 14-31 - Feature flags
- `platformio.ini` lines 18-29 - Build flags for TinyUSB
- `sdkconfig.defaults` - 60+ lines of SDK configuration
- No documentation file referencing all these settings together

## Recommended Fix
Create a `docs/CONFIGURATION.md` file that:

1. Lists all feature flags with their default values and descriptions:
   ```markdown
   ## Feature Flags
   
   | Flag | Default | Description |
   |------|---------|-------------|
   | DEBUG_MODE | 1 | Disables PIN lockouts for development |
   | FEATURE_SECURE_SERIAL | 0 | Require PIN for serial commands |
   | FEATURE_NVS_EDIT | 0 | Enable NVS delete operations |
   ```

2. Documents build flags from `platformio.ini`
3. References ESP-IDF SDK configuration in `sdkconfig.defaults`
4. Lists hardware configuration options from `hw_config.h`
5. Explains how to override defaults for different environments

## References
- [ESP-IDF Configuration System](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/kconfig.html)
- [PlatformIO Build Configuration](https://docs.platformio.org/en/latest/projectconf/section_env_build.html)
