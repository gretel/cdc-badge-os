---
title: "[LOW] Missing Log Sink Plugin Architecture"
severity: LOW
domain: architecture/extensibility
lens: logging-sinks
labels:
  - "audit:architecture/extensibility"
---

## Summary
The logging system (`cdc_log`) outputs to a single destination (USB serial console) with hardcoded formatting. Adding new log sinks (file system, BLE, circular buffer for post-mortem analysis) or changing log formats (JSON, structured logging) requires modifying the core logging code.

**Files affected:**
- `components/cdc_log/src/cdc_log.cpp` (lines 156-181) - hardcoded console output
- `components/cdc_log/include/cdc_log.h` - log macro definitions
- All modules using `LOG_I()`, `LOG_E()`, `LOG_W()`, `LOG_D()` macros

## Impact
- **Log sink extensibility**: Adding new destinations (e.g., BLE, SPIFFS, external logger) requires modifying `log_write()`
- **Format flexibility**: Cannot easily switch to JSON, structured, or timestamped formats without core changes
- **Testing difficulty**: Hard to capture logs for unit tests without serial output
- **Production debugging**: No way to add log buffering for crash analysis without modifying core
- **Bandwidth management**: Cannot filter or throttle logs per-sink (e.g., verbose locally, concise over BLE)

## Evidence

### Hardcoded Console Output in log_write
`components/cdc_log/src/cdc_log.cpp:156-181`:
```cpp
void log_write(log_level_t level, const char* tag, const char* fmt, ...) {
    // Always capture ERROR/WARN to error log
    bool capture = (level == CDC_LOG_LEVEL_ERROR || level == CDC_LOG_LEVEL_WARN);

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

    // Capture to error log (before suppression check)
    if (capture) {
        error_log_add(level, line);
    }

    // Output if not suppressed
    if (level <= s_log_level) {
        console_printf("%s\n", line);  // Hardcoded to serial!
    }
}
```

### No Log Sink Interface
There is no interface like:
```cpp
// Missing - should exist:
class ILogSink {
public:
    virtual ~ILogSink() = default;
    virtual void write(log_level_t level, const char* tag, const char* message) = 0;
    virtual void flush() = 0;
};

class LogRegistry {
public:
    static LogRegistry& instance();
    void registerSink(ILogSink* sink);
    void unregisterSink(ILogSink* sink);
    void write(log_level_t level, const char* tag, const char* message);
};
```

### Hardcoded Log Format
The format `[%s][%s] %s` is hardcoded in `snprintf(line, sizeof(line), ...)`. Changing to JSON or structured format would require:
- Modifying `log_write()` function
- Updating all callers if format changes significantly
- No runtime format switching

### Single Output Path
All log output goes through `console_printf()` which writes to USB serial. To add additional sinks:
```cpp
// Current: single path
if (level <= s_log_level) {
    console_printf("%s\n", line);
}

// Should be: multiple sinks
LogRegistry::instance().write(level, tag, buf);
```

## Recommended Fix

### Create Log Sink Interface
```cpp
// components/cdc_log/include/cdc_log_sink.h
#pragma once
#include "cdc_log.h"
#include <cstdint>

namespace cdc {

class ILogSink {
public:
    virtual ~ILogSink() = default;
    
    /**
     * Write a log entry
     * @param level Log level
     * @param tag Log tag
     * @param message Formatted message (without prefix)
     */
    virtual void write(log_level_t level, const char* tag, const char* message) = 0;
    
    /**
     * Flush any buffered output
     */
    virtual void flush() {}
    
    /**
     * Check if sink is enabled
     */
    virtual bool isEnabled() const { return true; }
};

/**
 * Log format types
 */
enum class LogFormat {
    TEXT,     // Default: [LEVEL][TAG] message
    JSON,     // JSON object per line
    SYSLOG,   // Syslog format
    CUSTOM    // For custom formatters
};

/**
 * Log registry for multiple sinks
 */
class LogRegistry {
public:
    static LogRegistry& instance();
    
    /**
     * Register a new log sink
     * @param sink Sink instance (must remain valid)
     */
    void registerSink(ILogSink* sink);
    
    /**
     * Unregister a log sink
     * @param sink Sink to unregister
     */
    void unregisterSink(ILogSink* sink);
    
    /**
     * Write log to all registered sinks
     * @param level Log level
     * @param tag Log tag
     * @param message Formatted message
     */
    void write(log_level_t level, const char* tag, const char* message);
    
    /**
     * Set global log level
     */
    void setLevel(log_level_t level);
    log_level_t getLevel() const;
    
    /**
     * Set log format
     */
    void setFormat(LogFormat format);
    LogFormat getFormat() const;
};

} // namespace cdc
```

