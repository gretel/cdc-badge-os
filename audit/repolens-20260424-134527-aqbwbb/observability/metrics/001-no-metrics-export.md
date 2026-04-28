---
title: "[HIGH] No metrics export mechanism for system observability"
severity: HIGH
domain: observability/metrics
lens: metrics-export
labels:
  - "metrics"
  - "observability"
---

## Summary
The CDC Badge OS firmware has no structured metrics export mechanism. While the system has logging via `cdc_log` (components/cdc_log/include/cdc_log.h) and basic status commands (STATUS, MEM in components/serial_cmd/src/SerialCmd.cpp:424-456), there is no Prometheus-compatible `/metrics` endpoint, no OTLP exporter, and no StatsD integration to ship metrics to external monitoring systems.

**Evidence:**
- `components/cdc_log/include/cdc_log.h` - Logging system outputs to USB CDC/UART but no metrics tracking
- `components/serial_cmd/src/SerialCmd.cpp:424-456` - STATUS/MEM commands provide one-shot data but no continuous metrics
- No metrics library imports (prometheus-cpp, esp_metrics, etc.) found in the codebase
- No `/metrics` route or exporter configuration in main.cpp

## Impact
Without structured metrics export:
- **No dashboards**: Cannot visualize system health trends (memory, uptime, operation counts)
- **No alerting**: Cannot set up alerts for low memory, high error rates, or battery issues
- **No capacity planning**: Cannot track long-term resource utilization patterns
- **Debug difficulty**: Hard to diagnose intermittent issues without historical data

## Evidence
Current telemetry options:
1. **Logging** (cdc_log): Text-based, not queryable, no time-series storage
2. **Serial commands** (STATUS, MEM): Manual polling, no automation support
3. **Error log**: Ring buffer of 50 entries, accessible via ERROR_LOG command but no export

Missing:
- No metrics counter for operations (FIDO2 authentications, TOTP generations, PIN attempts)
- No gauge for resource usage (heap, PSRAM, battery level)
- No histogram for operation latency
- No metrics endpoint or export configuration

## Recommended Fix
Implement a lightweight metrics system suitable for ESP32-S3:

1. **Add metrics header** (`components/cdc_metrics/include/cdc_metrics.h`):
   ```cpp
   // Simple counter/gauge API
   void metrics_increment(const char* name);
   void metrics_set_gauge(const char* name, uint32_t value);
   uint32_t metrics_get_gauge(const char* name);
   ```

2. **Register serial command to export metrics** (similar to STATUS):
   ```cpp
   reg.registerCommand({"METRICS", "Export current metrics", cmdMetrics, "system", false});
   ```

3. **Implement cmdMetrics()** in SerialCmd.cpp:
   - Output in Prometheus format: `metric_name value`
   - Include: heap_free, psram_free, uptime_ms, error_count, operations_by_type

4. **Wire metrics to key operations**:
   - FIDO2 authentications in `mod_fido2/src/Fido2Module.cpp`
   - TOTP generations in `mod_totp/src/TotpModule.cpp`
   - PIN attempts in `components/cdc_core/src/PinManager.cpp`
   - Error logging in `components/cdc_log/src/cdc_log.cpp`

## References
- [Prometheus exposition format](https://prometheus.io/docs/instrumenting/exposition_formats/)
- [ESP32-S3 memory specs](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
- Existing log infrastructure: `components/cdc_log/`
