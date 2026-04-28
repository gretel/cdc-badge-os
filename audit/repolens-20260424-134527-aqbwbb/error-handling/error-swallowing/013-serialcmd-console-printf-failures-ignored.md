---
title: "[LOW] Serial command handlers ignore printf() return values"
severity: LOW
domain: serial_cmd
lens: error-handling
labels:
  - "serial_cmd"
  - "Console"
  - "printf"
---

## Summary
Throughout the serial command handlers in `components/serial_cmd/src/CommandRegistry.cpp` and module command handlers, `printf()` return values are ignored, potentially masking output failures.

## Impact
- Serial output may be truncated without the caller knowing
- USB CDC buffer full conditions are not detected
- Command responses may appear incomplete

## Evidence
Multiple instances across command handlers, for example in `mod_gpg/src/GpgModule.cpp`:

```cpp
// Lines 133-138 in GpgModule.cpp
static void cmd_gpg_status(const char* args) {
    // ...
    cdc::serial::Console::printf("User-ID: %s\r\n", status.user_id);
    cdc::serial::Console::printf("Curve: %s\r\n", ...);
    cdc::serial::Console::printf("Created: %lu\r\n", ...);
    cdc::serial::Console::printf("Sign Count: %lu\r\n", ...);
    // No error checking on any printf calls
}
```

Similar patterns in:
- `cmd_gpg_generate()` (line 178): `Console::printf(ok ? "OK\r\n" : "ERROR\r\n");`
- `cmd_gpg_export()` (line 192): `Console::printf("%s\r\n", pem_buf);`
- All other module command handlers

## Recommended Fix
While checking every printf may be overkill for debug output, consider:

1. Add a helper that checks and logs failures:
```cpp
static void printf_or_log(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = Console::vprintf(fmt, args);
    va_end(args);
    if (ret < 0) {
        LOG_D(TAG, "Serial printf failed");
    }
}
```

2. Or at minimum, check return values in critical responses:
```cpp
int ret = cdc::serial::Console::printf("User-ID: %s\r\n", status.user_id);
if (ret < 0) {
    LOG_D(TAG, "Serial output failed");
}
```

## References
- ESP32 printf return value: returns -1 on buffer full/error
- USB CDC buffer limits: depends on endpoint size and polling rate
