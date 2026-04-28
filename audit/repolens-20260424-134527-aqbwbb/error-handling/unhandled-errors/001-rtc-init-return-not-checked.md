---
title: "[MEDIUM] RTC init() return value not checked in main boot sequence"
severity: MEDIUM
domain: error-handling
lens: unhandled-return-values
labels:
  - "audit:error-handling/unhandled-errors"
---

## Summary
In `main/main.cpp` at line 93, the `rtc->init()` method is called but its return value is not checked. Unlike other HAL initializations in the same function (I2C, Power, Sleep, etc.), the RTC initialization error is silently ignored.

**Location:** `main/main.cpp:93`
```cpp
if (rtc) {
    rtc->init();  // Return value not checked!
}
```

## Impact
- If RTC initialization fails (e.g., hardware issue, NVS corruption), the system continues booting with potentially invalid time
- Subsequent features depending on accurate time (TOTP, certificate validation, logging timestamps) may malfunction
- The code at line 95-97 checks `rtc->isTimeSet()` but this may give false positives if init failed silently
- Inconsistent error handling pattern compared to other HAL services which all check return values

## Evidence
**main.cpp:91-97**
```cpp
cdc::hal::IRtc* rtc = cdc::hal::getRtcInstance();
if (rtc) {
    rtc->init();  // <-- Return value ignored
}
if (!rtc || !rtc->isTimeSet()) {
    LOG_W(TAG, "System time not set");
}
```

**Contrast with proper error handling at main.cpp:109-113:**
```cpp
s_i2cBus = cdc::hal::getI2cBus0();
if (s_i2cBus && s_i2cBus->init()) {
    LOG_I(TAG, "I2C bus ready");
} else {
    LOG_E(TAG, "I2C bus init failed!");
}
```

**Esp32Rtc::init() return semantics (Rtc.cpp:63-98):**
Returns `true` on success, `false` if state is not UNINITIALIZED. Could fail if:
- NVS not initialized yet
- Timezone load fails
- RTC hardware issue

## Recommended Fix
Check the return value of `rtc->init()` and log an error if it fails:

```cpp
cdc::hal::IRtc* rtc = cdc::hal::getRtcInstance();
if (rtc) {
    if (rtc->init()) {
        LOG_I(TAG, "RTC initialized");
    } else {
        LOG_E(TAG, "RTC init failed!");
    }
}
```

This maintains consistency with other HAL initialization patterns in the same function.

## References
- ESP32-S3 RTC driver documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/rtc_cntl.html
- Project convention: All other HAL services in main.cpp check return values (I2C, Power, Sleep, WiFi, Bluetooth, Keypad, Secure Element, Display)
