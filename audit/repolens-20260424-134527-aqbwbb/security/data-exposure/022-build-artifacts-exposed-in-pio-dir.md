---
title: "[LOW] Build artifacts in .pio directory expose SDK configuration and compiler paths"
severity: LOW
domain: build
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The `.pio/build/cdc_badge_usb/` directory contains build artifacts that expose internal system information including:
- Full SDK configuration (`sdkconfig.h`)
- Compiler paths and include directories (`compile_commands.json`)
- Partition table configuration with coredump settings
- ESP32-S3 specific hardware configuration

These files are typically generated during build and may be inadvertently committed or shared.

## Impact
- **System fingerprinting**: Reveals exact ESP-IDF version and configuration
- **Path disclosure**: Compiler paths may reveal developer environment structure
- **Coredump config**: Indicates where crash dumps are stored (`CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH`)
- **Build reproducibility**: Allows attackers to reproduce build environment exactly

## Evidence
File: `.pio/build/cdc_badge_usb/config/sdkconfig.h` contains:
```c
#define CONFIG_PARTITION_TABLE_FILENAME "partitions_singleapp_coredump.csv"
#define CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH 1
#define CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF 1
#define CONFIG_ESP_COREDUMP_MAX_TASKS_NUM 64
```

File: `.pio/build/cdc_badge_usb/compile_commands.json` (if generated) would contain full paths to:
- ESP-IDF installation
- PlatformIO environment
- Header files

## Recommended Fix
1. Add `.pio/` to `.gitignore` if not already present
2. Document that build directory should not be shared
3. Consider using `gen_compile_commands.py` only when needed, not as part of standard build
4. Review what gets packaged for distribution (should exclude `.pio/`)

## References
- ESP-IDF build system: https://docs.espressif.com/projects/esp-idf/
- PlatformIO build directory: https://docs.platformio.org/en/latest/core.html
- Coredump configuration: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/core_dump.html
