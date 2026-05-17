#pragma once

#include "cdc_core/IModule.h"

namespace cdc::mod_sao {

class SaoModule : public core::IModule {
public:
    static SaoModule& instance();

    const char* getName() const override { return "mod_sao"; }
    const char* getVersion() const override { return "1.0.0"; }
    core::ServiceState getState() const override { return state_; }

    bool init() override;
    bool start() override;
    void stop() override;

    uint8_t getMenuItems(core::ModuleMenuItem* items, uint8_t maxItems) override;
    void onUnlock() override;

private:
    core::ServiceState state_ = core::ServiceState::UNINITIALIZED;
};

} // namespace cdc::mod_sao

extern "C" void mod_sao_register();
