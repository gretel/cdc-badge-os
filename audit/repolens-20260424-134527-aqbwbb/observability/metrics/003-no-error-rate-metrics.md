---
title: "[MEDIUM] No error rate metrics for SLO calculation"
severity: MEDIUM
domain: observability/metrics
lens: error-tracking
labels:
  - "metrics"
  - "observability"
  - "errors"
---

## Summary
Errors are logged via `LOG_E()` and `LOG_W()` macros but not counted or categorized. There is no way to compute error rates for SLO calculations or detect degradation trends. Errors exist in a ring buffer (50 entries) but are not exposed as metrics.

**Evidence:**
- `components/cdc_log/src/cdc_log.cpp:150-180` - `log_write()` captures ERROR/WARN to ring buffer but no counter increment
- `components/cdc_log/include/cdc_log.h:37-59` - Error log is PSRAM-backed ring buffer, accessible via `error_log_dump()` but no export as metric
- 368 occurrences of `LOG_E`/`LOG_W` found across codebase, none incrementing error counters
- No error categorization by type (timeout, auth failure, hardware error, etc.)

## Impact
Without error rate metrics:
- **No SLO tracking**: Cannot calculate error rate ratio (errors / total operations)
- **No trend detection**: Cannot detect if errors are increasing over time
- **Debug difficulty**: Hard to correlate error spikes with deployments or events
- **Alerting gap**: Cannot set up alerts for error rate thresholds

## Evidence
**Error logging flow** (`components/cdc_log/src/cdc_log.cpp`):
```cpp
void log_write(log_level_t level, const char* tag, const char* fmt, ...) {
    // Always capture ERROR/WARN to error log
    bool capture = (level == CDC_LOG_LEVEL_ERROR || level == CDC_LOG_LEVEL_WARN);
    
    // Format message
    char buf[256];
    // ... formatting ...
    
    // Capture to error log (before suppression check)
    if (capture) {
        error_log_add(level, line);  // Just stores in ring buffer
    }
    
    // Output if not suppressed
    if (level <= s_log_level) {
        console_printf("%s\n", line);
    }
}
```

**Error log access** (serial command):
- `cmdErrorLog()` at line 470-477 dumps buffer to console
- No mechanism to export as metric or track rate

**Error types in codebase** (examples):
- `components/mod_fido2/src/Fido2Module.cpp:184` - FIDO2 init failure
- `components/mod_totp/src/TotpModule.cpp:933` - Slot range missing
- `components/serial_cmd/src/SerialCmd.cpp:1379` - Authentication failures logged but not counted

## Recommended Fix
1. **Add error counter to metrics system**:
   ```cpp
   // components/cdc_metrics/include/cdc_metrics.h
   void metrics_error(const char* category);  // e.g., "fido2", "totp", "pin", "se"
   ```

2. **Modify log_write() to increment counter** (`components/cdc_log/src/cdc_log.cpp:146`):
   ```cpp
   void log_write(log_level_t level, const char* tag, const char* fmt, ...) {
       bool capture = (level == CDC_LOG_LEVEL_ERROR || level == CDC_LOG_LEVEL_WARN);
       
       // ... formatting ...
       
       if (capture) {
           error_log_add(level, line);
           // NEW: Increment error counter for metrics
           if (level == CDC_LOG_LEVEL_ERROR) {
               metrics_error(tag ? tag : "unknown");
           }
       }
       
       if (level <= s_log_level) {
           console_printf("%s\n", line);
       }
   }
   ```

3. **Add metrics command to show error rates**:
   ```cpp
   // In SerialCmd.cpp
   reg.registerCommand({"ERRORS", "Show error counts by category", cmdErrors, "system", false});
   ```

4. **Export error counters** in METRICS command:
   ```
   errors_total{category="fido2"} 3
   errors_total{category="totp"} 1
   errors_total{category="pin"} 5
   ```

## References
- Error log implementation: `components/cdc_log/src/cdc_log.cpp`
- SLO basics: [Google SRE book on error budgets](https://sre.google/sre-book/service-level-objectives/)
