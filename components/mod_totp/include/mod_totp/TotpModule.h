#pragma once

#include "cdc_core/ModuleBase.h"

namespace cdc::mod_totp {

class TotpModule : public core::ModuleBase {
public:
    bool init() override;
    void stop() override;

    const char* getVersion() const override { return "1.0"; }
    uint8_t getMenuItems(core::ModuleMenuItem* items, uint8_t maxItems) override;
    core::IModule::SlotRequest getSlotRequest() const override;
    void setSlotRange(const core::IModule::SlotRange& range) override;

    static TotpModule& instance();

private:
    TotpModule() : ModuleBase("mod_totp") {}
    core::IModule::SlotRange slotRange_ = {};
};

} // namespace cdc::mod_totp

extern "C" void mod_totp_register();
