#pragma once

#include "cdc_core/IModule.h"
#include <cstddef>

namespace cdc::mod_ble_serial {

/**
 * BLE Serial Module
 *
 * Provides serial command interface over Bluetooth Low Energy
 * using Nordic UART Service (NUS).
 *
 * Registers in the Bluetooth menu as a toggle item.
 * When enabled, serial commands work over BLE just like over USB.
 */
class BleSerialModule : public core::IModule {
public:
    // === IService ===
    const char* getName() const override { return "mod_ble_serial"; }
    core::ServiceState getState() const override { return state_; }
    bool init() override;
    bool start() override;
    void stop() override;

    // === IModule ===
    const char* getVersion() const override { return "1.0"; }
    uint8_t getMenuItems(core::ModuleMenuItem* items, uint8_t maxItems) override;
    void onTick(uint32_t nowMs) override;

    // === BLE Serial Specific ===

    /**
     * Check if BLE Serial is enabled
     */
    bool isEnabled() const { return enabled_; }

    /**
     * Toggle BLE Serial on/off
     */
    void toggle();

    /**
     * Singleton access
     */
    static BleSerialModule& instance();

private:
    BleSerialModule() = default;

    core::ServiceState state_ = core::ServiceState::UNINITIALIZED;
    bool enabled_ = false;

    // Menu label buffer
    static constexpr size_t LABEL_BUF_SIZE = 32;
    char labelBuf_[LABEL_BUF_SIZE] = {};

    // NVS namespace
    static constexpr const char* NVS_NAMESPACE = "mod_ble_serial";

    // I18n
    void registerStrings();

    // Console hooks
    void registerConsoleHooks();
    void unregisterConsoleHooks();

    // Pairing UI is centralized in AppUi (see ui_init). Kept as a no-op
    // for ABI stability of the .cpp call sites.
    void registerPairingCallback();

    // Settings
    void loadSettings();
    void saveSettings();
};

} // namespace cdc::mod_ble_serial

// C registration function
extern "C" void mod_ble_serial_register();
