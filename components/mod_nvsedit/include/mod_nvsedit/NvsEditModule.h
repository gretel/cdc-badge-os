#pragma once

#include "cdc_core/IModule.h"

namespace cdc::mod_nvsedit {

/**
 * NVS Editor Module
 *
 * Provides a GUI for browsing and editing NVS (Non-Volatile Storage).
 * Privileged tool: delete actions are disabled by default and require
 * FEATURE_NVS_EDIT=1 at build time.
 */
class NvsEditModule : public cdc::core::IModule {
public:
    // IService
    const char* getName() const override { return "mod_nvsedit"; }
    cdc::core::ServiceState getState() const override { return state_; }
    bool init() override;
    bool start() override;
    void stop() override;

    // IModule
    const char* getVersion() const override { return "1.0.0"; }
    uint8_t getMenuItems(cdc::core::ModuleMenuItem* items, uint8_t maxItems) override;

private:
    cdc::core::ServiceState state_ = cdc::core::ServiceState::UNINITIALIZED;
};

} // namespace cdc::mod_nvsedit

// Registration function (called by auto-generated code)
extern "C" void mod_nvsedit_register();
