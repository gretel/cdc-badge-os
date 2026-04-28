---
title: "[LOW] Error log structure and buffer sizes exposed via public header"
severity: LOW
domain: cdc_log
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The error log configuration constants are defined in a public header file, exposing internal buffer sizes that could aid an attacker in understanding the logging system's capacity and structure.

**Exposed constants in `components/cdc_log/include/cdc_log.h:36-37`:**
```cpp
#define ERROR_LOG_MAX_ENTRIES 50
#define ERROR_LOG_LINE_LEN    100
```

These define the exact ring buffer size (50 entries) and maximum message length (100 bytes).

## Impact
- **Buffer overflow planning**: Attacker knows exact buffer sizes for potential overflow attacks
- **Log capacity**: Knowing 50 entries helps estimate how long logs persist before overwriting
- **Memory layout**: Structure size can be calculated (50 × ~110 bytes = ~5.5 KB PSRAM)
- **Internal architecture**: Reveals use of PSRAM-backed ring buffer

## Evidence
File: `components/cdc_log/include/cdc_log.h:36-42`
```cpp
// Error Log (WARNING and ERROR messages, PSRAM-backed stack)
#define ERROR_LOG_MAX_ENTRIES 50
#define ERROR_LOG_LINE_LEN    100

typedef struct {
    uint32_t timestamp_ms;
    log_level_t level;
    char message[ERROR_LOG_LINE_LEN];
} error_log_entry_t;
```

## Recommended Fix
1. Move buffer size constants to implementation file (`.cpp`) if they don't need to be public
2. Use a more generic macro name with module prefix (e.g., `CDC_LOG_ERROR_MAX_ENTRIES`)
3. Consider adding indirection - define implementation constants separately from public API

Example fix:
```cpp
// In .h file - only expose what's needed publicly
#define ERROR_LOG_LINE_LEN    100

// In .cpp file - implementation detail
static constexpr size_t ERROR_LOG_MAX_ENTRIES = 50;
static constexpr size_t ERROR_LOG_ENTRY_SIZE = ERROR_LOG_LINE_LEN + 15; // timestamp + level + message
```

## References
- Information disclosure via headers: https://owasp.org/www-project-web-security-testing-guide/latest/4-Web_Application_Security_Testing/01-Information_Gathering/03-Discovering_the_Technology_Used_by_a_Web_Application.html
- Ring buffer implementation: `components/cdc_log/src/cdc_log.cpp:35-57`
