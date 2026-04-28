---
title: "[LOW] PowerManager update() called without error checking in main loop"
severity: LOW
domain: concurrency/async-patterns
lens: async-patterns
labels:
  - "concurrency"
  - "error-handling"
  - "power-management"
---

## Summary
In `main/main.cpp:246`, the `PowerManager::update()` is called in the main loop without any error handling. The method handles charger IRQs and watchdog kicks, but if I2C communication fails or the charger is in a fault state, there's no mechanism to report or recover from these errors.

**Location:** `main/main.cpp:246`

```cpp
// Main loop (lines 242-256)
while (true) {
    EventBus::instance().process();
    cdc::serial::SerialCmd::process();

    // Update power manager (handles charger IRQs)
    if (s_powerManager) {
        s_powerManager->update();  // No error check, no timeout
    }

    uint32_t nowMs = esp_timer_get_time() / 1000;
    cdc::ui::ui_process(nowMs);
    s_attestationService.onTick(nowMs);
    cdc::core::ModuleRegistry::instance().dispatchTick(nowMs);

    vTaskDelay(pdMS_TO_TICKS(10));
}
```

## Evidence
File: `main/main.cpp:246`
File: `components/cdc_hal/src/BQ25895Power.cpp:557-584`

```cpp
void BQ25895Power::update() {
    // Proactive watchdog kick every 30s (WDT timeout is 40s)
    uint32_t nowMs = (uint32_t)(esp_timer_get_time() / 1000);
    if ((nowMs - lastWdtKickMs_) >= 30000) {
        lastWdtKickMs_ = nowMs;
        kickWatchdog();  // Silent failure if I2C fails
    }

    // Handle charger IRQ
    if (charger_irq_pending) {
        charger_irq_pending = false;
        readChargerStatus();  // Silent failure if I2C fails
    }
}

// kickWatchdog() (lines 553-561)
void BQ25895Power::kickWatchdog() {
    uint8_t current = 0;
    if (readReg(BQ_REG_CHG_CTRL, &current)) {
        writeReg(BQ_REG_CHG_CTRL, current | (1 << 6));  // No error handling
    }
}

// readChargerStatus() (lines 311-402)
// Multiple I2C reads without error propagation
```

## Impact
- **Silent failures:** I2C communication errors are logged but don't propagate
- **Watchdog expiration:** If `kickWatchdog()` fails, the charger watchdog may expire
- **Stale state:** If `readChargerStatus()` fails, cached battery state becomes stale
- **No recovery:** No mechanism to detect and recover from charger communication failures

## Recommended Fix
Add error tracking and periodic state validation:

```cpp
// Add error tracking to BQ25895Power class
class BQ25895Power : public IPowerManager {
    // ... existing members ...
    
    uint8_t consecutiveFailures_ = 0;
    static constexpr uint8_t MAX_CONSECUTIVE_FAILURES = 5;
    uint32_t lastSuccessMs_ = 0;
};

// Updated update() with error handling
void BQ25895Power::update() {
    // Proactive watchdog kick every 30s (WDT timeout is 40s)
    uint32_t nowMs = (uint32_t)(esp_timer_get_time() / 1000);
    if ((nowMs - lastWdtKickMs_) >= 30000) {
        lastWdtKickMs_ = nowMs;
        if (kickWatchdog()) {
            consecutiveFailures_ = 0;
            lastSuccessMs_ = nowMs;
        } else {
            consecutiveFailures_++;
            if (consecutiveFailures_ >= MAX_CONSECUTIVE_FAILURES) {
                LOG_E(TAG, "Charger communication failed %d times", consecutiveFailures_);
                // State is now stale, consider marking as ERROR
            }
        }
    }

    // Handle charger IRQ
    if (charger_irq_pending) {
        charger_irq_pending = false;
        if (readChargerStatus()) {
            consecutiveFailures_ = 0;
            lastSuccessMs_ = nowMs;
        } else {
            consecutiveFailures_++;
            if (consecutiveFailures_ >= MAX_CONSECUTIVE_FAILURES) {
                LOG_E(TAG, "Charger communication failed %d times", consecutiveFailures_);
            }
        }
    }
}

// Add getter for health status
bool isHealthy() const {
    uint32_t nowMs = (uint32_t)(esp_timer_get_time() / 1000);
    return (nowMs - lastSuccessMs_) < 60000;  // Success within last 60s
}

// Update main loop to check health
// In main.cpp
while (true) {
    if (s_powerManager && s_powerManager->isHealthy()) {
        s_powerManager->update();
    } else if (s_powerManager) {
        LOG_W(TAG, "Power manager may be stale, checking...");
        s_powerManager->update();
    }
    // ... rest of loop
}
```

## References
- [BQ25895 Datasheet](https://www.ti.com/product/BQ25895)
- [I2C Bus Error Handling](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2c.html)
- ESP-IDF Power Management: `components/esp_pm/include/esp_pm.h`
