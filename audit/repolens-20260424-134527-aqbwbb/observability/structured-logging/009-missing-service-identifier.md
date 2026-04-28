---
title: "[LOW] Log entries lack global service/component identifier for multi-device correlation"
severity: LOW
domain: observability
lens: structured-logging
labels:
  - "audit:observability/structured-logging"
---

## Summary
Log entries use only a local TAG (e.g., "EventBus", "ModuleReg") but lack a global service/component identifier that would uniquely identify the device or application instance. This makes it difficult to correlate logs when multiple devices send logs to the same aggregation system.

**Location:** `components/cdc_log/include/cdc_log.h` (LOG macros) and all module files that define TAG.

## Impact
1. **Multi-device ambiguity**: Cannot distinguish logs from different badge devices in a shared log system.
2. **Missing context**: No device ID, firmware version, or build timestamp in log entries.
3. **Debug difficulty**: When debugging field issues, cannot easily filter logs by specific device.
4. **Limited analytics**: Cannot generate per-device metrics or dashboards.

## Evidence
Current TAG usage (inconsistent across modules):
```cpp
// AttestationKeyService.cpp
static const char* TAG = "AttestKey";

// EventBus.cpp
static const char* TAG = "EventBus";

// ModuleRegistry.cpp
static const char* TAG = "ModuleReg";
```

Log output format (`components/cdc_log/src/cdc_log.cpp:168-172`):
```cpp
snprintf(line, sizeof(line), "[%s][%s] %s",
         level_str[level], tag ? tag : "???", buf);
// Output: "[E][EventBus] Failed to create event queue"
// No device ID, no service name, no instance identifier
```

Example log output:
```
[14:30:45][E][GpgStorage] Failed to write slot 5
[14:30:46][I][ModuleReg] Registered module 'TOTP'
```

Missing: Device ID, firmware version, build hash, instance name.

## Recommended Fix
Add a global service identifier that is included in every log entry:

**Option 1: Add service name to log format (simplest)**
```cpp
// In cdc_log.h
#define SERVICE_NAME "CDC_BADGE"  // Configurable via build flag

// In cdc_log.cpp
void log_write(log_level_t level, const char* tag, const char* fmt, ...) {
    // ... existing code ...
    
    // Include service name
    snprintf(line, sizeof(line), "[%s][%s][%s] %s",
             level_str[level], SERVICE_NAME, tag ? tag : "???", buf);
}
```

**Option 2: Add device ID from NVS/secure element**
```cpp
// In cdc_log.h
void log_set_device_id(const char* device_id);
const char* log_get_device_id(void);

// In log_write():
snprintf(line, sizeof(line), "[%s][%s][%s][%s] %s",
         level_str[level],
         log_get_device_id(),  // e.g., "BADGE-A1B2C3"
         SERVICE_NAME,
         tag ? tag : "???",
         buf);
```

**Option 3: Add build metadata**
```cpp
// In cdc_log.h (with build-time values)
#define LOG_BUILD_HASH "abc123"
#define LOG_BUILD_DATE __DATE__
#define LOG_VERSION "0.5.0"

// Include in startup log
LOG_I("System", "CDC Badge OS v%s build %s (%s)", 
      LOG_VERSION, LOG_BUILD_HASH, LOG_BUILD_DATE);
```

**Recommendation:** Start with Option 1 (service name) and add Option 3 (build metadata) for development. Option 2 (device ID) can be added later if multi-device correlation becomes necessary.

**Estimated effort:** ~15-30 minutes (add service name to log format).

## References
- Related: Finding #002 (ISO 8601 timestamps) for log format improvements
- Related: Finding #007 (error log tag field) for structured log fields
- Best practice: Add device/service identifiers for distributed systems logging

</content>