---
title: "[LOW] esp-coredump dependency version check needed"
severity: LOW
domain: dependencies
lens: upgrade-paths
labels:
  - "audit:maintainability/upgrade-paths"
---

## Summary
The `esp-coredump` dependency in `tools/requirements.txt` is pinned to `>=1.5`. ESP-IDF v5.5.0 uses core dump format v2, and the latest esp-coredump may have better compatibility.

**Evidence:**
- File: `tools/requirements.txt`
- Current: `esp-coredump>=1.5`
- Project uses: `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y` (sdkconfig.defaults:64)
- Data format: `CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF=y` (sdkconfig.defaults:65)

## Impact
**Benefits:**
- v2.0+ has improved ESP-IDF v5.x compatibility
- Better ELF parsing for core dump analysis
- Support for new ESP32-S3 features

**Risk:**
- Minimal - esp-coredump is a CLI tool for analysis

## Evidence
From `tools/requirements.txt`:
```
esp-coredump>=1.5
```

From `sdkconfig.defaults`:
```
CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y
CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF=y
```

## Recommended Fix
1. **Check latest version**:
   ```bash
   pip show esp-coredump
   pip index versions esp-coredump
   ```

2. **Update if needed**:
   ```bash
   pip install esp-coredump>=2.0,<3.0
   ```

3. **Test core dump analysis**:
   ```bash
   espcoredump.py --help
   ```

## References
- [esp-coredump releases](https://github.com/espressif/esp-coredump/releases)
- [esp-coredump documentation](https://docs.espressif.com/projects/esp-coredump/latest/)
