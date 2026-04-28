---
title: "[LOW] PlatformIO espressif32 platform at v6.12.0, latest is v6.13.0"
severity: LOW
domain: dependencies
lens: upgrade-paths
labels:
  - "audit:maintainability/upgrade-paths"
---

## Summary
The PlatformIO `espressif32` platform is at version **v6.12.0** in `platformio.ini`. Version **v6.13.0** is available with updated toolchains and esptool.

**Evidence:**
- Current: `platform = espressif32@6.12.0` in `platformio.ini`
- Latest: v6.13.0 (released with ESP-IDF v5.5.3 support)

## Impact
**v6.13.0 includes:**
- ESP-IDF v5.5.3 support (vs v5.5.0 currently)
- Updated IDF toolchains to v14.2.0+20251107 (vs GCC 12.2.0)
- Updated esptoolpy to v4.11.0 (vs v4.8.0)
- Minor fixes and improvements

**Risk:** Low - incremental update with toolchain improvements.

## Evidence
From `platformio.ini`:
```ini
platform = espressif32@6.12.0
platform_packages =
    framework-espidf @ ~3.50500.0
    toolchain-xtensa-esp32s3 @ 12.2.0+20230208
```

PlatformIO espressif32 releases:
```
v6.13.0 - ESP-IDF v5.5.3, toolchains v14.2.0, esptoolpy v4.11.0
v6.12.0 - Current version
```

## Recommended Fix
Update `platformio.ini`:
```ini
; Change from:
platform = espressif32@6.12.0

; To:
platform = espressif32@6.13.0
```

Then rebuild:
```bash
~/.platformio/penv/bin/pio run -t clean
~/.platformio/penv/bin/pio run
```

## References
- [PlatformIO espressif32 releases](https://github.com/platformio/platform-espressif32/releases)
- [PlatformIO ESP32 documentation](https://docs.platformio.org/en/latest/platforms/espressif32.html)
