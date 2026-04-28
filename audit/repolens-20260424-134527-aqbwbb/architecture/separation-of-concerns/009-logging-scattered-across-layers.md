---
title: "[MEDIUM] Logging cross-cutting concern scattered across domain layers"
severity: MEDIUM
domain: architecture
lens: separation-of-concerns
labels:
  - "audit:architecture/separation-of-concerns"
---

## Summary
The codebase lacks a centralized logging abstraction. Instead of using a unified logging service that can be configured, intercepted, or routed centrally, logging calls are scattered across all layers using different mechanisms:
- `LOG_I`, `LOG_E`, `LOG_W`, `LOG_D` macros from `cdc_log.h` (used inconsistently)
- Raw `printf` calls mixed throughout
- Direct string formatting in business logic

This violates separation of concerns because **logging is a cross-cutting concern** that should be handled via middleware/decorators rather than being interleaved with domain logic.

**Location**: Multiple files across all layers:
- `components/cdc_core/src/ModuleRegistry.cpp:685` - `LOG_E` mixed with error handling
- `components/cdc_core/src/EventBus.cpp:28` - `LOG_W` mixed with initialization logic
- `components/mod_gpg/src/gpg.cpp:115` - NVS access mixed with logging
- `components/cdc_os_ui/src/AppUi.cpp:521` - NVS access mixed with UI initialization

## Impact
- **Maintainability**: Changing log format, level, or destination requires touching business logic
- **Testability**: Domain logic cannot be tested without capturing log output
- **Flexibility**: Cannot easily enable/disable logging per module or layer
- **Consistency**: Different modules may use different logging styles/levels
- **Debugging**: Hard to filter logs by layer (UI vs. business vs. data access)

## Evidence

### Example 1: Logging mixed with error handling (ModuleRegistry)
```cpp
// components/cdc_core/src/ModuleRegistry.cpp:685
void ModuleRegistry::reportModuleError(const char* name, const char* message) {
    for (uint8_t i = 0; i < count_; i++) {
        if (strcmp(modules_[i]->getName(), name) == 0) {
            if (modules_[i]->getState() == ServiceState::STARTED) {
                modules_[i]->stop();
                LOG_W(TAG, "Module '%s' stopped due to error", name);  // Logging mixed here
            }
            setModuleError(i, message);
            LOG_E(TAG, "Module '%s' error: %s", name, message ? message : "(null)");  // And here
            EventBus::instance().publish(evt);
            return;
        }
    }
    LOG_W(TAG, "reportModuleError: module '%s' not found", name);  // And here
}
```

### Example 2: Logging mixed with initialization (EventBus)
```cpp
// components/cdc_core/src/EventBus.cpp:28
bool EventBus::init(size_t queueSize) {
    if (initialized_) {
        LOG_W(TAG, "Already initialized");  // Logging mixed with state check
        return true;
    }
    
    queue_ = xQueueCreate(queueSize, sizeof(Event));
    if (!queue_) {
        LOG_E(TAG, "Failed to create event queue");  // Logging mixed with error handling
        return false;
    }
    
    initialized_ = true;
    LOG_I(TAG, "Initialized with queue size %u", queueSize);  // Logging mixed with success
    return true;
}
```

### Example 3: Inconsistent logging patterns
```cpp
// Some files use LOG_* macros:
components/cdc_core/src/EventBus.cpp: LOG_I(TAG, "Initialized with queue size %u", queueSize);

// Others use printf directly:
components/mod_gpg/src/gpg.cpp: printf("GPG: %s\n", __func__);

// Some have no logging at all:
components/cdc_ui/src/ViewStack.cpp: (no logging for state changes)
```

### Example 4: TAG constants scattered
```cpp
// Each file defines its own TAG:
components/cdc_core/src/EventBus.cpp: static const char* TAG = "EventBus";
components/cdc_core/src/ModuleRegistry.cpp: static const char* TAG = "ModuleReg";
components/mod_gpg/src/gpg.cpp: (no TAG, uses printf)
components/cdc_os_ui/src/AppUi.cpp: (no TAG, uses LOG_* without consistent TAG)
```

## Recommended Fix

### 1. Create a centralized logging service

```cpp
// components/cdc_core/include/cdc_core/Logger.h
namespace cdc::core {

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

enum class LogCategory {
    CORE,
    UI,
    HAL,
    MODULE_GPG,
    MODULE_FIDO2,
    MODULE_TOTP,
    MODULE_PASSWORD,
    // ...
};

class Logger {
public:
    static Logger& instance();
    
    void init(LogLevel level = LogLevel::INFO);
    void setCategoryLevel(LogCategory cat, LogLevel level);
    
    void log(LogCategory cat, LogLevel level, const char* file, int line, 
             const char* fmt, ...);
    
    // Convenience methods
    void debug(LogCategory cat, const char* fmt, ...);
    void info(LogCategory cat, const char* fmt, ...);
    void warn(LogCategory cat, const char* fmt, ...);
    void error(LogCategory cat, const char* fmt, ...);
};

// Macro for easy use
#define LOG(cat, level, ...) \
    cdc::core::Logger::instance().log(cat, level, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_D(cat, ...) LOG(cat, LogLevel::DEBUG, __VA_ARGS__)
#define LOG_I(cat, ...) LOG(cat, LogLevel::INFO, __VA_ARGS__)
#define LOG_W(cat, ...) LOG(cat, LogLevel::WARNING, __VA_ARGS__)
#define LOG_E(cat, ...) LOG(cat, LogLevel::ERROR, __VA_ARGS__)

} // namespace cdc::core
```

### 2. Refactor existing code to use centralized logging

```cpp
// Before:
static const char* TAG = "EventBus";
LOG_I(TAG, "Initialized with queue size %u", queueSize);

// After:
LOG_I(LogCategory::CORE, "EventBus initialized with queue size %u", queueSize);
```

### 3. Benefits of centralized logging
- **Separation of concerns**: Logging is no longer mixed with domain logic
- **Configurability**: Enable/disable logging per category at runtime
- **Routing**: Send different categories to different outputs (USB, file, BLE)
- **Testing**: Mock logger for unit tests
- **Consistency**: Standard format, levels, and categories across all modules

## References
- [Cross-cutting concern (Wikipedia)](https://en.wikipedia.org/wiki/Cross-cutting_concern)
- [Aspect-Oriented Programming](https://en.wikipedia.org/wiki/Aspect-oriented_programming)
- Existing issue #006 covers NVS persistence scattering (related infrastructure concern)
