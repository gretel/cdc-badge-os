---
title: "[LOW] RTC initialization blocks early boot sequence"
severity: LOW
domain: startup-performance
lens: startup-perf
labels:
  - "audit:performance/startup-perf"
---

## Summary
RTC initialization (`cdc::hal::getRtcInstance()->init()`) is called early in boot (`main.cpp:92-97`) before critical services like I2C and display are initialized. While the code handles failure gracefully, the RTC init is a synchronous blocking operation that could be deferred.

**Location:** `main/main.cpp:92-97`

## Impact
- **Early boot delay**: RTC initialization happens before the display is ready, so users don't see any feedback
- **Blocking I2C read**: RTC typically communicates over I2C, adding latency to the critical path
- **No parallelism**: RTC init could happen in parallel with other hardware initialization

## Evidence
From `main/main.cpp:92-97`:
```cpp
// === TIME VALIDATION ===
cdc::hal::IRtc* rtc = cdc::hal::getRtcInstance();
if (rtc) {
    rtc->init();
}
if (!rtc || !rtc->isTimeSet()) {
    LOG_W(TAG, "System time not set");
}
```

This runs before I2C bus initialization (`main.cpp:99-113`), which suggests the RTC init may be using a different I2C port or is blocking for another reason.

## Recommended Fix
**Defer RTC initialization** to after the display is ready:

1. Move RTC init to after display initialization (line 200)
2. If time is not set, show a message on the display instead of just logging
3. RTC doesn't need to be ready for the system to become responsive

```cpp
// After display->showSplash()
cdc::hal::IRtc* rtc = cdc::hal::getRtcInstance();
if (rtc) {
    rtc->init();
    if (!rtc->isTimeSet()) {
        LOG_W(TAG, "System time not set");
        // Optionally show on display
        display->showStatus("Time not set");
    }
}
```

## References
- ESP-IDF: [RTC initialization](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/rtc.html)