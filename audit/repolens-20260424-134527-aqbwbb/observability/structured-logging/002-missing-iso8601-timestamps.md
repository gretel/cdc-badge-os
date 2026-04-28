---
title: "[MEDIUM] Log output lacks ISO 8601 timestamps for better log aggregation"
severity: MEDIUM
domain: observability
lens: structured-logging
labels:
  - "audit:observability/structured-logging"
---

## Summary
The cdc_log library outputs timestamps in `HH:MM:SS` format without date information. For log aggregation systems (e.g., Grafana Loki, ELK, CloudWatch), this makes it difficult to correlate logs across days or sort logs accurately when multiple services write logs around midnight.

**Location:** `components/cdc_log/src/cdc_log.cpp` lines 117-119 (error log dump), and the main log format uses only level/tag without ISO 8601 timestamps.

## Impact
1. **Log correlation difficulty**: Cannot correlate events across day boundaries without external context.
2. **Log aggregation challenges**: Many log aggregation systems expect ISO 8601 timestamps for proper indexing and time-based queries.
3. **Debugging complexity**: When debugging issues that span multiple days, timestamp ambiguity can cause confusion.
4. **Limited time-zone awareness**: No timezone information in log output.

## Evidence
Current log format (from `components/cdc_log/src/cdc_log.cpp:179`):
```cpp
console_printf("%s\n", line);  // line = "[E][TAG] message"
```

Error log format (from `components/cdc_log/src/cdc_log.cpp:117-119`):
```cpp
console_printf("[%02lu:%02lu:%02lu][%s] %s\r\n",
               hours % 24, mins % 60, secs % 60,
               e->level == CDC_LOG_LEVEL_ERROR ? "E" : "W",
               e->message);
```

Output example:
```
[14:30:45][E] Failed to connect to BLE device
[14:30:46][I] Retrying connection...
```

Missing: Date, timezone, milliseconds/microseconds for high-frequency logging.

## Recommended Fix
Update the log format to include ISO 8601 timestamps with date and time:

1. **Modify `log_write()` in `components/cdc_log/src/cdc_log.cpp`**:
   - Include `<time.h>` and `<esp_timer.h>`
   - Get current time using `esp_timer_get_time()` or `time(NULL)`
   - Format as ISO 8601: `YYYY-MM-DDTHH:MM:SS.mmm`

2. **Update log format string** from:
   ```cpp
   snprintf(line, sizeof(line), "[%s][%s] %s", level_str[level], tag, buf);
   ```
   To:
   ```cpp
   snprintf(line, sizeof(line), "[%s][%s][%s] %s", level_str[level], tag, timestamp, buf);
   ```

3. **Example implementation**:
   ```cpp
   // Get timestamp
   int64_t now_us = esp_timer_get_time();
   time_t now = now_us / 1000000;
   struct tm tm;
   gmtime_r(&now, &tm);
   char timestamp[24];
   snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec, (int)(now_us % 1000000) / 1000);
   ```

**Estimated effort:** ~45-60 minutes (implementation + testing).

## References
- ISO 8601 standard: https://en.wikipedia.org/wiki/ISO_8601
- ESP-IDF time functions: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/time.html
- ESP timer: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/esp_timer.html

</content>