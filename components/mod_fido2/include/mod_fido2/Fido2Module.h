#pragma once

#include "cdc_core/IModule.h"

namespace cdc::mod_fido2 {

class Fido2Module : public core::IModule {
public:
    const char* getName() const override { return "mod_fido2"; }
    core::ServiceState getState() const override { return state_; }
    bool init() override;
    bool start() override;
    void stop() override;
    void setSlotRange(const core::IModule::SlotRange& range) override;
    core::IModule::SlotRequest getSlotRequest() const override;
    uint8_t getMenuItems(core::ModuleMenuItem* items, uint8_t maxItems) override;

    const char* getVersion() const override { return "0.1"; }

    static Fido2Module& instance();

private:
    Fido2Module() = default;
    core::ServiceState state_ = core::ServiceState::UNINITIALIZED;
    core::IModule::SlotRange slotRange_ = {};
};

} // namespace cdc::mod_fido2

extern "C" void mod_fido2_register();
