---
title: "[LOW] Inconsistent line endings in log output (\n vs \r\n)"
severity: LOW
domain: observability
lens: structured-logging
labels:
  - "audit:observability/structured-logging"
---

## Summary
The cdc_log library uses inconsistent line endings: most console output uses `\r\n` (CRLF) for serial terminal compatibility, but the main `log_write()` function uses `\n` (LF only) on line 179. This can cause display issues in some serial terminals and log aggregation systems.

**Location:** `components/cdc_log/src/cdc_log.cpp` line 179.

## Impact
1. **Terminal display issues**: Some serial terminals (e.g., PuTTY, minicom) expect `\r\n` for proper line wrapping.
2. **Inconsistent output**: Error log entries use `\r\n` while regular log entries use `\n`.
3. **Log parsing complexity**: Log aggregators may need to handle both line ending styles.

## Evidence
Inconsistent line endings in `components/cdc_log/src/cdc_log.cpp`:

Error log dump (lines 103, 107, 117) - **Uses CRLF**:
```cpp
console_printf("Error log: (empty)\r\n");
console_printf("Error log (%zu entries):\r\n", s_error_log_count);
console_printf("[%02lu:%02lu:%02lu][%s] %s\r\n", ...);
```

Main log output (line 179) - **Uses LF only**:
```cpp
console_printf("%s\n", line);
```

## Recommended Fix
Change line 179 to use `\r\n` for consistency:

```cpp
// Before:
console_printf("%s\n", line);

// After:
console_printf("%s\r\n", line);
```

**Estimated effort:** ~5 minutes (single line change).

## References
- Serial terminal conventions: https://en.wikipedia.org/wiki/Newline
- ESP-IDF serial output: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/console.html

</content>