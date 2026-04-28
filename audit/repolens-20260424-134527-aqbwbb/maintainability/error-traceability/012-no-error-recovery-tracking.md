---
title: "[LOW] No error recovery tracking or statistics"
severity: LOW
domain: Error Traceability
lens: error-traceability
labels:
  - "audit:maintainability/error-traceability"
---

## Summary
The codebase lacks error recovery tracking and statistics. While errors are logged and some retry logic exists (e.g., in ModuleRegistry), there is no mechanism to track:
- How many times an error occurred
- How many times recovery succeeded vs failed
- Error frequency over time
- Recovery success rates for different error types

## Impact
- **No error trends**: Cannot identify recurring issues or patterns
- **No recovery metrics**: Cannot measure effectiveness of retry logic
- **Manual debugging**: Must manually count errors in logs to understand frequency
- **No alerting**: Cannot trigger alerts based on error thresholds

## Evidence

### Error logging without tracking:
`components/cdc_log/src/cdc_log.cpp`:
```cpp
typedef struct {
    uint32_t timestamp_ms;
    log_level_t level;
    char message[ERROR_LOG_LINE_LEN];
    // No error code, no count, no category
} error_log_entry_t;
```

**Problem**: Each error log entry is independent - no aggregation or counting.

### Retry logic without statistics:
`components/cdc_core/src/ModuleRegistry.cpp:718-760`:
```cpp
bool ModuleRegistry::retryModule(uint8_t index) {
    if (index >= count_) return false;
    IModule* module = modules_[index];
    if (!module) return false;
    
    LOG_I(TAG, "Retrying module '%s'...", name);
    
    // Stop first
    if (module->getState() == ServiceState::STARTED) {
        module->stop();
    }
    
    // Try init
    if (module->getState() == ServiceState::UNINITIALIZED) {
        if (!module->init()) {
            reportModuleError(name, "Init failed on retry");
            return false;
        }
    }
    
    // Try start
    if (!module->start()) {
        reportModuleError(name, "Start failed on retry");
        return false;
    }
    
    return true;
}
```

**Problem**: 
- No tracking of how many times retry was called
- No tracking of retry success vs failure
- No limit on retry attempts (could retry forever)
- No exponential backoff

### Error events without correlation:
`components/cdc_core/src/ModuleRegistry.cpp:673-696`:
```cpp
void ModuleRegistry::reportModuleError(const char* name, const char* message) {
    // ...
    Event evt;
    evt.type = EventType::MODULE_ERROR;
    evt.data.value = i;
    EventBus::instance().publish(evt);
}
```

**Problem**: 
- No error ID for correlation
- No tracking of error frequency
- No aggregation of similar errors

### No error counters:
- No `error_count` variable anywhere
- No `error_by_type` map or array
- No `error_by_time` (e.g., errors per minute)
- No `recovery_count` (successful recoveries)

### No error thresholds:
- No maximum error count before alerting
- No error rate limiting
- No circuit breaker pattern

## Recommended Fix

### Fix 1: Add error statistics structure

Create `components/cdc_core/ErrorStats.h`:

```cpp
/**
 * Error statistics entry
 */
struct ErrorStat {
    uint16_t code;
    const char* name;
    uint32_t count;           // Total occurrences
    uint32_t first_seen_ms;   // First occurrence timestamp
    uint32_t last_seen_ms;    // Last occurrence timestamp
    uint16_t recoveries;      // Successful recoveries
    uint16_t failures;        // Failed recoveries
};

/**
 * Error statistics collector
 */
class ErrorStats {
public:
    static constexpr int MAX_STATS = 32;
    
    /**
     * Record an error occurrence
     */
    void record(uint16_t code, const char* name);
    
    /**
     * Record a successful recovery
     */
    void recordRecovery(uint16_t code);
    
    /**
     * Record a failed recovery
     */
    void recordFailure(uint16_t code);
    
    /**
     * Get statistics for a code
     */
    const ErrorStat* get(uint16_t code);
    
    /**
     * Get all statistics
     */
    size_t getAll(ErrorStat* buffer, size_t max);
    
    /**
     * Get total error count
     */
    uint32_t getTotalCount();
    
    /**
     * Get errors in last N seconds
     */
    uint32_t getCountLast(uint32_t seconds);
    
    /**
     * Get singleton instance
     */
    static ErrorStats& instance();
};
```

### Fix 2: Integrate error stats into error logging

Update `components/cdc_log/src/cdc_log.cpp`:

