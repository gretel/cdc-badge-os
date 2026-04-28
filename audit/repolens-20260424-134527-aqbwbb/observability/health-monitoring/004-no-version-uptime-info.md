---
title: "[LOW] Health response lacks version and build metadata"
severity: LOW
domain: observability
lens: health-monitoring
labels:
  - "audit:observability/health-monitoring"
---

## Summary

The `STATUS` command (`components/serial_cmd/src/SerialCmd.cpp:427-434`) does not include version information or build metadata that would be useful for:
- Fleet management (identifying firmware versions across devices)
- Debugging (correlating issues with specific builds)
- Rollback verification (confirming successful firmware updates)

**Current output:**
```
=== System Status ===
Free heap: 12345 bytes
Min free heap: 6789 bytes
Uptime: 123456 ms
```

**Missing information:**
- Firmware version (e.g., v0.5)
- Build date/time
- Git commit hash (for development builds)
- Configuration flags (DEBUG_MODE, FEATURE_SECURE_SERIAL, etc.)

## Impact

- **Fleet management difficulty**: Cannot easily identify which devices are running which firmware version
- **Debugging overhead**: Developers need to ask users for version info via multiple commands
- **Release verification**: No easy way to confirm a firmware update was successful

## Evidence

**File:** `components/serial_cmd/src/SerialCmd.cpp:427-434`

**File:** `main/main.cpp:81-83` - Boot log shows version but not in STATUS:
```cpp
LOG_I(TAG, "CDC Badge OS v0.5");
LOG_I(TAG, "Modular Rewrite");
```

## Recommended Fix

Enhance the `STATUS` command to include version and build metadata:

1. **Add version strings to the build:**
   ```cpp
   #define FIRMWARE_VERSION "v0.5"
   #define BUILD_DATE __DATE__
   #define BUILD_TIME __TIME__
   ```

2. **Update STATUS output:**
   ```
   === System Status ===
   Version: v0.5
   Build: 2026-01-15 14:30:22
   Free heap: 12345 bytes
   Min free heap: 6789 bytes
   Uptime: 123456 ms
   ```

3. **Optionally add a `VERSION` command** for concise version output:
   ```
   VERSION - Show firmware version and build info
   ```

## References

- Semantic Versioning: https://semver.org/
- ESP32 build system: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/build-system/index.html
