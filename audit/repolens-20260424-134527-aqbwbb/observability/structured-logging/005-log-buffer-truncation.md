---
title: "[LOW] Log message buffer size (256 bytes) may truncate long messages"
severity: LOW
domain: observability
lens: structured-logging
labels:
  - "audit:observability/structured-logging"
---

## Summary
The `log_write()` function uses a 256-byte buffer for formatted messages (`buf[256]`) and a 300-byte buffer for the final line (`line[300]`). While `vsnprintf()` prevents buffer overflows, messages longer than 256 characters will be silently truncated, potentially losing important context.

**Location:** `components/cdc_log/src/cdc_log.cpp` lines 161 and 168.

## Impact
1. **Data loss**: Long error messages (e.g., JSON payloads, stack traces, detailed diagnostics) may be truncated.
2. **Silent truncation**: No warning when truncation occurs, making it hard to detect.
3. **Debugging gaps**: Critical error context may be lost when it's needed most.

## Evidence
Current implementation (`components/cdc_log/src/cdc_log.cpp:156-182`):
```cpp
void log_write(log_level_t level, const char* tag, const char* fmt, ...) {
    // Format message
    char buf[256];  // Limited buffer
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);  // Truncates if > 256 chars
    va_end(args);

    // Format with prefix
    char line[300];  // Also limited
    snprintf(line, sizeof(line), "[%s][%s] %s", level_str[level], tag, buf);
    // ...
}
```

Example scenarios where truncation could occur:
- Logging JSON credential data: `LOG_D(TAG, "Credential: %s", json_string);`
- Logging long URLs: `LOG_I(TAG, "URL: %s", url);` (from `mod_gpg/src/openpgp/openpgp.cpp:1017`)
- Logging error details with stack traces
- Logging large binary data as hex strings

## Recommended Fix
Option 1: **Increase buffer sizes** (simplest):
```cpp
char buf[512];  // Double the size
char line[600];
```

Option 2: **Add truncation detection** (more robust):
```cpp
char buf[256];
va_list args;
va_start(args, fmt);
int len = vsnprintf(buf, sizeof(buf), fmt, args);
va_end(args);

// Check for truncation
if (len >= sizeof(buf)) {
    LOG_W("cdc_log", "Message truncated: expected %d bytes", len);
}
```

Option 3: **Use dynamic allocation for very long messages** (most complex):
- For messages > 256 bytes, allocate on heap (with PSRAM preference on ESP32-S3)
- Free after logging

**Recommendation:** Option 1 (increase to 512 bytes) is sufficient for most use cases and has minimal overhead on ESP32-S3 with PSRAM.

**Estimated effort:** ~15 minutes (simple buffer size change).

## References
- ESP32-S3 PSRAM: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/dynamic_memory.html
- vsnprintf truncation: https://man7.org/linux/man-pages/man3/printf.3.html

</content>