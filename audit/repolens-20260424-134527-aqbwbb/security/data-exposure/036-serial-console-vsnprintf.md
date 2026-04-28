---
title: "[LOW] Serial Console uses vsnprintf which may expose formatted data to serial output"
severity: LOW
domain: serial-cmd
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The Serial Console component uses `vsnprintf` to format output strings for serial transmission. While `vsnprintf` is safer than `sprintf`, the formatted output is sent directly to the serial console, potentially exposing sensitive data if commands log structured information.

**Location:** `components/serial_cmd/src/Console.cpp:45`

## Impact
- **Formatted data exposure**: Any data passed to Console::printf is sent to serial output
- **Command output leakage**: Structured command results are fully exposed
- **No filtering layer**: Console doesn't filter or redact sensitive patterns
- **Buffer size dependence**: Relies on callers to use appropriate buffer sizes

## Evidence
File: `components/serial_cmd/src/Console.cpp:40-50`
```cpp
void Console::vprintf(const char* format, va_list args) {
    char buffer[256];
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    if (len < 0 || len >= sizeof(buffer)) {
        // Truncation or error
    }
    // Send buffer to serial output
    // ...
}
```

The Console class is used throughout the codebase for command output:
- `components/serial_cmd/src/SerialCmd.cpp` - All command output
- `components/serial_cmd/src/CommandRegistry.cpp` - Error messages
- `components/mod_nvsedit/src/NvsEditModule.cpp` - NVS data display

## Recommended Fix
1. Add a redaction layer for common sensitive patterns (PINs, secrets, hashes)
2. Consider adding a DEBUG_MODE check before formatting sensitive data
3. Document that Console::printf outputs directly to serial (no filtering)

Example fix:
```cpp
// Add a simple redaction helper
static bool needsRedaction(const char* format) {
    return strstr(format, "PIN") || strstr(format, "secret") || strstr(format, "hash");
}

void Console::vprintf(const char* format, va_list args) {
    char buffer[256];
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    if (needsRedaction(format)) {
        // Optionally redact or limit output
    }
    // ...
}
```

## References
- vsnprintf safety: https://www.cplusplus.com/reference/cstdio/vsnprintf/
- Related to issue #18 (NVS data exposure)
- Related to issue #1 (password exposure)
