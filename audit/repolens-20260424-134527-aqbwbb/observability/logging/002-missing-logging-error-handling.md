---
title: "[MEDIUM] Missing error logging in serial command handlers"
severity: MEDIUM
domain: logging
lens: logging-coverage
labels:
  - "audit:observability/logging"
---

## Summary
Several error paths in serial command handlers (`components/serial_cmd/src/SerialCmd.cpp`) print error messages to console but do not log them via `LOG_E()`, making it difficult to track errors programmatically or in the error log buffer.

**Key locations:**

1. **NVS commands** (lines 507-517): NVS erase/init failures print to console but don't log:
```cpp
esp_err_t err = nvs_flash_erase();
if (err != ESP_OK) {
    Console::printf("ERROR: NVS erase failed (%s)\r\n", esp_err_to_name(err));
    return;  // No LOG_E here
}
```

2. **TROPIC01 session commands** (line 958): Session start failure:
```cpp
if (se->sessionStart()) {
    Console::printf("OK: Session started\r\n");
} else {
    Console::printf("ERROR: Session start failed\r\n");  // No LOG_E
}
```

3. **PIN authentication** (lines 823-830): Empty PIN argument not logged:
```cpp
if (!args || !*args) {
    Console::printf("Usage: AUTH <pin>\r\n");
    Console::printf("Retries: %d\r\n", pm.getBadgeRetries());
    return;  // No LOG_D for debug tracking
}
```

## Impact
- **Debug difficulty**: Console output shows errors but they don't appear in structured log output
- **Error tracking**: Error log buffer (`error_log_get_entries()`) won't capture these failures
- **Troubleshooting**: Remote debugging becomes harder when errors aren't logged consistently

## Evidence
The `cdc_log` library provides an error log ring buffer accessible via `ERROR_LOG` command:
```cpp
// In cdc_log.h
#define ERROR_LOG_MAX_ENTRIES 50
size_t error_log_get_entries(error_log_entry_t* entries, size_t max_entries);
```

When `ESP_LOG` is used instead of `LOG_E`, these errors go into the standard ESP log but may not appear over USB CDC. When only `Console::printf()` is used, errors are visible but not tracked in the structured error log.

## Recommended Fix
Add `LOG_E()` calls for all error conditions in `SerialCmd.cpp`:

1. **NVS commands** (around line 507):
```cpp
if (err != ESP_OK) {
    LOG_E(TAG, "NVS erase failed: %s", esp_err_to_name(err));
    Console::printf("ERROR: NVS erase failed (%s)\r\n", esp_err_to_name(err));
    return;
}
```

2. **TROPIC01 session** (around line 958):
```cpp
if (se->sessionStart()) {
    LOG_I(TAG, "TR01 session started");
    Console::printf("OK: Session started\r\n");
} else {
    LOG_E(TAG, "TR01 session start failed");
    Console::printf("ERROR: Session start failed\r\n");
}
```

3. **PIN authentication** (around line 823):
```cpp
if (!args || !*args) {
    LOG_D(TAG, "AUTH command with no PIN argument");
    Console::printf("Usage: AUTH <pin>\r\n");
    ...
}
```

## References
- `components/cdc_log/include/cdc_log.h` - Error log API
- `components/serial_cmd/src/SerialCmd.cpp` - Serial command handlers
