#pragma once

#include "cdc_core/IModule.h"

namespace cdc::mod_hid {

/**
 * BLE HID Keyboard Module
 *
 * Provides Bluetooth HID keyboard functionality for auto-type features.
 * Registers as IKeyboardProvider service for use by other modules (TOTP, Password).
 */
class HidModule : public core::IModule {
public:
    const char* getName() const override { return "mod_hid"; }
    core::ServiceState getState() const override { return state_; }
    bool init() override;
    bool start() override;
    void stop() override;

    const char* getVersion() const override { return "1.0"; }
    uint8_t getMenuItems(core::ModuleMenuItem* items, uint8_t maxItems) override;

    static HidModule& instance();

private:
    HidModule() = default;
    core::ServiceState state_ = core::ServiceState::UNINITIALIZED;
};

} // namespace cdc::mod_hid

extern "C" void mod_hid_register();
