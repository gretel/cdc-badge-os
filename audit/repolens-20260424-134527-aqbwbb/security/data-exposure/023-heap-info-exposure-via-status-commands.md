---
title: "[LOW] System status commands expose heap memory statistics useful for side-channel attacks"
severity: LOW
domain: serial-cmd
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The `STATUS` and `MEM` serial commands expose detailed heap memory statistics including:
- Current free heap size
- Minimum free heap ever recorded
- Total PSRAM available
- Free PSRAM at time of query

While useful for debugging, this information can be used for side-channel attacks to infer system activity patterns.

**Locations:**
- `components/serial_cmd/src/SerialCmd.cpp:428-440` - cmdStatus()
- `components/serial_cmd/src/SerialCmd.cpp:443-457` - cmdMem()

Example from `SerialCmd.cpp:433-434`:
```cpp
Console::printf("Free heap: %lu bytes\r\n", (unsigned long)esp_get_free_heap_size());
Console::printf("Min free heap: %lu bytes\r\n", (unsigned long)esp_get_minimum_free_heap_size());
```

## Impact
- **Side-channel leakage**: Memory allocation patterns can reveal which operations were performed
- **Timing correlation**: Free heap changes can indicate when certain modules allocate memory
- **Fingerprinting**: Specific memory layout reveals exact hardware configuration
- **Debug info in production**: May reveal internal state if commands are accessible remotely

## Evidence
File: `components/serial_cmd/src/SerialCmd.cpp:428-457`
```cpp
static void cmdStatus(const char* args) {
    (void)args;
    Console::printf("=== System Status ===\r\n");
    Console::printf("Free heap: %lu bytes\r\n", (unsigned long)esp_get_free_heap_size());
    Console::printf("Min free heap: %lu bytes\r\n", (unsigned long)esp_get_minimum_free_heap_size());
    Console::printf("Uptime: %llu ms\r\n", esp_timer_get_time() / 1000ULL);
    Console::flush();
}

static void cmdMem(const char* args) {
    (void)args;
    Console::printf("=== Memory Usage ===\r\n");
    Console::printf("Heap: %lu / %lu bytes free\r\n",
                   (unsigned long)esp_get_free_heap_size(),
                   (unsigned long)heap_caps_get_total_size(MALLOC_CAP_DEFAULT));
    size_t psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t psramTotal = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    ...
}
```

## Recommended Fix
1. Restrict STATUS/MEM commands to authenticated sessions only (add `true` to registerCommand for privileged)
2. Add a build flag to compile out memory stats commands for production
3. Consider providing only relative/percentage values instead of exact byte counts
4. Document that these commands should not be exposed in production deployments

Example fix:
```cpp
// Make STATUS and MEM commands privileged (require authentication)
reg.registerCommand({"STATUS", "Show system status", cmdStatus, "system", true});  // true = privileged
reg.registerCommand({"MEM", "Show memory usage", cmdMem, "system", true});
```

## References
- Side-channel memory attacks: https://en.wikipedia.org/wiki/Side-channel_attack
- ESP32 memory architecture: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/memory/memory-layout.html
- Related to issue #19 (verbose error messages)
