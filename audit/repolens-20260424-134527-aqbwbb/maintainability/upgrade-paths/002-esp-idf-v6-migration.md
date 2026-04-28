---
title: "[MEDIUM] ESP-IDF v5.5.0 to v6.0 migration planning needed"
severity: MEDIUM
domain: framework
lens: upgrade-paths
labels:
  - "audit:maintainability/upgrade-paths"
---

## Summary
The project uses **ESP-IDF v5.5.0** (via `platform-espressif32@6.12.0` with `framework-espidf @ ~3.50500.0`). ESP-IDF **v6.0** is available as a major release with significant changes that require planned migration.

**Evidence:**
- Current: `platform-espressif32@6.12.0` + `framework-espidf @ ~3.50500.0` (platformio.ini:16)
- ESP-IDF version: `CONFIG_IDF_INIT_VERSION="5.5.0"` (sdkconfig.cdc_badge_usb)
- Latest: ESP-IDF v6.0 (stable release available)

## Impact
**ESP-IDF v6.0 Breaking Changes (from release notes):**
- **Toolchain update**: GCC 12.4 → LLVM 15 (new compiler toolchain)
- **CMake improvements**: New CMake 3.24+ requirements, changed component structure
- **API changes**: Some deprecated APIs removed in v6.0
- **NimBLE updates**: Bluetooth stack changes may affect `mod_ble_serial`, `mod_vcard`, `mod_hid`
- **TinyUSB**: Updates to USB device stack (project uses TinyUSB for CDC/HID)

**Benefits:**
- Improved performance and memory efficiency
- Better power management for deep sleep
- Latest security patches
- Extended support window

**Risk:**
- Migration requires testing all modules, especially BLE and USB components
- Custom patches and build configuration may need adjustments

## Evidence
From `platformio.ini`:
```ini
platform = espressif32@6.12.0
platform_packages =
    framework-espidf @ ~3.50500.0  # Maps to ESP-IDF v5.5.0
```

From `sdkconfig.cdc_badge_usb`:
```
CONFIG_IDF_INIT_VERSION="5.5.0"
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_TINYUSB_ENABLED=1
```

ESP-IDF v6.0 is the latest stable release with significant changes.

## Recommended Fix
**Phase 1 - Assessment (1 hour):**
1. Review [ESP-IDF v6.0 Migration Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/migration-guide/v6.0.html)
2. Identify deprecated APIs used in the codebase:
   ```bash
   grep -r "esp_log.h\|ESP_LOG" main/ components/
   ```

**Phase 2 - Prepare (1-2 hours):**
1. Update `platformio.ini` to test v6.0:
   ```ini
   platform = espressif32@6.13.0
   platform_packages =
       framework-espidf @ ~3.60000.0
   ```

2. Update `sdkconfig.defaults` if needed for new defaults

**Phase 3 - Test (2-3 hours):**
1. Build with ESP-IDF v6.0
2. Test all modules: FIDO2, TOTP, password vault, GPG, BLE, USB
3. Verify patches still apply

**Note:** This is a **planning issue**. Full migration can be done incrementally.

## References
- [ESP-IDF v6.0 Release Notes](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32s3/release-notes.html)
- [ESP-IDF v6.0 Migration Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/migration-guide/v6.0.html)
- [PlatformIO ESP32 Platform v6.13.0](https://github.com/platformio/platform-espressif32/releases/tag/v6.13.0)
- [ESP-IDF GitHub Tags](https://github.com/espressif/esp-idf/tags)
