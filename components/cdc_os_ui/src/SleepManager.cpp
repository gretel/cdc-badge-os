#include "cdc_os_ui/SleepManager.h"
#include "cdc_os_ui/views/LockScreenView.h"
#include "cdc_ui/ViewStack.h"
#include "cdc_hal/ISleepController.h"
#include "cdc_hal/IPowerManager.h"
#include "cdc_hal/IDisplay.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <ctime>
#include <cstdio>

namespace cdc::ui {

// Forward declaration for status icon update
void updatePowerStatusIcons();

SleepManager& SleepManager::instance() {
    static SleepManager s_instance;
    return s_instance;
}

void SleepManager::init(hal::ISleepController* sleep, hal::IPowerManager* power, LockScreenView* lockScreen) {
    sleep_ = sleep;
    power_ = power;
    lockScreen_ = lockScreen;
    lockScreenEnteredMs_ = 0;
    inLightSleep_ = false;
}

void SleepManager::resetTimer(uint32_t nowMs) {
    lockScreenEnteredMs_ = nowMs;
}

void SleepManager::resetTimer() {
    lockScreenEnteredMs_ = esp_timer_get_time() / 1000;
}

void SleepManager::checkLockScreenSleep(uint32_t nowMs) {
    // Only on lock screen (depth == 1)
    if (ViewStack::instance().depth() != 1) return;
    if (!lockScreen_) return;

    // Initialize timer if needed
    if (lockScreenEnteredMs_ == 0) {
        lockScreenEnteredMs_ = nowMs;
        return;
    }

    // Skip if USB connected (keep device responsive)
    if (power_ && power_->isUsbConnected()) {
        lockScreenEnteredMs_ = nowMs;  // Reset timer
        // Remove light sleep icon if shown
        if ((lockScreen_->getStatusIcons() & StatusIcon::LIGHT_SLEEP) != StatusIcon::NONE) {
            lockScreen_->removeStatusIcon(StatusIcon::LIGHT_SLEEP);
        }
        return;
    }

    // Check timeout
    uint32_t elapsed = nowMs - lockScreenEnteredMs_;
    if (elapsed >= LIGHT_SLEEP_TIMEOUT_MS) {
        enterLockScreenSleep();
    }
}

void SleepManager::enterLockScreenSleep() {
    if (!lockScreen_ || !sleep_) return;

    // Show light sleep icon
    lockScreen_->addStatusIcon(StatusIcon::LIGHT_SLEEP);
    inLightSleep_ = true;

    // Render the icon before sleeping
    ViewStack::instance().render();

    // Wait for E-Paper partial refresh to complete
    vTaskDelay(pdMS_TO_TICKS(350));

    // Enter light sleep (blocking call, returns after wakeup)
    sleep_->enterLightSleep();

    // Handle wakeup
    handleWakeup();
}

void SleepManager::handleWakeup() {
    if (!sleep_ || !lockScreen_) return;

    // Immediately ensure backlight is off after wakeup to prevent glitches
    // (LEDC may briefly show wrong state after light sleep)
    auto* display = hal::getDisplayInstance();
    if (display && !display->isBacklightOn()) {
        display->backlightOff();
    }

    hal::WakeupSource source = sleep_->getWakeupSource();

    if (source == hal::WakeupSource::GPIO) {
        // Key press wakeup - user interaction
        lockScreen_->removeStatusIcon(StatusIcon::LIGHT_SLEEP);
        lockScreenEnteredMs_ = esp_timer_get_time() / 1000;  // Reset timer
        inLightSleep_ = false;

        // Update status icons (battery, USB, etc.)
        updatePowerStatusIcons();

        // Force display refresh
        lockScreen_->markDirty();

    } else if (source == hal::WakeupSource::TIMER) {
        // Timer wakeup - just update clock, keep icon, go back to sleep
        time_t now = time(nullptr);
        struct tm* tm = localtime(&now);
        if (tm) {
            char buf[40];
            snprintf(buf, sizeof(buf), "%02d:%02d", tm->tm_hour, tm->tm_min);
            lockScreen_->setClock(buf);
            snprintf(buf, sizeof(buf), "%02d.%02d.%04d", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
            lockScreen_->setDate(buf);
        }

        // Update power icons
        updatePowerStatusIcons();

        // Render clock update
        ViewStack::instance().render();
        vTaskDelay(pdMS_TO_TICKS(350));

        // Check if USB was connected during sleep
        if (power_ && power_->isUsbConnected()) {
            // USB connected - exit light sleep mode
            lockScreen_->removeStatusIcon(StatusIcon::LIGHT_SLEEP);
            lockScreenEnteredMs_ = esp_timer_get_time() / 1000;
            inLightSleep_ = false;
        } else {
            // Go back to sleep immediately
            sleep_->enterLightSleep();
            handleWakeup();  // Recursive call to handle next wakeup
        }
    } else {
        // Unknown wakeup - treat like GPIO
        lockScreen_->removeStatusIcon(StatusIcon::LIGHT_SLEEP);
        lockScreenEnteredMs_ = esp_timer_get_time() / 1000;
        inLightSleep_ = false;
    }
}

} // namespace cdc::ui
