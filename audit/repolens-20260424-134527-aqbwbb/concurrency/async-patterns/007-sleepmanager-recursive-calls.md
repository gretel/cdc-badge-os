---
title: "[LOW] SleepManager handleWakeup() uses recursive calls which can overflow stack"
severity: LOW
domain: concurrency/async-patterns
lens: async-patterns
labels:
  - "concurrency"
  - "recursion"
  - "sleep"
  - "stack-usage"
---

## Summary
In `components/cdc_os_ui/src/SleepManager.cpp`, the `handleWakeup()` function calls itself recursively at line 185 when handling timer-based wakeup. If the system experiences rapid consecutive timer wakeups (e.g., due to a bug or hardware issue), this could lead to unbounded recursion and stack overflow.

**Location:** `components/cdc_os_ui/src/SleepManager.cpp:178-190`

```cpp
void SleepManager::handleWakeup() {
    if (!sleep_ || !lockScreen_) return;

    // ... backlight handling ...

    hal::WakeupSource source = sleep_->getWakeupSource();

    if (source == hal::WakeupSource::GPIO) {
        // ... GPIO handling ...
    } else if (source == hal::WakeupSource::TIMER) {
        // ... timer handling ...
        
        if (power_ && power_->isUsbConnected()) {
            // USB connected - exit light sleep mode
            lockScreen_->removeStatusIcon(StatusIcon::LIGHT_SLEEP);
            lockScreenEnteredMs_ = esp_timer_get_time() / 1000;
            inLightSleep_ = false;
        } else {
            // Go back to sleep immediately
            sleep_->enterLightSleep();
            handleWakeup();  // Recursive call - potential stack overflow!
        }
    } else {
        // ... unknown handling ...
    }
}
```

## Impact
- **Stack overflow risk:** Each recursive call uses stack space (~100-200 bytes). Rapid consecutive timer wakeups could exhaust the task stack
- **Hard to debug:** Stack overflow manifests as random crashes, not clear error messages
- **Unbounded recursion:** No limit on recursion depth - could happen if timer wakeup fires continuously

## Evidence
File: `components/cdc_os_ui/src/SleepManager.cpp:185`
- Line 185: `handleWakeup();` - recursive call without depth limit
- The recursive call happens after `sleep_->enterLightSleep()` returns, meaning each recursion adds a new stack frame
- Task stack size: `TASK_STACK_SIZE` in `cdc_os_ui/SleepManager.h` - typically 4096 bytes

Call chain:
```
handleWakeup()
  -> sleep_->enterLightSleep()  // Blocks until wakeup
  -> handleWakeup()  // Recursive call
    -> sleep_->enterLightSleep()
    -> handleWakeup()  // Another recursion
      ...
```

## Recommended Fix
Replace recursion with a loop:

```cpp
void SleepManager::handleWakeup() {
    if (!sleep_ || !lockScreen_) return;

    // Use a loop instead of recursion
    while (true) {
        // Ensure backlight is off after wakeup
        auto* display = hal::getDisplayInstance();
        if (display && !display->isBacklightOn()) {
            display->backlightOff();
        }

        hal::WakeupSource source = sleep_->getWakeupSource();

        if (source == hal::WakeupSource::GPIO) {
            // Key press wakeup - user interaction
            lockScreen_->removeStatusIcon(StatusIcon::LIGHT_SLEEP);
            lockScreenEnteredMs_ = esp_timer_get_time() / 1000;
            inLightSleep_ = false;
            updatePowerStatusIcons();
            lockScreen_->markDirty();
            break;  // Exit loop

        } else if (source == hal::WakeupSource::TIMER) {
            // Timer wakeup - update clock
            time_t now = time(nullptr);
            struct tm* tm = localtime(&now);
            if (tm) {
                char buf[40];
                snprintf(buf, sizeof(buf), "%02d:%02d", tm->tm_hour, tm->tm_min);
                lockScreen_->setClock(buf);
                snprintf(buf, sizeof(buf), "%02d.%02d.%04d", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
                lockScreen_->setDate(buf);
            }

            updatePowerStatusIcons();
            ViewStack::instance().render();
            vTaskDelay(pdMS_TO_TICKS(350));

            // Check if USB was connected during sleep
            if (power_ && power_->isUsbConnected()) {
                // USB connected - exit light sleep mode
                lockScreen_->removeStatusIcon(StatusIcon::LIGHT_SLEEP);
                lockScreenEnteredMs_ = esp_timer_get_time() / 1000;
                inLightSleep_ = false;
                break;  // Exit loop
            } else {
                // Go back to sleep and continue loop
                sleep_->enterLightSleep();
                // Continue to next iteration
            }
        } else {
            // Unknown wakeup - treat like GPIO
            lockScreen_->removeStatusIcon(StatusIcon::LIGHT_SLEEP);
            lockScreenEnteredMs_ = esp_timer_get_time() / 1000;
            inLightSleep_ = false;
            break;  // Exit loop
        }
    }
}
```

**Alternative: Add recursion depth limit (if loop not feasible)**
```cpp
void SleepManager::handleWakeup(uint8_t depth = 0) {
    // Add max depth check
    static constexpr uint8_t MAX_RECURSION = 5;
    if (depth > MAX_RECURSION) {
        LOG_E(TAG, "Max wakeup recursion depth reached, breaking");
        return;
    }

    // ... existing code ...
    
    if (source == hal::WakeupSource::TIMER && !power_->isUsbConnected()) {
        sleep_->enterLightSleep();
        handleWakeup(depth + 1);  // Pass depth counter
    }
}
```

## References
- [FreeRTOS Stack Usage](https://www.freertos.org/Stacks-and-stack-overflow-checking.html)
- [ESP32 Task Stack Configuration](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html#configuring-tasks)
- [Recursion in Embedded Systems](https://www.embedded.com/design/programming-languages-and-compilers/4024877/Recursion-in-embedded-systems)
