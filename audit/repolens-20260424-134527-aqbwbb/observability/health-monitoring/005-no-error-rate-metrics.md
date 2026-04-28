---
title: "[LOW] No error rate metrics or error log integration"
severity: LOW
domain: observability
lens: health-monitoring
labels:
  - "audit:observability/health-monitoring"
---

## Summary

The error logging system (`components/cdc_log`) maintains a PSRAM-backed ring buffer of 50 error/warn entries, but:
- No command to get error counts/rates (e.g., "errors in last 5 minutes")
- No integration with the `STATUS` command to show recent error activity
- No mechanism to export error metrics for external monitoring

## Impact

- **No error trend analysis**: Cannot determine if errors are increasing/decreasing over time
- **No alerting on error spikes**: Sudden error rate increases go unnoticed
- **Manual troubleshooting**: Operators must manually inspect error log for patterns

## Evidence

**File:** `components/cdc_log/include/cdc_log.h` - Error log API:
```cpp
size_t error_log_get_entries(error_log_entry_t* entries, size_t max_entries);
size_t error_log_get_count(void);
void error_log_clear(void);
void error_log_dump(void);  // Dumps ALL entries to console
```

**File:** `components/serial_cmd/src/SerialCmd.cpp:473-480` - ERROR_LOG command:
```cpp
static void cmdErrorLog(const char* args) {
    if (args && strcmp(args, "CLEAR") == 0) {
        error_log_clear();
        Console::printf("Error log cleared.\r\n");
    } else {
        error_log_dump();  // Dumps all entries, no filtering
    }
}
```

**Missing features:**
- Time-based filtering (e.g., `ERROR_LOG LAST 5M`)
- Error count by severity
- Error rate calculation
- Integration with STATUS command

## Recommended Fix

Enhance error log functionality:

1. **Add time-based filtering to ERROR_LOG:**
   ```
   ERROR_LOG         - Show all entries
   ERROR_LOG LAST 5  - Show last 5 entries
   ERROR_LOG LAST 5M - Show entries from last 5 minutes
   ERROR_LOG COUNT   - Show error count by severity
   ```

2. **Add error rate to STATUS command:**
   ```cpp
   Console::printf("Errors (last 5 min): %d\r\n", error_log_get_count_last(5 * 60 * 1000));
   ```

3. **Add error rate helper:**
   ```cpp
   size_t error_log_get_count_last(uint32_t ms);  // Count entries from last N ms
   ```

## References

- Error budget concepts: https://sre.google/sre-book/setting-targets/
- ESP32 PSRAM: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/memory.html
