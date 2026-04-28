---
title: "[MEDIUM] Log messages lack file location context for error tracing"
severity: MEDIUM
domain: Error Traceability
lens: error-traceability
labels:
  - "audit:maintainability/error-traceability"
---

## Summary
Log messages (especially errors) do not include source file location information (`__FILE__`, `__LINE__`, `__func__`). When errors occur, there is no way to trace them back to the exact source location without manually searching through the codebase.

## Impact
- **Debugging inefficiency**: When an error message appears, developers must search the codebase to find where it was logged
- **Slow incident response**: During production issues, identifying the source of errors takes longer
- **No automatic stack traces**: Errors cannot be automatically correlated with code locations
- **Maintenance burden**: Refactoring becomes harder as error locations are not self-documenting

## Evidence

### Current logging pattern (no location info):

`components/cdc_log/include/cdc_log.h`:
```cpp
#define LOG_E(tag, fmt, ...) log_write(CDC_LOG_LEVEL_ERROR,   tag, fmt, ##__VA_ARGS__)
#define LOG_W(tag, fmt, ...) log_write(CDC_LOG_LEVEL_WARN,    tag, fmt, ##__VA_ARGS__)
```

### Example error log without location context:

`components/mod_fido2/src/fido2_storage.cpp:316`:
```cpp
LOG_E("FIDO2", "ECDSA sign failed for slot %d", logical_slot);
```

When this appears in logs:
```
[E][FIDO2] ECDSA sign failed for slot 5
```

There is **no indication** of:
- Which file (`fido2_storage.cpp`)
- Which function
- Which line number (316)

### Comparison with ESP-IDF pattern:

ESP-IDF's `ESP_LOGE` macro includes location info:
```cpp
#define ESP_LOGE(tag, format, ...) \
    esp_log_level_write(ESP_LOG_ERROR, tag, format, ##__VA_ARGS__, __FILE__, __LINE__, __func__)
```

### Current cdc_log structure:

`components/cdc_log/src/cdc_log.cpp:168-175`:
```cpp
void log_write(log_level_t level, const char* tag, const char* fmt, ...) {
    // Format message
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    // Format with prefix
    char line[300];
    snprintf(line, sizeof(line), "[%s][%s] %s",
             level_str[level], tag ? tag : "???", buf);
    // ...
}
```

The format string `[%s][%s] %s` only includes level, tag, and message - no file/line/function.

## Recommended Fix

### Option 1: Add location macros (recommended)

Update `components/cdc_log/include/cdc_log.h`:

```cpp
// Core logging function with location info
void log_write_loc(log_level_t level, const char* tag, const char* file, int line, 
                   const char* func, const char* fmt, ...);

// Convenience macros with automatic location capture
#define LOG_E(tag, fmt, ...) \
    log_write_loc(CDC_LOG_LEVEL_ERROR, tag, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define LOG_W(tag, fmt, ...) \
    log_write_loc(CDC_LOG_LEVEL_WARN, tag, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define LOG_I(tag, fmt, ...) \
    log_write_loc(CDC_LOG_LEVEL_INFO, tag, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define LOG_D(tag, fmt, ...) \
    log_write_loc(CDC_LOG_LEVEL_DEBUG, tag, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
```

Update `log_write_loc` implementation:

```cpp
void log_write_loc(log_level_t level, const char* tag, const char* file, int line,
                   const char* func, const char* fmt, ...) {
    // Format message
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    // Extract filename from path (basename)
    const char* basename = file;
    for (const char* p = file; *p; p++) {
        if (*p == '/' || *p == '\\') basename = p + 1;
    }

    // Format with location: [E][TAG][file:line:func] message
    char line[350];
    snprintf(line, sizeof(line), "[%s][%s][%s:%d:%s] %s",
             level_str[level], tag ? tag : "???",
             basename, line, func ? func : "?", buf);

    // Output
    console_printf("%s\n", line);
}
```

### Option 2: Conditional location logging (for memory-constrained builds)

```cpp
#ifdef LOG_INCLUDE_LOCATION
#define LOG_E(tag, fmt, ...) \
    log_write_loc(CDC_LOG_LEVEL_ERROR, tag, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#else
#define LOG_E(tag, fmt, ...) \
    log_write(CDC_LOG_LEVEL_ERROR, tag, fmt, ##__VA_ARGS__)
#endif
```

### Option 3: Add debug-only verbose logging

```cpp
#define LOG_E_V(tag, fmt, ...) \
    do { \
        char loc[60]; \
        snprintf(loc, sizeof(loc), "[%s:%d]", __FILE__, __LINE__); \
        LOG_E(tag, "%s " fmt, loc, ##__VA_ARGS__); \
    } while(0)
```

## References
- ESP-IDF logging: [esp_log.h](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/utils.html#logging)
- C preprocessor macros: `__FILE__`, `__LINE__`, `__func__`
- Existing cdc_log: `components/cdc_log/include/cdc_log.h`, `components/cdc_log/src/cdc_log.cpp`

</content>