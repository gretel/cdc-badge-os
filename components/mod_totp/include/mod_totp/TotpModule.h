#pragma once

#include "cdc_core/IModule.h"

namespace cdc::mod_totp {

class TotpModule : public core::IModule {
public:
    const char* getName() const override { return "mod_totp"; }
    core::ServiceState getState() const override { return state_; }
    bool init() override;
    bool start() override;
    void stop() override;

    const char* getVersion() const override { return "1.0"; }
    uint8_t getMenuItems(core::ModuleMenuItem* items, uint8_t maxItems) override;
    core::IModule::SlotRequest getSlotRequest() const override;
    void setSlotRange(const core::IModule::SlotRange& range) override;

    static TotpModule& instance();

private:
    TotpModule() = default;
    core::ServiceState state_ = core::ServiceState::UNINITIALIZED;
    core::IModule::SlotRange slotRange_ = {};
};

} // namespace cdc::mod_totp

extern "C" void mod_totp_register();
