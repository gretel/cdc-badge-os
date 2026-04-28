---
title: "[MEDIUM] Silent failures in ESP32-S3 wakeup GPIO configuration"
severity: MEDIUM
domain: hardware-abstraction
lens: error-handling
labels:
  - "audit:error-handling/unhandled-errors"
---

## Summary
In `components/cdc_hal/src/SleepController.cpp`, the `enterLightSleep()` and `enterDeepSleep()` functions configure GPIO wakeup without checking return values. If these calls fail, the badge may not wake correctly from sleep.

## Impact
If GPIO wakeup configuration fails:
- **Light sleep**: Badge might not wake on keypad press, requiring manual reset
- **Deep sleep**: Badge might not wake at all, appearing "dead" until power cycle
- No error is logged, making debugging difficult

## Evidence
File: `components/cdc_hal/src/SleepController.cpp:127-128, 173`

```cpp
void Esp32SleepController::enterLightSleep() {
    if (!lightSleepConfigured_) {
        // Configure timer wakeup
        if (lightSleepIntervalS_ > 0) {
            esp_sleep_enable_timer_wakeup(lightSleepIntervalS_ * 1000000ULL);  // <-- UNCHECKED
        }

        // Configure GPIO wakeup (keypad interrupt) - level triggered
        gpio_wakeup_enable(EXP_IRQ_PIN, GPIO_INTR_LOW_LEVEL);  // <-- UNCHECKED
        esp_sleep_enable_gpio_wakeup();  // <-- UNCHECKED

        lightSleepConfigured_ = true;
        LOG_I(TAG, "Light sleep configured (GPIO%d + %lus timer)",
                 EXP_IRQ_PIN, (unsigned long)lightSleepIntervalS_);
    }
    // ...
}

void Esp32SleepController::enterDeepSleep() {
    // Configure GPIO wakeup only (no timer)
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);  // <-- UNCHECKED
    esp_sleep_enable_ext1_wakeup_io(1ULL << EXP_IRQ_PIN, ESP_EXT1_WAKEUP_ANY_LOW);  // <-- UNCHECKED

    // Enter deep sleep (causes reset on wake)
    esp_deep_sleep_start();
}
```

All ESP-IDF sleep/wakeup functions return `esp_err_t` but none are checked.

## Recommended Fix
Add error checking and logging:

```cpp
void Esp32SleepController::enterLightSleep() {
    if (!lightSleepConfigured_) {
        // Configure timer wakeup
        if (lightSleepIntervalS_ > 0) {
            esp_err_t err = esp_sleep_enable_timer_wakeup(lightSleepIntervalS_ * 1000000ULL);
            if (err != ESP_OK) {
                LOG_W(TAG, "Timer wakeup config failed: %s", esp_err_to_name(err));
            }
        }

        // Configure GPIO wakeup (keypad interrupt) - level triggered
        esp_err_t err = gpio_wakeup_enable(EXP_IRQ_PIN, GPIO_INTR_LOW_LEVEL);
        if (err != ESP_OK) {
            LOG_E(TAG, "GPIO wakeup enable failed: %s", esp_err_to_name(err));
        }
        
        err = esp_sleep_enable_gpio_wakeup();
        if (err != ESP_OK) {
            LOG_E(TAG, "GPIO wakeup config failed: %s", esp_err_to_name(err));
        }

        lightSleepConfigured_ = true;
        LOG_I(TAG, "Light sleep configured (GPIO%d + %lus timer)",
                 EXP_IRQ_PIN, (unsigned long)lightSleepIntervalS_);
    }
    // ...
}
```

## References
- ESP-IDF GPIO wakeup documentation: `gpio_wakeup_enable()` returns `esp_err_t`
- ESP-IDF sleep documentation: `esp_sleep_enable_*()` functions return `esp_err_t`
