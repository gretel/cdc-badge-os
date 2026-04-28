---
title: "[MEDIUM] Hardcoded sleep interval and callback count constants"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "audit:code-quality/magic-values"
---

## Summary
Magic values for default sleep interval (60 seconds) and maximum callback count (8) are used in `components/cdc_hal/src/SleepController.cpp`. While defined as constants, their rationale could be better documented.

**Location:** `components/cdc_hal/src/SleepController.cpp:30,37`

## Impact
- **Maintainability**: Changing sleep behavior requires understanding why these specific values were chosen.
- **Scalability**: The 8-callback limit may need adjustment as more modules register sleep callbacks.
- **Configuration**: Default 60-second interval may not be optimal for all use cases.

## Evidence

**Line 30:**
```cpp
static constexpr uint32_t DEFAULT_LIGHT_SLEEP_INTERVAL_S = 60;
```

**Line 37:**
```cpp
static constexpr size_t MAX_CALLBACKS = 8;
```

The sleep interval is used at line 123:
```cpp
esp_sleep_enable_timer_wakeup(lightSleepIntervalS_ * 1000000ULL);
```

The callback limit is checked at lines 252 and 285:
```cpp
if (preSleepCount_ >= MAX_CALLBACKS) {
    LOG_W(TAG, "Pre-sleep callback limit reached");
    return false;
}
```

## Recommended Fix

1. Add explanatory comments:
```cpp
/** \brief Default light-sleep timer interval in seconds. */
/** \brief Balance between power savings and responsiveness. Adjust based on use case. */
static constexpr uint32_t DEFAULT_LIGHT_SLEEP_INTERVAL_S = 60;

/** \brief Maximum number of registered callbacks per callback list. */
/** \brief Current modules: UI, GPG, FIDO2, TOTP (4 modules, 8 slots provides headroom). */
static constexpr size_t MAX_CALLBACKS = 8;
```

2. Consider adding a minimum interval to prevent excessive sleep/wake cycles:
```cpp
static constexpr uint32_t MIN_SLEEP_INTERVAL_S = 5;  // Prevent too-frequent sleep
static constexpr uint32_t MAX_SLEEP_INTERVAL_S = 3600; // 1 hour max
```

3. Add validation when setting the interval:
```cpp
void Esp32SleepController::setLightSleepInterval(uint32_t seconds) {
    if (seconds < MIN_SLEEP_INTERVAL_S) seconds = MIN_SLEEP_INTERVAL_S;
    if (seconds > MAX_SLEEP_INTERVAL_S) seconds = MAX_SLEEP_INTERVAL_S;
    // ...
}
```

## References
- `components/cdc_hal/src/SleepController.cpp` - ESP32-S3 sleep controller implementation
- [ESP-IDF Sleep Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/sleep_modes.html)
