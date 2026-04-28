---
title: "[MEDIUM] Missing error correlation between EventBus and error log"
severity: MEDIUM
domain: Error Traceability
lens: error-traceability
labels:
  - "audit:maintainability/error-traceability"
---

## Summary
The EventBus publishes `MODULE_ERROR` events for error notification, but there is no correlation between these events and the error log entries. Error events and log messages cannot be joined to provide complete context for debugging.

## Impact
- **Fragmented debugging**: Developers must correlate error events and log messages manually
- **Lost context**: Error events don't include the full error message or stack context
- **Incomplete monitoring**: Cannot build error dashboards that combine event and log data
- **Missing timestamps**: Event timestamps differ from log timestamps, making correlation difficult

## Evidence

### EventBus error events lack detail:
`components/cdc_core/include/cdc_core/EventBus.h`:
```cpp
enum class EventType : uint8_t {
    // ...
    // Module error (data.ptr = module name, data.value = index)
    MODULE_ERROR,
    // ...
};

struct Event {
    EventType type;
    uint32_t timestamp;  // millis since boot
    union {
        char key;        // For KEY_* events
        uint8_t value;   // Generic value
        void* ptr;       // Pointer to extended data
    } data;
};
```

**Problem**: `MODULE_ERROR` event only carries:
- Module index (`data.value`)
- Module name pointer (`data.ptr`) - but this is just a pointer, not the error message

### Error reporting loses detail:
`components/cdc_core/src/ModuleRegistry.cpp:673-696`:
```cpp
void ModuleRegistry::reportModuleError(const char* name, const char* message) {
    // ...
    setModuleError(i, message);  // Message stored internally
    LOG_E(TAG, "Module '%s' error: %s", name, message);  // Message in log
    
    // Publish error event for UI notification
    Event evt;
    evt.type = EventType::MODULE_ERROR;
    evt.data.value = i;  // Only index, no message!
    EventBus::instance().publish(evt);
}
```

**Problem**: The error message is:
- Logged (via `LOG_E`)
- Stored internally (`setModuleError`)
- **NOT** included in the event (only index is passed)

### No error event enrichment:
When an error event is published, external subscribers cannot access:
- The error message
- The error severity
- Stack trace (if available)
- Related context data

### Error log lacks event correlation:
`components/cdc_log/src/cdc_log.cpp`:
```cpp
typedef struct {
    uint32_t timestamp_ms;
    log_level_t level;
    char message[ERROR_LOG_LINE_LEN];
    // No event ID or correlation field
} error_log_entry_t;
```

**Problem**: Error log entries have no way to correlate with EventBus events.

### Event handlers cannot query error details:
```cpp
// Current pattern - handler only gets index
void onModuleError(const Event& evt) {
    uint8_t index = evt.data.value;
    // Cannot get error message from event!
    // Must look up in ModuleRegistry (if accessible)
}
```

## Recommended Fix

### Fix 1: Enrich MODULE_ERROR event with message

Update `components/cdc_core/include/cdc_core/EventBus.h`:

```cpp
struct Event {
    EventType type;
    uint32_t timestamp;  // millis since boot
    union {
        char key;        // For KEY_* events
        uint8_t value;   // Generic value
        void* ptr;       // Pointer to extended data
        struct {
            const char* module_name;
            const char* error_message;
            uint8_t severity;  // 0=info, 1=warn, 2=critical
        } error_info;
    } data;
};
```

### Fix 2: Update error reporting to include message

Update `components/cdc_core/src/ModuleRegistry.cpp:673-696`:

```cpp
void ModuleRegistry::reportModuleError(const char* name, const char* message) {
    if (!name) return;

    for (uint8_t i = 0; i < count_; i++) {
        if (strcmp(modules_[i]->getName(), name) == 0) {
            if (modules_[i]->getState() == ServiceState::STARTED) {
                modules_[i]->stop();
                LOG_W(TAG, "Module '%s' stopped due to error", name);
            }
            setModuleError(i, message);
            LOG_E(TAG, "Module '%s' error: %s", name, message ? message : "(null)");

            // Publish enriched error event
            Event evt;
            evt.type = EventType::MODULE_ERROR;
            evt.data.error_info.module_name = name;
            evt.data.error_info.error_message = message ? message : "(null)";
            evt.data.error_info.severity = 2;  // Critical
            EventBus::instance().publish(evt);
            return;
        }
    }
    LOG_W(TAG, "reportModuleError: module '%s' not found", name);
}
```

### Fix 3: Add error severity classification

Update `EventBus.h`:

```cpp
enum class ErrorSeverity : uint8_t {
    INFO = 0,       // Informational
    WARNING = 1,    // Non-critical
    CRITICAL = 2    // Requires attention
};

struct ErrorEvent {
    const char* module_name;
    const char* error_message;
    ErrorSeverity severity;
    uint16_t error_code;  // Optional error code
};
```

### Fix 4: Add correlation ID to error events

```cpp
struct ErrorEvent {
    uint32_t correlation_id;  // Unique ID for this error
    const char* module_name;
    const char* error_message;
    ErrorSeverity severity;
    uint16_t error_code;
    uint32_t stack_trace[5];  // Optional stack trace
    uint8_t stack_frames;
};
```

### Fix 5: Add error query API

Update `ModuleRegistry.h`:

```cpp
/**
 * Get error details by module index
 * @param index Module index
 * @param out_message Output error message
 * @param out_severity Output severity
 * @return true if error exists
 */
bool getModuleError(uint8_t index, const char** out_message, 
                    ErrorSeverity* out_severity);

/**
 * Get error details by module name
 * @param name Module name
 * @param out_index Output module index
 * @param out_message Output error message
 * @return true if error exists
 */
bool getModuleErrorByName(const char* name, uint8_t* out_index,
                          const char** out_message);
```

### Fix 6: Add error event handler helper

```cpp
// components/cdc_core/ErrorEvents.h
class ErrorEvents {
public:
    /**
     * Subscribe to error events with full context
     */
    static uint8_t subscribe(void (*handler)(const ErrorEvent&));
    
    /**
     * Get last error for module
     */
    static ErrorEvent getLast(const char* module_name);
    
    /**
     * Get all errors
     */
    static size_t getAll(ErrorEvent* buffer, size_t max);
};
```

## Example Usage After Fix

```cpp
// Error event handler gets full context
void onModuleError(const Event& evt) {
    if (evt.type == EventType::MODULE_ERROR) {
        const char* module = evt.data.error_info.module_name;
        const char* msg = evt.data.error_info.error_message;
        uint8_t severity = evt.data.error_info.severity;
        
        LOG_E("ERR", "Module '%s' error: %s (severity=%d)", module, msg, severity);
        
        // Show toast with message
        showToastError(msg);
    }
}

// Correlation with error log
void onCriticalError(const ErrorEvent& err) {
    // Get error log entry
    error_log_entry_t entries[50];
    size_t count = error_log_get_entries(entries, 50);
    
    // Find matching entry by timestamp or message
    for (size_t i = 0; i < count; i++) {
        if (strstr(entries[i].message, err.error_message)) {
            // Found match - can show full context
            LOG_I("ERR", "Correlated: %s", entries[i].message);
        }
    }
}
```

## References
- EventBus: `components/cdc_core/include/cdc_core/EventBus.h`
- ModuleRegistry: `components/cdc_core/src/ModuleRegistry.cpp`
- Error Log: `components/cdc_log/include/cdc_log.h`
