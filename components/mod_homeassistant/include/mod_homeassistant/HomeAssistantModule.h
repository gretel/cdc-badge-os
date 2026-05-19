#pragma once

#include "cdc_core/ModuleBase.h"

namespace cdc::mod_homeassistant {

/**
 * \brief Home Assistant controller module.
 *
 * Provides a quick-action favorites list backed by the Home Assistant REST
 * API. WiFi is requested on demand when the user enters the module and
 * released on exit (no permanent network connection).
 *
 * Token is stored encrypted in a TROPIC01 R-Memory slot; URL and favorites
 * live in the module's NVS namespace.
 */
class HomeAssistantModule : public core::ModuleBase {
public:
    bool init() override;
    void stop() override;

    const char* getVersion() const override { return "0.1"; }
    uint8_t getMenuItems(core::ModuleMenuItem* items, uint8_t maxItems) override;
    core::IModule::SlotRequest getSlotRequest() const override;
    void setSlotRange(const core::IModule::SlotRange& range) override;

    /**
     * \brief Slot range assigned by the module registry (used by token storage).
     */
    const core::IModule::SlotRange& slotRange() const { return slotRange_; }

    static HomeAssistantModule& instance();

private:
    HomeAssistantModule() : ModuleBase("mod_homeassistant") {}
    core::IModule::SlotRange slotRange_ = {};
};

} // namespace cdc::mod_homeassistant

extern "C" void mod_homeassistant_register();
