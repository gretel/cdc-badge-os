#pragma once

#include <cstdint>

namespace cdc {
namespace hal {
class IDisplay;
class IKeypad;
class IPowerManager;
class ISleepController;
class ISecureElement;
} // namespace hal
} // namespace cdc

namespace cdc::ui {

struct UiDeps {
    hal::IDisplay* display = nullptr;
    hal::IKeypad* keypad = nullptr;
    hal::IPowerManager* power = nullptr;
    hal::ISleepController* sleep = nullptr;
    hal::ISecureElement* secureElement = nullptr;
};

// Initialize UI system (views, menus, callbacks)
void ui_init(const UiDeps& deps);

// Call after modules are initialized to rebuild menus with module items
void ui_on_modules_ready();

// Rebuild all menus (call after module state changes)
void ui_rebuild_menus();

// Per-loop UI processing (input, timers, rendering)
void ui_process(uint32_t nowMs);

/**
 * \brief Reboots the device into USB download (bootloader) mode.
 *
 * Shows a "Bootloader Mode" toast, forces an EPD full refresh, switches
 * off the backlight, then spawns a dedicated task that arms
 * `RTC_CNTL_FORCE_DOWNLOAD_BOOT` and calls `esp_restart()`. Returns to
 * the caller so any locks it holds are released before the reset runs.
 */
void rebootIntoBootloader();

} // namespace cdc::ui
