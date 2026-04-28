---
title: "[LOW] Error aggregation limited to simple ring buffer without analytics"
severity: LOW
domain: Error Traceability
lens: error-traceability
labels:
  - "audit:maintainability/error-traceability"
---

## Summary
The error log system (PSRAM-backed ring buffer) captures ERROR and WARN messages but lacks aggregation, trending, or analytics capabilities. There's no mechanism to summarize errors or identify patterns.

## Impact
- **Manual analysis required**: Each error must be reviewed individually
- **No pattern detection**: Cannot identify recurring errors or trends
- **Limited debugging**: Cannot quickly see error frequency or distribution
- **No alerting**: Critical errors may go unnoticed if not actively monitored

## Evidence

### Current error log implementation:

`components/cdc_log/include/cdc_log.h`:
```cpp
typedef struct {
    uint32_t timestamp_ms;
    log_level_t level;
    char message[ERROR_LOG_LINE_LEN];  // 100 chars max
} error_log_entry_t;

#define ERROR_LOG_MAX_ENTRIES 50  // Limited to 50 entries
```

`components/cdc_log/src/cdc_log.cpp`:
```cpp
/** \brief PSRAM-backed error/warn ring log storage (no heap allocation). */
EXT_RAM_BSS_ATTR static error_log_entry_t s_error_log[ERROR_LOG_MAX_ENTRIES];
static size_t s_error_log_head = 0;
static size_t s_error_log_count = 0;

void error_log_dump(void) {
    // Just prints all entries with no aggregation or summary
    console_printf("Error log (%zu entries):\r\n", s_error_log_count);
    // ... prints each entry
}
```

### Features missing:
- No error counting/grouping
- No frequency analysis
- No time-based aggregation (e.g., errors per minute)
- No severity summary
- No error trend detection
- No external export or monitoring integration

### Current usage:
- Error log is PSRAM-backed (good for memory efficiency)
- Captures ERROR and WARN level messages
- Limited to 50 entries (may miss older errors)
- Only accessible via serial dump

## Recommended Fix

1. **Add error aggregation statistics:**

   ```cpp
   typedef struct {
       uint16_t total_count;
       uint16_t by_level[2];  // WARN, ERROR counts
       uint16_t by_module[16]; // Per-module counts
       uint32_t first_error_ms;
       uint32_t last_error_ms;
       uint16_t most_common_idx; // Index of most frequent error
   } error_stats_t;
   
   error_stats_t error_log_get_stats(void);
   ```

2. **Add error frequency tracking:**

   ```cpp
   typedef struct {
       char message[ERROR_LOG_LINE_LEN];
       uint16_t count;
       uint32_t first_seen_ms;
       uint32_t last_seen_ms;
   } error_summary_t;
   
   error_summary_t* error_log_get_aggregated(size_t* count);
   ```

3. **Add export functionality:**

   ```cpp
   // Export to serial in structured format
   void error_log_dump_json(void);
   
   // Export as CSV for analysis
   void error_log_dump_csv(void);
   ```

4. **Add critical error alerting:**

   ```cpp
   // Callback for critical errors
   typedef void (*error_critical_hook_t)(const error_log_entry_t* entry);
   void error_log_register_critical_hook(error_critical_hook_t hook);
   ```

5. **Consider increasing capacity:**
   - Current: 50 entries
   - Recommended: 100-200 entries (still fits in PSRAM)

## References
- Current implementation: `components/cdc_log/src/cdc_log.cpp`
- PSRAM usage is appropriate for embedded context
