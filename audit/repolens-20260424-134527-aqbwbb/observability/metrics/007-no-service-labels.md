---
title: "[MEDIUM] No service-level context labels for metrics"
severity: MEDIUM
domain: observability/metrics
lens: service-labels
labels:
  - "metrics"
  - "observability"
  - "context"
---

## Summary
The firmware has no mechanism to attach service-level context labels (service name, version, environment, hardware revision) to metrics. Without these labels, metrics exported to a time-series database cannot be properly filtered, grouped, or correlated across different badge instances or deployments.

**Evidence:**
- `main/main.cpp:2` - Version hardcoded as "CDC Badge OS v0.5" but never exposed as metric label
- `main/main.cpp:78` - Version logged but not available as structured data
- No `build_info` metric with version, commit hash, build date
- No environment label (development, production)
- No hardware revision label (badge v1.0, v2.0)
- No module version labels for individual components

## Impact
Without service-level labels:
- **Multi-tenant dashboards impossible**: Cannot filter metrics by badge instance or deployment
- **Version tracking missing**: Cannot correlate issues with specific firmware versions
- **Rollback validation hard**: Cannot verify if a rollback improved metrics
- **Debug difficulty**: Hard to distinguish between issues in different environments
- **No build traceability**: Cannot identify which build commit produced a given metric

## Evidence
**Version information available but not exposed** (`main/main.cpp`):
```cpp
LOG_I(TAG, "CDC Badge OS v0.5");  // Line 78
LOG_I(TAG, "Modular Rewrite");
```
- Logged as text, not structured as a metric
- No `build_info` gauge with version label

**No build metadata** - Missing:
- Git commit hash
- Build timestamp
- Build configuration (DEBUG_MODE, FEATURE_USB, etc.)
- Hardware revision

**No environment context** - Missing:
- Environment label (dev, test, prod)
- Serial number or device ID for instance identification

**Current serial command output** (`components/serial_cmd/src/SerialCmd.cpp`):
```cpp
static void cmdStatus(const char* args) {
    Console::printf("CDC Badge OS v0.5\r\n");
    Console::printf("Uptime: %llu ms\r\n", ...);
    // No structured labels for metrics export
}
```

## Recommended Fix
1. **Define build info macro** (add to `components/cdc_metrics/include/cdc_metrics.h`):
   ```cpp
   // Build info metric (set once at startup)
   #define METRIC_BUILD_INFO "build_info"
   
   void metrics_set_build_info(const char* version, const char* commit, 
                                const char* env, const char* hardware);
   ```

2. **Add to CMakeLists.txt** (capture build metadata):
   ```cmake
   # Get git info
   execute_process(COMMAND git rev-parse --short HEAD
                   OUTPUT_VARIABLE GIT_COMMIT
                   OUTPUT_STRIP_TRAILING_WHITESPACE)
   execute_process(COMMAND git describe --tags --always
                   OUTPUT_VARIABLE GIT_TAG
                   OUTPUT_STRIP_TRAILING_WHITESPACE)
   
   # Pass to firmware
   target_compile_definitions(cdc_badge_usb PRIVATE
       GIT_COMMIT="${GIT_COMMIT}"
       GIT_TAG="${GIT_TAG}"
       BUILD_DATE="${CMAKE_BUILD_TYPE}")
   ```

3. **Set build info at startup** (`main/main.cpp`):
   ```cpp
   extern "C" void app_main(void) {
       // ... existing init ...
       
       // Set build info metric
       metrics_set_build_info(
           "v0.5",           // version
           GIT_COMMIT,       // commit hash
           "production",     // environment (from build flag)
           "v1.0"           // hardware revision
       );
       
       // ... rest of init ...
   }
   ```

4. **Export in METRICS command**:
   ```
   build_info{version="v0.5",commit="abc123",env="production",hardware="v1.0"} 1
   ```

5. **Add device identity** (optional but recommended):
   ```cpp
   // Get ESP32 unique MAC or generate device ID
   #define METRIC_DEVICE_INFO "device_info"
   void metrics_set_device_info(const char* serial, const char* mac);
   
   // Export:
   device_info{serial="ABC123",mac="24:62:ab:cd:ef:01"} 1
   ```

## References
- [Prometheus build_info convention](https://prometheus.io/docs/practices/instrumentation/#build-information)
- [OpenTelemetry resource attributes](https://opentelemetry.io/docs/specs/otel/resource/sdk/#specifying-resource-attributes)
- ESP32 unique MAC: `esp_efuse_mac_get_default()`

</content>