---
title: "[LOW] Error Log Retention Limited to Fixed Ring Buffer"
severity: LOW
domain: compliance
lens: data-retention
labels:
  - "audit:compliance/data-retention"
---

## Summary
The error log implementation uses a fixed-size ring buffer (50 entries) stored in PSRAM. When full, oldest entries are overwritten. No persistent storage, rotation, or export mechanism exists.

**Location**: `components/cdc_log/src/cdc_log.cpp` (lines 35-60), `components/cdc_log/include/cdc_log.h` (lines 35-50)

## Impact
1. **Data Loss**: Important error history lost after 50 entries
2. **Volatile Storage**: PSRAM cleared on power cycle
3. **No Long-term Analysis**: Cannot track error patterns over time
4. **Debug Limitation**: Hard to diagnose intermittent issues

## Evidence
**Ring buffer definition** (`cdc_log.h:35-45`):
```cpp
#define ERROR_LOG_MAX_ENTRIES 50
#define ERROR_LOG_LINE_LEN    100

typedef struct {
    uint32_t timestamp_ms;
    log_level_t level;
    char message[ERROR_LOG_LINE_LEN];
} error_log_entry_t;
```

**PSRAM-backed storage** (`cdc_log.cpp:35-38`):
```cpp
EXT_RAM_BSS_ATTR static error_log_entry_t s_error_log[ERROR_LOG_MAX_ENTRIES];
static size_t s_error_log_head = 0;  // Next write position
static size_t s_error_log_count = 0; // Number of entries
```

**Ring buffer overwrite behavior** (`cdc_log.cpp:48-58`):
```cpp
// Write to current position (overwrites oldest if full)
error_log_entry_t* entry = &s_error_log[s_error_log_head];
// ... fill entry ...

// Advance head (ring buffer)
s_error_log_head = (s_error_log_head + 1) % ERROR_LOG_MAX_ENTRIES;
if (s_error_log_count < ERROR_LOG_MAX_ENTRIES) {
    s_error_log_count++;
}
```

**No persistence**: Log entries exist only in RAM, lost on reboot.

## Recommended Fix
Consider one of these enhancements:

**Option 1: NVS-backed persistent log**
Store error log entries in NVS with rotation:
```cpp
// Store last 100 errors in NVS namespace "error_log"
// Rotate when full, keep most recent
```

**Option 2: Configurable size**
Make buffer size configurable via compile-time flag:
```c
#ifndef ERROR_LOG_MAX_ENTRIES
#define ERROR_LOG_MAX_ENTRIES 50
#endif
```

**Option 3: Export command**
Add serial command to export log to serial for external storage:
```bash
ERROR_LOG EXPORT  # Dumps all entries for piping to file
```

**Option 4: Auto-clear on dump**
Clear log after manual dump (for fresh debugging session):
```bash
ERROR_LOG DUMP CLEAR  # Dumps and clears
```

## References
- ESP-IDF NVS API for persistent storage
- Syslog rotation conventions
