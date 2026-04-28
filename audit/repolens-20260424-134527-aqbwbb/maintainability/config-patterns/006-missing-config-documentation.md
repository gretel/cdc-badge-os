---
title: "[LOW] Missing configuration documentation for new developers"
severity: LOW
domain: maintainability/config-patterns
lens: config-patterns
labels:
  - "audit:maintainability/config-patterns"
---

## Summary
There is no centralized documentation explaining all configuration options for the project. New developers must search through multiple files (platformio.ini, sdkconfig.defaults, feature_flags.h, hw_config.h) to understand available configuration.

## Impact
1. **Onboarding friction**: New developers spend time searching for configuration information
2. **Configuration errors**: Easy to miss important settings or set incorrect values
3. **Inconsistent builds**: Different developers may configure differently without knowing
4. **Hardware setup unclear**: No guide for adapting to hardware variants

## Evidence
**Configuration files exist but are not documented:**
- `platformio.ini` - Build flags, TinyUSB configuration (no explanation of flags)
- `sdkconfig.defaults` - ESP-IDF config (no comments explaining each setting)
- `components/cdc_core/feature_flags.h` - Feature flags (minimal comments)
- `components/cdc_hal/hw_config.h` - Hardware pins (basic comments only)
- `main/tropic_slot_map.h` - TROPIC01 slots (has comments but not in docs)

**Missing documentation:**
- No `.env.example` file showing build flag examples
- No `CONFIGURATION.md` or similar explaining all settings
- No guide for hardware customization
- No guide for feature flag combinations

**Code without context:**
```cpp
// sdkconfig.defaults:3 (no explanation of why 16384)
CONFIG_ESP_MAIN_TASK_STACK_SIZE=16384

// platformio.ini:27 (no explanation of buffer sizes)
-D CONFIG_TINYUSB_CDC_RX_BUFSIZE=256
-D CONFIG_TINYUSB_CDC_TX_BUFSIZE=256

// feature_flags.h:30 (no explanation of what DEBUG_MODE controls)
#define DEBUG_MODE 1
```

## Recommended Fix
1. **Create `docs/CONFIGURATION.md`**:
   ```markdown
   # Configuration Guide
   
   ## Quick Start
   For development, use default settings. For production, see "Production Settings" below.
   
   ## Build Configuration
   
   ### Feature Flags
   Set in `platformio.ini` build_flags:
   
   | Flag | Default | Dev | Prod | Description |
   |------|---------|-----|------|-------------|
   | DEBUG_MODE | 1 | 1 | 0 | Disable security lockouts |
   | FEATURE_NVS_EDIT | 0 | 1 | 0 | NVS editor tool |
   | FEATURE_SECURE_SERIAL | 1 | 1 | 1 | PIN for serial |
   
   ### Example platformio.ini
   ```ini
   build_flags =
       -DDEBUG_MODE=0
       -DFEATURE_NVS_EDIT=0
   ```
   
   ## Hardware Configuration
   
   ### Pin Configuration
   All pins defined in `components/cdc_hal/include/cdc_hal/hw_config.h`
   
   To customize for hardware variant:
   1. Edit `hw_config.h`
   2. Update `sdkconfig.defaults` for E-Paper pins
   3. Rebuild
   
   ### E-Paper Display
   Configured via Kconfig in `sdkconfig.defaults`:
   ```
   CONFIG_EINK_SPI_MOSI=13
   CONFIG_EINK_SPI_CLK=12
   ...
   ```
   
   ## NVS Storage Layout
   See `docs/NVS_SCHEMA.md` for data layout.
   
   ## Timeout Configuration
   All timeouts defined in `components/cdc_core/include/cdc_core/Config.h`
   
   ## Production Checklist
   - [ ] Set `DEBUG_MODE=0`
   - [ ] Set `FEATURE_NVS_EDIT=0`
   - [ ] Verify all timeout values
   - [ ] Check KDF iterations (>= 100000)
   ```

2. **Add comments to sdkconfig.defaults**:
   ```ini
   # CDC Badge OS v0.5 - SDK Configuration
   # Main task stack: 16KB (enough for crypto + WiFi operations)
   CONFIG_ESP_MAIN_TASK_STACK_SIZE=16384
   
   # PSRAM: Octal mode, 80MHz, allow BSS in external memory
   CONFIG_SPIRAM_MODE_OCT=y
   CONFIG_SPIRAM_SPEED_80M=y
   CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y
   ```

3. **Create `.env.example`**:
   ```bash
   # CDC Badge OS - Build Configuration Example
   # Copy to .env and adjust for your environment
   
   # Development settings
   DEBUG_MODE=1
   FEATURE_NVS_EDIT=1
   
   # Production settings (uncomment for release)
   # DEBUG_MODE=0
   # FEATURE_NVS_EDIT=0
   ```

## References
- [12-Factor App - Config](https://12factor.net/config)
- [ESP-IDF Configuration](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/kconfig.html)
