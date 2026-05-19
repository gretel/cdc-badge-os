#pragma once

#include "cdc_ui/IView.h"
#include "mod_homeassistant/HaFavorite.h"

namespace cdc::mod_homeassistant {

/**
 * \brief Slider-only view for adjusting a light's brightness (0-100%).
 *
 * Reuses `cdc::ui::SliderView` under the hood. On save, issues a POST to
 * `light/turn_on` with `brightness_pct` and pops back to HomeView.
 */
class HaLightDetailView : public ui::ViewBase {
public:
    /**
     * \brief Initializes and pushes the underlying SliderView.
     * \param entityId Target light entity id.
     * \param displayName Title for the slider screen.
     * \param initialBrightness Current brightness (0-100).
     */
    void init(const char* entityId,
              const char* displayName,
              uint8_t initialBrightness);

    void onEnter(void* context) override;
    void render(bool partial) override;
    ui::InputResult onKey(char key) override;
    const char* getName() const override { return "HaLightDetailView"; }

private:
    void onBrightnessSaved(uint16_t value);

    char    entityId_[64]   = {};
    char    displayName_[48] = {};
    uint8_t initialValue_   = 0;
};

} // namespace cdc::mod_homeassistant