### Implement Built-in Sinks
```cpp
// components/cdc_log/src/SerialSink.cpp
namespace cdc {

class SerialSink : public ILogSink {
public:
    void write(log_level_t level, const char* tag, const char* message) override {
        char line[300];
        snprintf(line, sizeof(line), "[%s][%s] %s",
                 level_str[level], tag ? tag : "???", message);
        console_printf("%s\n", line);
    }
};

} // namespace cdc
```

```cpp
// components/cdc_log/src/ErrorLogSink.cpp
namespace cdc {

class ErrorLogSink : public ILogSink {
public:
    void write(log_level_t level, const char* tag, const char* message) override {
        if (level == CDC_LOG_LEVEL_ERROR || level == CDC_LOG_LEVEL_WARN) {
            error_log_add(level, message);
        }
    }
};

} // namespace cdc
```

```cpp
// components/mod_log_spiiff/src/SpiffsSink.cpp
namespace cdc {

class SpiffsSink : public ILogSink {
public:
    void write(log_level_t level, const char* tag, const char* message) override {
        // Append to SPIFFS log file
        static char path[] = "/logs.txt";
        FILE* f = fopen(path, "a");
        if (f) {
            fprintf(f, "%s\n", message);
            fclose(f);
        }
    }
    
    void flush() override {
        // Sync filesystem
        sync();
    }
};

} // namespace cdc
```

```cpp
// components/mod_log_ble/src/BleSink.cpp
namespace cdc {

class BleSink : public ILogSink {
public:
    void write(log_level_t level, const char* tag, const char* message) override {
        // Send via BLE characteristic (throttled)
        static uint32_t lastSend = 0;
        uint32_t now = esp_log_timestamp();
        if (now - lastSend > 100) {  // Max 10 logs/sec
            ble_log_notify(level, tag, message);
            lastSend = now;
        }
    }
    
    bool isEnabled() const override {
        return ble_is_connected();
    }
};

} // namespace cdc
```

### Implement Formatters
```cpp
// components/cdc_log/src/LogFormatter.cpp
namespace cdc {

class ILogFormatter {
public:
    virtual ~ILogFormatter() = default;
    virtual void format(char* buf, size_t len, log_level_t level, 
                       const char* tag, const char* message) = 0;
};

class TextFormatter : public ILogFormatter {
    void format(char* buf, size_t len, log_level_t level, 
               const char* tag, const char* message) override {
        snprintf(buf, len, "[%s][%s] %s",
                 level_str[level], tag ? tag : "???", message);
    }
};

class JsonFormatter : public ILogFormatter {
    void format(char* buf, size_t len, log_level_t level, 
               const char* tag, const char* message) override {
        snprintf(buf, len, "{\"level\":\"%s\",\"tag\":\"%s\",\"msg\":\"%s\"}",
                 level_str[level], tag ? tag : "", message);
    }
};

} // namespace cdc
```

### Refactor log_write to Use Registry
```cpp
// components/cdc_log/src/cdc_log.cpp
#include "cdc_log_sink.h"

void log_write(log_level_t level, const char* tag, const char* fmt, ...) {
    // Format message
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    // Use registry to write to all sinks
    auto& registry = cdc::LogRegistry::instance();
    registry.write(level, tag, buf);
}
```

### Initialize Sinks at Startup
```cpp
// main/main.cpp
#include "cdc_log_sink.h"

void app_main() {
    auto& registry = cdc::LogRegistry::instance();
    
    // Register built-in sinks
    static cdc::SerialSink serialSink;
    registry.registerSink(&serialSink);
    
    static cdc::ErrorLogSink errorSink;
    registry.registerSink(&errorSink);
    
    // Optional: Register additional sinks
    #if CONFIG_LOG_SPIFFS_ENABLED
    static cdc::SpiffsNode* spiffsSink;
    registry.registerSink(spiffsSink);
    #endif
    
    #if CONFIG_LOG_BLE_ENABLED
    static cdc::BleSink bleSink;
    registry.registerSink(&bleSink);
    #endif
    
    // Set format
    #if CONFIG_LOG_FORMAT_JSON
    registry.setFormat(cdc::LogFormat::JSON);
    #endif
}
```

## References
- Observer Pattern for log sinks: https://refactoring.guru/design-patterns/observer
- Chain of Responsibility for log filtering: https://refactoring.guru/design-patterns/chain-of-responsibility
- Fluent Interface for log building: https://refactoring.guru/design-patterns/builder
- Structured logging best practices: https://github.com/observability/structured-logging-guide

</content>