---
title: "[LOW] Uptime and timing information exposed via serial commands"
severity: LOW
domain: serial-cmd
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The serial commands expose precise timing information including system uptime and timer values that can be used for:
- Boot time fingerprinting
- Activity pattern analysis
- Timing-based side-channel attacks

**Locations:**
- `components/serial_cmd/src/SerialCmd.cpp:435` - Uptime in STATUS command
- `components/serial_cmd/src/SerialCmd.cpp:1206` - AUTH timeout logging

Example from `SerialCmd.cpp:435`:
```cpp
Console::printf("Uptime: %llu ms\r\n", esp_timer_get_time() / 1000ULL);
```

## Impact
- **Boot fingerprinting**: Exact uptime reveals when device was last booted
- **Activity correlation**: Timing patterns can reveal when specific operations occur
- **Session tracking**: AUTH timeout (60s default) can be precisely calculated
- **Power cycle detection**: Can determine if device was rebooted recently

## Evidence
File: `components/serial_cmd/src/SerialCmd.cpp:428-440`
```cpp
static void cmdStatus(const char* args) {
    (void)args;
    Console::printf("=== System Status ===\r\n");
    Console::printf("Free heap: %lu bytes\r\n", (unsigned long)esp_get_free_heap_size());
    Console::printf("Min free heap: %lu bytes\r\n", (unsigned long)esp_get_minimum_free_heap_size());
    Console::printf("Uptime: %llu ms\r\n", esp_timer_get_time() / 1000ULL);  // Exposes exact uptime
    Console::flush();
}
```

File: `components/cdc_log/include/cdc_log.h` shows error log timestamps:
```cpp
typedef struct {
    uint32_t timestamp_ms;  // Millisecond precision timestamps
    log_level_t level;
    char message[ERROR_LOG_LINE_LEN];
} error_log_entry_t;
```

## Recommended Fix
1. Round uptime to nearest minute for STATUS command (less precision needed)
2. Consider making uptime optional via build flag
3. Document that precise timing may leak operational patterns
4. For production, consider adding jitter to timing outputs

Example fix:
```cpp
// Round to nearest minute instead of exact milliseconds
uint64_t uptime_sec = esp_timer_get_time() / 1000000ULL;
Console::printf("Uptime: %llu seconds (~%llu minutes)\r\n", 
                (unsigned long long)uptime_sec,
                (unsigned long long)uptime_sec / 60);
```

## References
- Timing side-channel attacks: https://en.wikipedia.org/wiki/Timing_attack
- ESP32 timer system: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/timer.html
- Related to issue #23 (heap info exposure)
