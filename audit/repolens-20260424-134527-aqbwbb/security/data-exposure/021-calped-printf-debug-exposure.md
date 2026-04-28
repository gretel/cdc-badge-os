---
title: "[MEDIUM] CalEPD library uses raw printf for debug output exposing buffer sizes and memory layout"
severity: MEDIUM
domain: CalEPD
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The CalEPD display library (third-party e-paper driver) uses raw `printf()` calls throughout for debug output instead of the project's `cdc_log` library. These printf statements expose internal buffer sizes, memory layout, and display dimensions that could aid an attacker in understanding the system architecture.

**Locations found:**
- `components/CalEPD/models/wave12i48.cpp:28-29` - Constructor prints buffer size
- `components/CalEPD/models/wave12i48.cpp:36` - Init debug prints
- `components/CalEPD/models/wave12i48.cpp:46` - fillScreen prints buffer size
- `components/CalEPD/models/gdew075HD.cpp:22,114` - Buffer size exposure
- `components/CalEPD/models/gdeh0154d67.cpp:103` - BUFF Size print
- And 15+ more locations across different display models

Example from `wave12i48.cpp:28-29`:
```cpp
printf("Wave12I48() constructor injects IO and extends Adafruit_GFX(%d,%d) Pix Buffer[%d]\nNOTE: Requires external RAM\n",
WAVE12I48_WIDTH, WAVE12I48_HEIGHT, (int) WAVE12I48_BUFFER_SIZE);
```

## Impact
- **Information leakage**: Exposes exact buffer sizes and memory layout
- **Architecture fingerprinting**: Attacker can identify specific display models and memory requirements
- **Bypasses logging control**: printf output goes directly to UART, cannot be filtered by log level
- **Consistent with issue #10**: Similar to ESP_LOG bypass but affects third-party library

## Evidence
File: `components/CalEPD/models/wave12i48.cpp:28-29`
```cpp
printf("Wave12I48() constructor injects IO and extends Adafruit_GFX(%d,%d) Pix Buffer[%d]\nNOTE: Requires external RAM\n",
WAVE12I48_WIDTH, WAVE12I48_HEIGHT, (int) WAVE12I48_BUFFER_SIZE);
```

File: `components/CalEPD/models/gdew075HD.cpp:114`
```cpp
printf("Sending a %d bytes buffer via SPI\n", sizeof(_buffer));
```

## Recommended Fix
1. Create a wrapper macro for CalEPD that maps to cdc_log when DEBUG_MODE is enabled
2. Alternatively, wrap all printf calls in CalEPD with a debug flag check that uses cdc_log
3. For production builds, ensure DEBUG_MODE=0 disables all these printf calls

Example wrapper approach:
```cpp
#ifdef DEBUG_MODE
#define CALPD_LOG(fmt, ...) LOG_D("CalEPD", fmt, ##__VA_ARGS__)
#else
#define CALPD_LOG(fmt, ...)
#endif

// Then replace:
if (debug_enabled) printf("fillScreen(%x) Buffer size:%d\n", color, (int)WAVE12I48_BUFFER_SIZE);

// With:
if (debug_enabled) CALPD_LOG("fillScreen(%x) Buffer size:%d", color, (int)WAVE12I48_BUFFER_SIZE);
```

## References
- Related to issue #10 (ESP_LOG bypasses cdc_log)
- Project logging standard: `components/cdc_log/include/cdc_log.h`
- Feature flags: `components/cdc_core/feature_flags.h`
