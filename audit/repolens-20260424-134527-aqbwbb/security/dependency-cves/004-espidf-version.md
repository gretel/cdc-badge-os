---
title: "[LOW] Outdated ESP-IDF Platform Version in PlatformIO"
severity: LOW
domain: dependencies
lens: dependency-cves
labels:
  - "audit:security/dependency-cves"
---

## Summary
The `platformio.ini` file specifies `espressif32@6.12.0` platform and `framework-espidf @ ~3.50500.0` (ESP-IDF v5.5.0). While ESP-IDF v5.5 is relatively current, the platform version may benefit from updates to include the latest security patches for the ESP32-S3.

**Files affected:**
- `platformio.ini` (line 8: `platform = espressif32@6.12.0`)
- `platformio.ini` (line 16: `framework-espidf @ ~3.50500.0`)

## Impact
1. **Missing security patches**: ESP-IDF receives regular security updates for the ESP32-S3 SoC.
2. **Bluetooth stack updates**: ESP-IDF v5.x has ongoing security fixes for BLE/WiFi stacks.
3. **TinyUSB compatibility**: Newer ESP-IDF versions have improved TinyUSB integration.

## Evidence
From `platformio.ini`:
```ini
platform = espressif32@6.12.0
...
platform_packages =
    framework-espidf @ ~3.50500.0
```

ESP-IDF v5.5.0 is specified via `~3.50500.0` (PlatformIO version mapping).

## Recommended Fix
1. **Check for newer ESP-IDF versions**:
   ```bash
   ~/.platformio/penv/bin/pio update
   ```

2. **Update to latest ESP-IDF v5.x** if available:
   ```ini
   ; In platformio.ini
   platform = espressif32@6.13.0  ; or latest
   platform_packages =
       framework-espidf @ ~3.50501.0  ; or latest v5.5.x
   ```

3. **Review ESP-IDF release notes** for security fixes:
   https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/versions.html

4. **Test thoroughly** after updating, as major version updates may require code changes.

## References
- ESP-IDF releases: https://github.com/espressif/esp-idf/releases
- ESP-IDF changelog: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/history.html
- PlatformIO ESP32 platform: https://registry.platformio.org/tools/espressif/espressif32
