---
title: "[MEDIUM] Log macros lack lazy evaluation for expensive operations"
severity: MEDIUM
domain: observability
lens: structured-logging
labels:
  - "audit:observability/structured-logging"
---

## Summary
The cdc_log macros always format the log message (via `vsnprintf`) regardless of whether the current log level will actually display it. This causes unnecessary CPU cycles and memory usage, especially for DEBUG/VERBOSE logs with expensive string formatting or function calls.

**Location:** `components/cdc_log/include/cdc_log.h` lines 129-133 and `components/cdc_log/src/cdc_log.cpp` lines 156-181.

## Impact
1. **Performance overhead**: Every log call formats the string even when filtered out.
2. **CPU cycles wasted**: String formatting with `vsnprintf` is relatively expensive on embedded systems.
3. **Stack usage**: Variable argument processing uses stack space even for suppressed logs.
4. **No short-circuit**: Complex expressions in log arguments are always evaluated.

**Example scenario:**
```cpp
// Even if DEBUG level is disabled, this still:
// 1. Calls expensiveFunc()
// 2. Formats the string with vsnprintf
LOG_D(TAG, "Value: %d", expensiveFunc());
```

## Evidence
Current implementation (`components/cdc_log/src/cdc_log.cpp:156-181`):
```cpp
void log_write(log_level_t level, const char* tag, const char* fmt, ...) {
    // Always capture ERROR/WARN to error log
    bool capture = (level == CDC_LOG_LEVEL_ERROR || level == CDC_LOG_LEVEL_WARN);

    // Format message (happens BEFORE level check!)
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);  // Always formatted!
    va_end(args);

    // Format with prefix
    char line[300];
    snprintf(line, sizeof(line), "[%s][%s] %s",
             level_str[level], tag ? tag : "???", buf);

    // Capture to error log (before suppression check)
    if (capture) {
        error_log_add(level, line);
    }

    // Output if not suppressed (format already done!)
    if (level <= s_log_level) {
        console_printf("%s\n", line);
    }
}
```

The level check happens at line 178, but the expensive `vsnprintf` happens at line 162.

## Recommended Fix
Add lazy evaluation support using a two-stage approach:

**Option 1: Add level-check macros (simplest)**
```cpp
// In cdc_log.h
#define LOG_ENABLED(level) (level <= log_get_level())

#define LOG_E(tag, fmt, ...) do { \
    if (LOG_ENABLED(CDC_LOG_LEVEL_ERROR)) log_write(CDC_LOG_LEVEL_ERROR, tag, fmt, ##__VA_ARGS__); \
} while(0)

#define LOG_W(tag, fmt, ...) do { \
    if (LOG_ENABLED(CDC_LOG_LEVEL_WARN)) log_write(CDC_LOG_LEVEL_WARN, tag, fmt, ##__VA_ARGS__); \
} while(0)

#define LOG_I(tag, fmt, ...) do { \
    if (LOG_ENABLED(CDC_LOG_LEVEL_INFO)) log_write(CDC_LOG_LEVEL_INFO, tag, fmt, ##__VA_ARGS__); \
} while(0)

#define LOG_D(tag, fmt, ...) do { \
    if (LOG_ENABLED(CDC_LOG_LEVEL_DEBUG)) log_write(CDC_LOG_LEVEL_DEBUG, tag, fmt, ##__VA_ARGS__); \
} while(0)

#define LOG_V(tag, fmt, ...) do { \
    if (LOG_ENABLED(CDC_LOG_LEVEL_VERBOSE)) log_write(CDC_LOG_LEVEL_VERBOSE, tag, fmt, ##__VA_ARGS__); \
} while(0)
```

**Option 2: Add lazy log_write function (more flexible)**
```cpp
// In cdc_log.h
typedef void (*log_formatter_t)(char* buf, size_t buf_size);
void log_write_lazy(log_level_t level, const char* tag, log_formatter_t formatter);

// Usage:
LOG_D(TAG, "Value: %d", expensiveFunc());  // Old way, always formats

// New way with lazy evaluation:
LOG_D_LAZY(TAG, "Value: %d", {
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%d", expensiveFunc());
    strcpy(buf, tmp);
});
```

**Option 3: Pre-check in macro (balanced approach)**
```cpp
// In cdc_log.h
#define LOG_E(tag, fmt, ...) do { \
    if (CDC_LOG_LEVEL_ERROR <= s_log_level) { \
        va_list args; \
        va_start(args, fmt); \
        log_write_va(CDC_LOG_LEVEL_ERROR, tag, fmt, args); \
        va_end(args); \
    } \
} while(0)
```

**Recommendation:** Option 1 is simplest and provides good performance for most cases.

**Estimated effort:** ~30-45 minutes (add level-check macros + update existing code).

## References
- ESP-IDF logging: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/log.html
- Common practice: Many logging libraries (e.g., SLF4J, log4j2) use lazy evaluation for performance
- Related: Finding #003 (hardcoded log level) discusses log level configuration

</content>