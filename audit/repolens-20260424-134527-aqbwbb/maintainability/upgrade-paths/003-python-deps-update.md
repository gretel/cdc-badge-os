---
title: "[LOW] Python dependencies outdated in tools/requirements.txt"
severity: LOW
domain: dependencies
lens: upgrade-paths
labels:
  - "audit:maintainability/upgrade-paths"
---

## Summary
Python tool dependencies in `tools/requirements.txt` are using old versions without version pinning, which may lead to compatibility issues and missing bug fixes.

**Evidence:**
- File: `tools/requirements.txt`
- Current versions:
  - `esptool>=4.7` (no upper bound, latest is v5.x)
  - `requests>=2.28` (latest is v2.32.x)
  - `bleak>=0.21` (latest is v0.22.x)
  - `esp-coredump>=1.5` (latest is v2.x)

Also in `third_party/libtropic/scripts/test_runner/requirements.txt`:
- `pyserial==3.5` (released 2020, latest is v3.5 but check for newer)
- `telnetlib3==2.0.4`

## Impact
**risks:**
- **esptool**: v5.0+ has improved ESP32-S3 support and faster flashing
- **requests**: v2.28+ is good, but v2.32 has security fixes
- **bleak**: v0.22 has better BLE 5.0+ support
- **esp-coredump**: v2.0 has improved crash analysis for ESP-IDF v5.x

**Benefits:**
- Security patches
- Better ESP32-S3 support
- Improved tool reliability

## Evidence
From `tools/requirements.txt`:
```
esptool>=4.7
requests>=2.28
bleak>=0.21
esp-coredump>=1.5
```

From `third_party/libtropic/scripts/test_runner/requirements.txt`:
```
pyserial==3.5
telnetlib3==2.0.4
```

## Recommended Fix
1. **Update version constraints** in `tools/requirements.txt`:
   ```
   esptool>=4.7,<6.0
   requests>=2.28,<3.0
   bleak>=0.21,<0.23
   esp-coredump>=1.5,<3.0
   ```

2. **Test tools** after update:
   ```bash
   pip install -r tools/requirements.txt --upgrade
   python tools/flash_firmware.py --help
   python tools/ble_serial.py --help
   ```

3. **Check pyserial**: v3.5 is still current. No update needed.

## References
- [esptool releases](https://github.com/espressif/esptool/releases)
- [bleak releases](https://github.com/bleak/bleak/releases)
- [esp-coredump releases](https://github.com/espressif/esp-coredump/releases)
