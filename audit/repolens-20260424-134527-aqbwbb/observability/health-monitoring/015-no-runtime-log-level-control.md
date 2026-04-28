---
title: "[LOW] No runtime log level control command"
severity: LOW
domain: observability
lens: health-monitoring
labels:
  - "audit:observability/health-monitoring"
---

## Summary

The logging system supports runtime log level control via `log_set_level()` and `log_get_level()` functions (`components/cdc_log/include/cdc_log.h:61-62`), but **no serial command exists to query or change the log level**.

**What exists:**
- Log levels defined: NONE, ERROR, WARN, INFO, DEBUG, VERBOSE
- API functions: `log_set_level()`, `log_get_level()`
- Log level affects output verbosity (messages below threshold are suppressed)

**What's missing:**
- No `LOG_LEVEL` command to show current level
- No `LOG_SET` command to change level at runtime
- Developers must recompile or add temporary code to adjust verbosity

## Impact

1. **Debugging difficulty**: Cannot increase verbosity on-demand to trace issues
2. **Production tuning**: Cannot reduce log noise without rebuilding firmware
3. **Memory/bandwidth optimization**: Cannot dynamically throttle logs for BLE vs USB
4. **Convenience**: Requires knowing internal function calls to change log level

## Evidence

**Log level API** (`components/cdc_log/include/cdc_log.h:61-62`):
```cpp
// Set the log level (messages below this level are suppressed)
void log_set_level(log_level_t level);
log_level_t log_get_level(void);
```

**Log level implementation** (`components/cdc_log/src/cdc_log.cpp:17,136-142`):
```cpp
static log_level_t s_log_level = CDC_LOG_LEVEL_DEBUG;

void log_set_level(log_level_t level) {
    s_log_level = level;
}

log_level_t log_get_level(void) {
    return s_log_level;
}
```

**Search results**:
```bash
grep -r "LOG_LEVEL\|log_level" components/serial_cmd/src/SerialCmd.cpp
# No results - no commands registered for log level
```

**Available log levels** (from `cdc_log.h:24-31`):
- `CDC_LOG_LEVEL_NONE` - No output
- `CDC_LOG_LEVEL_ERROR` - Errors only
- `CDC_LOG_LEVEL_WARN` - Warnings and errors
- `CDC_LOG_LEVEL_INFO` - Info, warnings, errors
- `CDC_LOG_LEVEL_DEBUG` - Debug, info, warnings, errors
- `CDC_LOG_LEVEL_VERBOSE` - All messages

## Recommended Fix

Add log level commands to the serial interface:

**Step 1: Add helper function** (`components/serial_cmd/src/SerialCmd.cpp`):
```cpp
static const char* logLevelToStr(log_level_t level) {
    switch (level) {
        case CDC_LOG_LEVEL_NONE:     return "NONE";
        case CDC_LOG_LEVEL_ERROR:    return "ERROR";
        case CDC_LOG_LEVEL_WARN:     return "WARN";
        case CDC_LOG_LEVEL_INFO:     return "INFO";
        case CDC_LOG_LEVEL_DEBUG:    return "DEBUG";
        case CDC_LOG_LEVEL_VERBOSE:  return "VERBOSE";
        default:                     return "UNKNOWN";
    }
}

static log_level_t strToLogLevel(const char* str) {
    if (strcasecmp(str, "NONE") == 0)     return CDC_LOG_LEVEL_NONE;
    if (strcasecmp(str, "ERROR") == 0)    return CDC_LOG_LEVEL_ERROR;
    if (strcasecmp(str, "WARN") == 0)     return CDC_LOG_LEVEL_WARN;
    if (strcasecmp(str, "INFO") == 0)     return CDC_LOG_LEVEL_INFO;
    if (strcasecmp(str, "DEBUG") == 0)    return CDC_LOG_LEVEL_DEBUG;
    if (strcasecmp(str, "VERBOSE") == 0)  return CDC_LOG_LEVEL_VERBOSE;
    return CDC_LOG_LEVEL_DEBUG;  // Default
}
```

**Step 2: Add GET command**:
```cpp
static void cmdLogLevel(const char* args) {
    (void)args;
    log_level_t level = log_get_level();
    Console::printf("Log level: %s\r\n", logLevelToStr(level));
}
```

**Step 3: Add SET command**:
```cpp
static void cmdLogSet(const char* args) {
    if (!args || !*args) {
        Console::printf("Usage: LOG_SET <level>\r\n");
        Console::printf("Levels: NONE, ERROR, WARN, INFO, DEBUG, VERBOSE\r\n");
        return;
    }
    
    log_level_t level = strToLogLevel(args);
    log_set_level(level);
    Console::printf("OK: Log level set to %s\r\n", logLevelToStr(level));
}
```

**Step 4: Register commands** (`components/serial_cmd/src/SerialCmd.cpp:registerBuiltinCommands`):
```cpp
// Logging commands
reg.registerCommand({"LOG_LEVEL", "Show current log level", cmdLogLevel, "log", false});
reg.registerCommand({"LOG_SET", "Set log level (LOG_SET DEBUG)", cmdLogSet, "log", false});
```

**Expected output**:
```
$ LOG_LEVEL
Log level: DEBUG

$ LOG_SET INFO
OK: Log level set to INFO

$ LOG_LEVEL
Log level: INFO
```

**Optional enhancement**: Add to STATUS command:
```cpp
Console::printf("Log level: %s\r\n", logLevelToStr(log_get_level()));
```

## References

- ESP-IDF logging: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/log.html
- Log level best practices: https://www.12factor.net/logs

</content>