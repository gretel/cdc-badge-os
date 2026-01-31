#pragma once

#include "cdc_hal/ISleepController.h"
#include <cstdint>

// Forward declarations
namespace cdc::ui {
class LockScreenView;
}
namespace cdc::hal {
class IPowerManager;
}

namespace cdc::ui {

// Light sleep timeout (seconds after which lock screen enters light sleep)
static constexpr uint32_t LIGHT_SLEEP_TIMEOUT_MS = 120 * 1000;

// Light sleep management for lock screen
class SleepManager {
public:
    static SleepManager& instance();

    // Initialize with dependencies
    void init(hal::ISleepController* sleep, hal::IPowerManager* power, LockScreenView* lockScreen);

    // Check if should enter sleep (call from ui_process when on lock screen)
    void checkLockScreenSleep(uint32_t nowMs);

    // Reset the sleep timer (on key press or activity)
    void resetTimer(uint32_t nowMs);

    // Reset timer to current time
    void resetTimer();

    // Check if currently in light sleep
    bool isInLightSleep() const { return inLightSleep_; }

private:
    SleepManager() = default;

    void enterLockScreenSleep();
    void handleWakeup();

    hal::ISleepController* sleep_ = nullptr;
    hal::IPowerManager* power_ = nullptr;
    LockScreenView* lockScreen_ = nullptr;

    uint32_t lockScreenEnteredMs_ = 0;
    bool inLightSleep_ = false;
};

} // namespace cdc::ui