```cpp
void log_write(log_level_t level, const char* tag, const char* fmt, ...) {
    // ... existing code ...
    
    // Record error stats
    if (level == CDC_LOG_LEVEL_ERROR || level == CDC_LOG_LEVEL_WARN) {
        // Extract error code from message if present
        uint16_t code = extractErrorCode(line);
        ErrorStats::instance().record(code, line);
    }
}
```

### Fix 3: Add retry statistics to ModuleRegistry

Update `components/cdc_core/src/ModuleRegistry.cpp`:

```cpp
struct ModuleRetryStats {
    uint8_t retries;
    uint8_t successes;
    uint8_t failures;
    uint32_t last_retry_ms;
};

static ModuleRetryStats retryStats[MAX_MODULES] = {};

bool ModuleRegistry::retryModule(uint8_t index) {
    if (index >= count_) return false;
    
    retryStats[index].retries++;
    retryStats[index].last_retry_ms = xTaskGetTickCount() * 1000 / 1000;
    
    // Check max retries
    if (retryStats[index].retries > 5) {
        LOG_E(TAG, "Module '%s' exceeded max retries", modules_[index]->getName());
        retryStats[index].failures++;
        return false;
    }
    
    // ... existing retry logic ...
    
    bool success = doRetry(index);
    if (success) {
        retryStats[index].successes++;
    } else {
        retryStats[index].failures++;
    }
    
    return success;
}

/**
 * Get retry statistics for module
 */
const ModuleRetryStats& getModuleRetryStats(uint8_t index) {
    return retryStats[index];
}
```

### Fix 4: Add error rate limiting

```cpp
/**
 * Error rate limiter
 */
class ErrorRateLimiter {
public:
    /**
     * Check if error rate exceeds threshold
     * @param code Error code
     * @param maxPerMinute Maximum errors per minute
     * @return true if within limit
     */
    bool check(uint16_t code, uint16_t maxPerMinute);
    
    /**
     * Record error for rate limiting
     */
    void record(uint16_t code);
};
```

### Fix 5: Add error dashboard command

```cpp
// components/serial_cmd/src/SerialCmd.cpp
static void cmd_error_stats(ICommandRegistry& reg) {
    console_printf("Error Statistics:\r\n");
    console_printf("-----------------\r\n");
    
    ErrorStat stats[32];
    size_t count = ErrorStats::instance().getAll(stats, 32);
    
    for (size_t i = 0; i < count; i++) {
        console_printf("[%d] %s\r\n", stats[i].code, stats[i].name);
        console_printf("    Count: %u, Recoveries: %u, Failures: %u\r\n",
                       stats[i].count, stats[i].recoveries, stats[i].failures);
        console_printf("    Rate: %u/min\r\n", stats[i].count / 60);
    }
}
```

### Fix 6: Add circuit breaker pattern

```cpp
/**
 * Circuit breaker for error handling
 */
class CircuitBreaker {
public:
    enum State {
        CLOSED,      // Normal operation
        OPEN,        // Failing, skip work
        HALF_OPEN    // Testing recovery
    };
    
    /**
     * Check if circuit is closed (can proceed)
     */
    bool canProceed(uint16_t code);
    
    /**
     * Record success (reset counter)
     */
    void recordSuccess(uint16_t code);
    
    /**
     * Record failure (increment counter)
     */
    void recordFailure(uint16_t code);
    
    /**
     * Get current state
     */
    State getState(uint16_t code);
};
```

## Example Usage After Fix

```cpp
// Record error with stats
void onError(const char* module, const char* error) {
    uint16_t code = generateErrorCode(module);
    ErrorStats::instance().record(code, error);
    
    // Check rate limit
    if (!ErrorRateLimiter::check(code, 10)) {
        LOG_E("ERR", "Error rate exceeded, throttling");
        return;
    }
    
    // Check circuit breaker
    if (!CircuitBreaker::canProceed(code)) {
        LOG_W("ERR", "Circuit open, skipping %s", module);
        return;
    }
    
    // Original error handling
    reportModuleError(module, error);
}

// Record recovery
void onRetrySuccess(const char* module) {
    uint16_t code = generateErrorCode(module);
    ErrorStats::instance().recordRecovery(code);
    CircuitBreaker::recordSuccess(code);
}

// Get error stats
cmd_error_stats(reg);
// Output:
// Error Statistics:
// -----------------
// [1001] GPG_SLOT_FULL
//     Count: 5, Recoveries: 3, Failures: 2
//     Rate: 2/min
```

## References
- Error log: `components/cdc_log/include/cdc_log.h`
- ModuleRegistry: `components/cdc_core/src/ModuleRegistry.cpp`
- EventBus: `components/cdc_core/include/cdc_core/EventBus.h`
