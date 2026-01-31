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

} // namespace cdc::ui
