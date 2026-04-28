---
title: "[LOW] No alerting configuration for health events"
severity: LOW
domain: observability
lens: health-monitoring
labels:
  - "audit:observability/health-monitoring"
---

## Summary

The codebase has no alerting rules or notification configuration for critical health events:
- No alert definitions for hardware failures (secure element, power manager, display, etc.)
- No threshold-based alerts for memory pressure (heap below threshold)
- No alerting configuration files (Prometheus rules, Grafana alerts, etc.)

## Impact

- **No automated notification**: Operators are not alerted when the device enters a degraded state
- **Manual monitoring required**: Health must be checked manually via serial commands
- **Delayed incident response**: Issues might go unnoticed until users report problems

## Evidence

**Search results:**
- No files matching `*.rules.yml`, `alerting.yaml`, `prometheus/` in the repository
- No references to alerting frameworks (Prometheus, Grafana, PagerDuty) in source files
- Error log exists (`cdc_log.cpp`) but is not wired to any alerting system

**File:** `components/cdc_log/src/cdc_log.cpp` - PSRAM-backed error log (50 entries) exists but:
- No mechanism to trigger alerts when error count exceeds threshold
- No webhook/notification hook for critical errors
- Error log is only accessible via `ERROR_LOG` command

## Recommended Fix

Add alerting hooks for critical events:

1. **Define alert thresholds in a config file:**
   ```yaml
   # config/alerts.yaml
   heap_threshold: 10000  # Alert when free heap < 10KB
   error_log_threshold: 40  # Alert when error log > 80% full
   ```

2. **Add alert callback hooks:**
   ```cpp
   typedef void (*AlertCallback)(const char* severity, const char* message);
   void log_register_alert_callback(AlertCallback cb);
   ```

3. **Trigger alerts on critical conditions:**
   - Memory pressure (heap < threshold)
   - Secure element session lost
   - Power manager critical (battery < 10%)
   - Error log overflow

4. **Create an `ALERTS` command** to show recent alerts:
   ```
   ALERTS - Show recent alert history
   ALERTS_CLEAR - Clear alert history
   ```

## References

- Prometheus alerting rules: https://prometheus.io/docs/prometheus/latest/configuration/alerting_rules/
- ESP32 memory management: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/memory.html
