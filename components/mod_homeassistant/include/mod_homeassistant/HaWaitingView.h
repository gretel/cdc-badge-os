#pragma once

#include "cdc_ui/IView.h"
#include <cstdint>

namespace cdc::mod_homeassistant {

/**
 * \brief Setup-pending screen.
 *
 * Shown when URL or token is missing. Lists the pending steps as static text
 * and polls NVS / R-Memory in `onTick()` to detect when the user has issued
 * the corresponding serial command. Once both URL and token are present, the
 * module pings the HA instance and transitions to HaHomeView.
 *
 * `KEY_BACK` (3) opens a context menu offering the GUI URL fallback wizard.
 */
class HaWaitingView : public ui::ViewBase {
public:
    void onEnter(void* context) override;
    void onTick(uint32_t nowMs) override;
    void render(bool partial) override;
    ui::InputResult onKey(char key) override;
    const char* getName() const override { return "HaWaitingView"; }
    const char* getFooterHint() const override;

private:
    void openContextMenu();
    void attemptTransition();

    bool     urlPresent_   = false;
    bool     tokenPresent_ = false;
    uint32_t lastPollMs_   = 0;
};

} // namespace cdc::mod_homeassistant
