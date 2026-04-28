---
title: "[LOW] Error log ring buffer not exposed for audit retrieval"
severity: LOW
domain: observability
lens: audit-trail
labels:
  - "audit:observability/audit-trail"
---

## Summary

The `cdc_log` library has a PSRAM-backed error log ring buffer (`error_log_entry_t`) that stores ERROR and WARN messages, but there is no mechanism to retrieve these entries programmatically for audit analysis. The entries exist in memory but are only accessible via `error_log_dump()` which outputs to console.

**Files affected:**
- `components/cdc_log/include/cdc_log.h:36-53` - Error log structure and API
- `components/cdc_log/src/error_log.c` - Implementation (not read, but API exists)

## Impact

1. **Audit Retrieval**: Cannot programmatically retrieve error history for analysis.
2. **Forensic Export**: No way to export error logs for external analysis.
3. **Debugging**: Hard to correlate error patterns across sessions.

## Evidence

In `cdc_log.h:36-53`:
```cpp
#define ERROR_LOG_MAX_ENTRIES 50
#define ERROR_LOG_LINE_LEN    100

typedef struct {
    uint32_t timestamp_ms;
    log_level_t level;
    char message[ERROR_LOG_LINE_LEN];
} error_log_entry_t;

// Get error log entries (returns count, fills entries array)
size_t error_log_get_entries(error_log_entry_t* entries, size_t max_entries);
size_t error_log_get_count(void);
void error_log_clear(void);
void error_log_dump(void);  // Dump to console
```

The API exists (`error_log_get_entries()`) but:
- No serial command to retrieve entries
- No export format defined
- No integration with audit framework

## Recommended Fix

### Step 1: Add serial command for audit retrieval (15 min)

In `components/serial_cmd/src/CommandRegistry.cpp`:
```cpp
static void cmd_audit_dump(const char* args) {
    size_t count = error_log_get_count();
    cdc::serial::Console::printf("Audit entries: %zu\r\n", count);

    error_log_entry_t entries[ERROR_LOG_MAX_ENTRIES];
    size_t retrieved = error_log_get_entries(entries, ERROR_LOG_MAX_ENTRIES);

    for (size_t i = 0; i < retrieved; i++) {
        const char* level = "";
        switch (entries[i].level) {
            case CDC_LOG_LEVEL_ERROR: level = "ERROR"; break;
            case CDC_LOG_LEVEL_WARN: level = "WARN"; break;
            default: level = "INFO"; break;
        }
        cdc::serial::Console::printf("%lu [%s] %s\r\n",
                                    (unsigned long)entries[i].timestamp_ms,
                                    level, entries[i].message);
    }

    if (args && strstr(args, "--clear")) {
        error_log_clear();
        cdc::serial::Console::printf("Error log cleared\r\n");
    }
}

// Register command
registry.registerCommand({"AUDIT_DUMP", "Dump error log entries", cmd_audit_dump, "audit", false});
```

### Step 2: Add JSON export format (10 min)

```cpp
static void cmd_audit_dump(const char* args) {
    bool json = args && strstr(args, "--json");
    size_t count = error_log_get_count();

    if (json) {
        cdc::serial::Console::printf("{\"count\":%zu,\"entries\":[", count);
    } else {
        cdc::serial::Console::printf("Audit entries: %zu\r\n", count);
    }

    error_log_entry_t entries[ERROR_LOG_MAX_ENTRIES];
    size_t retrieved = error_log_get_entries(entries, ERROR_LOG_MAX_ENTRIES);

    for (size_t i = 0; i < retrieved; i++) {
        if (json) {
            if (i > 0) cdc::serial::Console::printf(",");
            cdc::serial::Console::printf("{\"ts\":%lu,\"msg\":\"%s\"}",
                                        (unsigned long)entries[i].timestamp_ms,
                                        entries[i].message);
        } else {
            cdc::serial::Console::printf("%lu: %s\r\n",
                                        (unsigned long)entries[i].timestamp_ms,
                                        entries[i].message);
        }
    }

    if (json) {
        cdc::serial::Console::printf("]}\r\n");
    }

    if (args && strstr(args, "--clear")) {
        error_log_clear();
        if (json) cdc::serial::Console::printf("{\"cleared\":true}\r\n");
        else cdc::serial::Console::printf("Error log cleared\r\n");
    }
}
```

## References

- NIST SP 800-92 - Guide to Computer Security Log Management (Section 4.2.3 Log retrieval)
- RFC 5424 - Syslog structured data format
