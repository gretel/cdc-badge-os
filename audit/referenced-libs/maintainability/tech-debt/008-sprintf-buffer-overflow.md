---
title: "[HIGH] Unsafe sprintf usage without bounds checking in CalEPD"
severity: HIGH
domain: maintainability
lens: tech-debt/safety
labels:
  - safety
  - buffer-overflow
  - c-strings
---

## Summary
The CalEPD component uses `sprintf()` without bounds checking in display driver initialization methods. This creates potential buffer overflow vulnerabilities, especially when the buffer size is small (only 3 bytes in one case).

## Impact
- **Buffer overflow risk**: `sprintf()` doesn't check if output fits in buffer
- **Stack corruption**: Small buffers (3 bytes) can easily overflow
- **Firmware crashes**: Stack corruption can cause unpredictable behavior
- **Security vulnerability**: Buffer overflows are a common attack vector

## Evidence

### Critical case - gdeh0213b73.cpp:
File: `components/CalEPD/models/gdeh0213b73.cpp`, Lines 227-229
```cpp
void Gdeh0213b73::cmd(uint8_t command){
  char buffer[3];  // Only 3 bytes!
  sprintf(buffer,"%x",command);  // No bounds checking!
  if (gpio_get_level((gpio_num_t)CONFIG_EINK_BUSY) == 1) {
    _waitBusy(buffer);
  }
  IO.cmd(command);
}
```

**Analysis:**
- Buffer size: 3 bytes (2 hex digits + null terminator)
- Command is `uint8_t` (0x00 to 0xFF = 0 to 255)
- Hex output: 1-2 characters (e.g., "a", "ff")
- **Edge case**: If command is 0xFF, sprintf writes "ff" (2 chars) + null = 3 bytes (OK)
- **But**: If debug formatting changes or buffer is reused, overflow risk increases

### Duplicate in fix variant:
File: `components/CalEPD/models/fix/gdeh0213b73.cpp`, Line 185
```cpp
sprintf(buffer,"%x",command);
```

### Typical usage pattern:
The buffer is used for debug logging:
```cpp
void Gdeh0213b73::_waitBusy(const char* message) {
    // message is "cmd" + hex value like "f" or "22"
    ESP_LOGI(TAG, "Busy wait: %s", message);
}
```

## Recommended Fix

### Option 1 - Use snprintf (recommended):
Replace `sprintf` with `snprintf` for safety:

```cpp
void Gdeh0213b73::cmd(uint8_t command){
  char buffer[4];  // Increased to 4 for safety (2 hex + optional prefix + null)
  snprintf(buffer, sizeof(buffer), "%x", command);  // Bounds checking!
  if (gpio_get_level((gpio_num_t)CONFIG_EINK_BUSY) == 1) {
    _waitBusy(buffer);
  }
  IO.cmd(command);
}
```

### Option 2 - Use C++ std::string for formatting:
```cpp
#include <string>

void Gdeh0213b73::cmd(uint8_t command){
  std::string buffer = String(command, HEX).c_str();
  if (gpio_get_level((gpio_num_t)CONFIG_EINK_BUSY) == 1) {
    _waitBusy(buffer.c_str());
  }
  IO.cmd(command);
}
```

### Option 3 - Pass command directly (most efficient):
Refactor `_waitBusy` to accept uint8_t:
```cpp
void Gdeh0213b73::_waitBusy(uint8_t command) {
    // Format inside function with proper buffer
    char buffer[4];
    snprintf(buffer, sizeof(buffer), "cmd:%x", command);
    ESP_LOGI(TAG, "Busy wait: %s", buffer);
    // ... rest of implementation
}

void Gdeh0213b73::cmd(uint8_t command){
  if (gpio_get_level((gpio_num_t)CONFIG_EINK_BUSY) == 1) {
    _waitBusy(command);  // Pass command directly
  }
  IO.cmd(command);
}
```

### Apply to both files:
1. `components/CalEPD/models/gdeh0213b73.cpp` - Line 229
2. `components/CalEPD/models/fix/gdeh0213b73.cpp` - Line 185

## References
- C standard library: `snprintf()` vs `sprintf()`: https://en.cppreference.com/w/cpp/io/c/fprintf
- Buffer overflow prevention: https://www.securecoding.cert.org/confluence/display/c/STR34-C.+Use+snprintf%28%29+instead+of+sprintf%28%29
- ESP32 string formatting best practices
