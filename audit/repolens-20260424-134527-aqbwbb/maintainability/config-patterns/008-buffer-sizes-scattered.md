---
title: "[MEDIUM] Buffer size configuration scattered across components"
severity: MEDIUM
domain: maintainability/config-patterns
lens: config-patterns
labels:
  - "audit:maintainability/config-patterns"
---

## Summary
Buffer sizes for USB, I2C, SPI, and other communication protocols are hardcoded in multiple files rather than being centralized. This makes it difficult to tune for different memory constraints or performance requirements.

## Impact
1. **Memory tuning**: Cannot easily adjust buffer sizes for different PSRAM availability
2. **Performance tuning**: Cannot optimize for different use cases (e.g., high-throughput vs low-power)
3. **Consistency**: Related buffers (e.g., RX/TX) may have different sizes without clear rationale
4. **Documentation**: No explanation of why specific sizes were chosen

## Evidence
**Buffer sizes scattered across files:**

| Buffer | Size | Location |
|--------|------|----------|
| TinyUSB CDC RX | 256 bytes | `platformio.ini:27` |
| TinyUSB CDC TX | 256 bytes | `platformio.ini:28` |
| CCID max message | 2048 bytes | `components/mod_gpg/include/mod_gpg/openpgp/ccid.h:10` |
| CTAPHID max message | 2048 bytes | `components/mod_fido2/include/mod_fido2/ctaphid.h:10` |
| I2C device count | 4 | `components/cdc_hal/src/I2cBus.cpp:20` |
| MAX_HANDLERS (EventBus) | 16 | `components/cdc_core/include/cdc_core/EventBus.h:15` |
| MAX_SERVICES (ServiceRegistry) | 24 | `components/cdc_core/include/cdc_core/ServiceRegistry.h:12` |
| MAX_MODULES | 16 | `components/cdc_core/include/cdc_core/ModuleRegistry.h:10` |
| MAX_DISABLED_LIST_SIZE | 128 | `components/cdc_core/include/cdc_core/ModuleRegistry.h:13` |
| MAX_COMMANDS | 64 | `components/serial_cmd/src/CommandRegistry.cpp:10` |
| MAX_STRINGS (I18n) | 512 | `components/cdc_ui/include/cdc_ui/I18n.h:10` |
| MAX_PIN_LENGTH | 8 | `components/cdc_views/include/cdc_views/PinEntryView.h:10` |
| MAX_TEXT_LEN (ListView) | 512 | `components/cdc_views/include/cdc_views/ListView.h:10` |
| MAX_ITEMS (ListView) | 512 | `components/cdc_views/include/cdc_views/ListView.h:11` |

**Code examples:**
```cpp
// platformio.ini:27-28 (build flags, not in code)
-D CONFIG_TINYUSB_CDC_RX_BUFSIZE=256
-D CONFIG_TINYUSB_CDC_TX_BUFSIZE=256

// components/mod_gpg/include/mod_gpg/openpgp/ccid.h:10
#define CCID_MAX_MSG_SIZE 2048

// components/cdc_core/include/cdc_core/EventBus.h:15
static constexpr size_t MAX_HANDLERS = 16;

// components/cdc_core/include/cdc_core/ServiceRegistry.h:12
static constexpr size_t MAX_SERVICES = 24;
```

**No configuration documentation:**
- No explanation of why TinyUSB buffers are 256 bytes
- No guidance on tuning buffer sizes for different use cases
- No validation that buffer sizes are appropriate

## Recommended Fix
1. **Create buffer configuration header**:
   ```cpp
   // components/cdc_core/include/cdc_core/BufferConfig.h
   #pragma once
   
   namespace cdc::config {
   namespace buffer {
   
   // === USB Configuration ===
   constexpr size_t TINYUSB_CDC_RX = 256;
   constexpr size_t TINYUSB_CDC_TX = 256;
   constexpr size_t TINYUSB_HID = 64;
   
   // === Protocol buffers ===
   constexpr size_t CCID_MAX_MSG = 2048;
   constexpr size_t CTAPHID_MAX_MSG = 2048;
   
   // === System buffers ===
   constexpr size_t I2C_DEVICES = 4;
   constexpr size_t EVENT_HANDLERS = 16;
   constexpr size_t SERVICES = 24;
   constexpr size_t MODULES = 16;
   constexpr size_t COMMANDS = 64;
   
   // === UI buffers ===
   constexpr size_t I18N_STRINGS = 512;
   constexpr size_t LIST_VIEW_ITEMS = 512;
   constexpr size_t PIN_MAX_LENGTH = 8;
   
   }
   }
   ```

2. **Add build-time overrides**:
   ```cpp
   // components/cdc_core/include/cdc_core/BufferConfig.h
   #ifndef TINYUSB_CDC_RX_BUFSIZE
   #define TINYUSB_CDC_RX_BUFSIZE 256
   #endif
   
   namespace cdc::config {
   namespace buffer {
   constexpr size_t TINYUSB_CDC_RX = TINYUSB_CDC_RX_BUFSIZE;
   constexpr size_t TINYUSB_CDC_TX = TINYUSB_CDC_TX_BUFSIZE;
   }
   }
   ```

3. **Update existing code to use config**:
   ```cpp
   // Instead of:
   static constexpr size_t MAX_HANDLERS = 16;
   
   // Use:
   #include "cdc_core/BufferConfig.h"
   static constexpr size_t MAX_HANDLERS = cdc::config::buffer::EVENT_HANDLERS;
   ```

4. **Document buffer configuration**:
   ```markdown
   ## Buffer Configuration
   
   | Buffer | Default | Tunable | Notes |
   |--------|---------|---------|-------|
   | TinyUSB CDC RX | 256 | Yes | Set via build flag |
   | TinyUSB CDC TX | 256 | Yes | Set via build flag |
   | CCID message | 2048 | No | Fixed by protocol |
   | Event handlers | 16 | Yes | Increase if needed |
   | I18n strings | 512 | Yes | For language expansion |
   ```

## References
- [TinyUSB Configuration](https://github.com/hathach/tinyusb/blob/master/docs/configuration.md)
- [ESP32 Memory Management](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/memory/mem_alloc.html)
