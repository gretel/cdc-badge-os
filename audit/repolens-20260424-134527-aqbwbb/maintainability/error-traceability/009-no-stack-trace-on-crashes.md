---
title: "[MEDIUM] No stack trace capture for crash diagnostics"
severity: MEDIUM
domain: Error Traceability
lens: error-traceability
labels:
  - "audit:maintainability/error-traceability"
---

## Summary
The codebase lacks stack trace capture functionality for crash diagnostics. When the ESP32-S3 encounters a fatal error (panic, exception, reset), there is no mechanism to capture and preserve the call stack for post-mortem analysis.

## Impact
- **Crash debugging difficulty**: When the device crashes, there's no stack trace to identify the crash location
- **Production troubleshooting**: Hard to diagnose intermittent crashes without stack information
- **Lost context**: Reset reasons are known but not correlated with execution context
- **Manual reproduction required**: Developers must reproduce crashes to add temporary debugging

## Evidence

### No backtrace capture in error log:
`components/cdc_log/src/cdc_log.cpp`:
- Error log captures ERROR/WARN messages with timestamps
- No stack trace capture on error entry
- No integration with ESP-IDF backtrace functions

### No panic handler:
- No `esp_panic_handler` registration
- No custom exception handler for C++ exceptions
- No core dump capture on fatal errors

### Reset reason tracking missing:
`components/cdc_core/src/EspHardware.cpp` (if exists) or main initialization:
- No capture of `esp_reset_reason()` at startup
- No logging of previous reset cause
- No correlation between reset cause and error log

### ESP-IDF backtrace functions available but unused:
```cpp
// Available but not integrated:
void esp_backtrace_print(uint32_t limit);
esp_reset_reason_t esp_reset_reason(void);
```

### Current error handling:
Errors are logged via `LOG_E()` but when a critical error leads to crash/reset:
- Error message may be in the ring buffer
- No stack trace attached
- No memory dump
- No register state

## Recommended Fix

### Fix 1: Add stack trace capture to error log

Create `components/cdc_core/StackTrace.h`:

```cpp
/**
 * \brief Capture current stack trace
 * \param buffer Output buffer for trace
 * \param maxFrames Maximum frames to capture
 * \return Number of frames captured
 */
uint32_t stack_trace_capture(uint32_t* buffer, uint8_t maxFrames);

/**
 * \brief Format stack trace as string
 * \param buffer Trace buffer from capture
 * \param frames Number of frames
 * \param out Output string buffer
 * \param outSize Output buffer size
 */
void stack_trace_format(const uint32_t* buffer, uint8_t frames, 
                        char* out, size_t outSize);

/**
 * \brief Dump stack trace to console
 * \param buffer Trace buffer
 * \param frames Number of frames
 */
void stack_trace_dump(const uint32_t* buffer, uint8_t frames);
```

### Fix 2: Enhance error log with stack traces

Update `components/cdc_log/include/cdc_log.h`:

```cpp
typedef struct {
    uint32_t timestamp_ms;
    log_level_t level;
    char message[ERROR_LOG_LINE_LEN];
    uint32_t stack_trace[10];  // Optional stack trace
    uint8_t stack_frames;      // Number of valid frames
} error_log_entry_t;

/**
 * \brief Write error with stack trace capture
 * \param tag Log tag
 * \param fmt Format string
 * \param ... Format arguments
 */
#define LOG_E_ST(tag, fmt, ...) \
    log_write_stack(CDC_LOG_LEVEL_ERROR, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * \brief Write error with stack trace (for critical errors)
 */
void log_write_stack(log_level_t level, const char* tag, const char* file, int line,
                     const char* fmt, ...);
```

### Fix 3: Add reset reason logging

Update `main/main.cpp` or create `components/cdc_core/ResetTracker.h`:

```cpp
/**
 * \brief Log reset reason at startup
 */
void reset_tracker_log_reason(void);

/**
 * \brief Get previous reset reason
 * \return Reset reason string or "Unknown"
 */
const char* reset_tracker_get_reason(void);

/**
 * \brief Get previous reset timestamp
 * \return Timestamp in ms (0 if unknown)
 */
uint32_t reset_tracker_get_time(void);
```

Usage in `main()`:
```cpp
void app_main(void) {
    // Log reset reason first
    reset_tracker_log_reason();
    
    // Initialize other systems
    log_init();
    
    // If previous reset was panic, dump error log
    if (reset_tracker_was_panic()) {
        LOG_W("BOOT", "Previous boot crashed, dumping error log");
        error_log_dump();
    }
}
```

### Fix 4: Add panic handler

```cpp
#include "esp_panic.h"

/**
 * \brief Custom panic handler
 */
static void panic_handler(esp_panic_method_t method, 
                          const esp_panic_info_t* info) {
    // Log panic to error buffer (if space available)
    LOG_E("PANIC", "Panic at %s:%d in %s()", 
          info->file, info->line, info->func);
    
    // Dump stack trace
    uint32_t trace[10];
    uint8_t frames = stack_trace_capture(trace, 10);
    
    // Output to UART (guaranteed to work)
    printf("PANIC STACK TRACE:\r\n");
    stack_trace_dump(trace, frames);
    
    // Call default handler
    esp_panic_default_handler(method, info);
}

// Register in app_main()
esp_panic_set_handler(panic_handler);
```

### Fix 5: Add core dump capture (optional, for deep debugging)

```cpp
#include "esp_core_dump.h"

/**
 * \brief Capture core dump to NVS
 */
void core_dump_capture(void);

// Call before reset on critical errors
void handleCriticalError(const char* error) {
    LOG_E("CRIT", "%s", error);
    core_dump_capture();
    esp_restart();
}
```

## Implementation Priority

1. **High**: Reset reason logging (simple, high value)
2. **High**: Stack trace capture for critical errors
3. **Medium**: Panic handler integration
4. **Low**: Core dump capture (requires more storage)

## References
- [ESP-IDF Backtrace](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/stack_trace.html)
- [ESP-IDF Reset Reasons](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/utils.html#reset-reasons)
- [ESP-IDF Panic Handler](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/utils.html#panic-handler)
- [ESP-IDF Core Dump](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/core_dump.html)
