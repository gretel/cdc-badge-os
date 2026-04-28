---
title: "[MEDIUM] Log level is hardcoded to DEBUG with no environment-based configuration"
severity: MEDIUM
domain: observability
lens: structured-logging
labels:
  - "audit:observability/structured-logging"
---

## Summary
The cdc_log library hardcodes the initial log level to `CDC_LOG_LEVEL_DEBUG` in `log_init()`. There is no support for environment variables or configuration files to set the log level at runtime. This means:
1. All debug logs are always output (can be verbose for production).
2. Log level can only be changed programmatically via `log_set_level()`.
3. No way to adjust verbosity without recompiling or adding a serial command.

**Location:** `components/cdc_log/src/cdc_log.cpp` line 17 and 130.

## Impact
1. **Verbose production logs**: DEBUG level is very detailed - may overwhelm log aggregation systems in production.
2. **No runtime control**: Cannot adjust log verbosity without code changes or serial access.
3. **Memory/performance**: Capturing all debug logs may impact performance on resource-constrained ESP32.
4. **Less flexible deployment**: Different environments (dev, test, prod) need different log levels but all use the same default.

## Evidence
Current implementation (`components/cdc_log/src/cdc_log.cpp:17`):
```cpp
static log_level_t s_log_level = CDC_LOG_LEVEL_DEBUG;
```

Initialization (`components/cdc_log/src/cdc_log.cpp:129-131`):
```cpp
void log_init(void) {
    s_log_level = CDC_LOG_LEVEL_DEBUG;  // Hardcoded!
    console_init();
}
```

No environment variable support:
- No `getenv("LOG_LEVEL")` check
- No config file parsing
- No serial command to change log level (unless manually added)

## Recommended Fix
Add environment-based log level configuration:

1. **Modify `log_init()` in `components/cdc_log/src/cdc_log.cpp`**:
   ```cpp
   #include <stdlib.h>
   
   void log_init(void) {
       s_log_level = CDC_LOG_LEVEL_DEBUG;  // Default
   
       // Check environment variable for log level
       const char* env_level = getenv("LOG_LEVEL");
       if (env_level) {
           if (strcmp(env_level, "ERROR") == 0) s_log_level = CDC_LOG_LEVEL_ERROR;
           else if (strcmp(env_level, "WARN") == 0) s_log_level = CDC_LOG_LEVEL_WARN;
           else if (strcmp(env_level, "INFO") == 0) s_log_level = CDC_LOG_LEVEL_INFO;
           else if (strcmp(env_level, "DEBUG") == 0) s_log_level = CDC_LOG_LEVEL_DEBUG;
           else if (strcmp(env_level, "VERBOSE") == 0) s_log_level = CDC_LOG_LEVEL_VERBOSE;
           else if (strcmp(env_level, "NONE") == 0) s_log_level = CDC_LOG_LEVEL_NONE;
       }
   
       console_init();
   }
   ```

2. **Optional: Add serial command for runtime log level change**:
   - Register `LOG_LEVEL [level]` command in CommandRegistry
   - Allows changing level without rebooting

3. **Document the environment variable**:
   - Add to CLAUDE.md or README
   - List valid values: ERROR, WARN, INFO, DEBUG, VERBOSE, NONE

**Estimated effort:** ~30-45 minutes (implementation + testing).

## References
- ESP-IDF environment variables: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/environment_vars.html
- Common practice: Many embedded systems use `LOG_LEVEL` or `RUST_LOG`-style env vars for dynamic log control

</content>