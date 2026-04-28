---
title: "[MEDIUM] Error log stores tag embedded in message instead of as separate field"
severity: MEDIUM
domain: observability
lens: structured-logging
labels:
  - "audit:observability/structured-logging"
---

## Summary
The error log ring buffer stores the tag as part of the formatted message string rather than as a separate structured field. This makes it difficult to filter, aggregate, or search logs by tag in a log analysis system.

**Location:** `components/cdc_log/src/cdc_log.cpp` lines 168-172 and `components/cdc_log/include/cdc_log.h` lines 39-43.

## Impact
1. **Limited query capability**: Cannot easily filter errors by tag (e.g., "show all errors from GpgStorage") without string parsing.
2. **Aggregation difficulty**: Cannot group errors by tag for metrics dashboards.
3. **Inefficient storage**: Tag is duplicated in every message entry instead of being a single enum field.
4. **Parsing overhead**: Log aggregation systems must parse the message string to extract the tag.

## Evidence
Current error log entry structure (`components/cdc_log/include/cdc_log.h:39-43`):
```cpp
typedef struct {
    uint32_t timestamp_ms;
    log_level_t level;
    char message[ERROR_LOG_LINE_LEN];  // Tag is embedded here!
} error_log_entry_t;
```

Message formatting (`components/cdc_log/src/cdc_log.cpp:168-172`):
```cpp
// Tag is formatted into the message string
snprintf(line, sizeof(line), "[%s][%s] %s",
         level_str[level], tag ? tag : "???", buf);

// Then stored with tag embedded
error_log_add(level, line);
```

Example stored entry:
```
message = "[E][GpgStorage] Failed to write slot 5"
// Tag "GpgStorage" is inside the message, not a separate field
```

## Recommended Fix
Add a separate `tag` field to the error log entry structure:

1. **Update `error_log_entry_t` in `components/cdc_log/include/cdc_log.h`**:
   ```cpp
   #define ERROR_LOG_TAG_LEN 32  // Enough for most module names
   
   typedef struct {
       uint32_t timestamp_ms;
       log_level_t level;
       char tag[ERROR_LOG_TAG_LEN];      // NEW: Separate tag field
       char message[ERROR_LOG_LINE_LEN]; // Just the message content
   } error_log_entry_t;
   ```

2. **Update `error_log_add()` in `components/cdc_log/src/cdc_log.cpp`**:
   ```cpp
   static void error_log_add(log_level_t level, const char* tag, const char* message) {
       if (level != CDC_LOG_LEVEL_ERROR && level != CDC_LOG_LEVEL_WARN) return;
       if (!message) return;
   
       error_log_entry_t* entry = &s_error_log[s_error_log_head];
       entry->timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
       entry->level = level;
       strncpy(entry->tag, tag ? tag : "UNKNOWN", ERROR_LOG_TAG_LEN - 1);
       entry->tag[ERROR_LOG_TAG_LEN - 1] = '\0';
       strncpy(entry->message, message, ERROR_LOG_LINE_LEN - 1);
       entry->message[ERROR_LOG_LINE_LEN - 1] = '\0';
   
       s_error_log_head = (s_error_log_head + 1) % ERROR_LOG_MAX_ENTRIES;
       if (s_error_log_count < ERROR_LOG_MAX_ENTRIES) {
           s_error_log_count++;
       }
   }
   ```

3. **Update call site in `log_write()`**:
   ```cpp
   // Before:
   error_log_add(level, line);
   
   // After:
   error_log_add(level, tag, buf);  // Pass tag and message separately
   ```

4. **Update `error_log_dump()` output format** (optional, for better readability):
   ```cpp
   console_printf("[%02lu:%02lu:%02lu][%s][%s] %s\r\n",
                  hours % 24, mins % 60, secs % 60,
                  e->level == CDC_LOG_LEVEL_ERROR ? "E" : "W",
                  e->tag,  // Tag as separate field
                  e->message);
   ```

**Memory impact**: Adds ~32 bytes per entry × 50 entries = ~1.6 KB in PSRAM (acceptable).

**Estimated effort:** ~45-60 minutes (structure change + update all call sites + test).

## References
- Error log structure: `components/cdc_log/include/cdc_log.h`
- Error log implementation: `components/cdc_log/src/cdc_log.cpp`
- Related: Finding #005 (log buffer truncation) discusses similar storage considerations

</content>