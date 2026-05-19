#include "mod_homeassistant/HaLightDetailView.h"
#include "mod_homeassistant/HaClient.h"
#include "mod_homeassistant/HaI18n.h"
#include "cdc_ui/ViewStack.h"
#include "cdc_views/SliderView.h"
#include "cdc_views/ToastView.h"
#include "cdc_log.h"
#include <cstring>
#include <cstdio>

static const char* TAG = "HA_LIGHT";

namespace cdc::mod_homeassistant {

/** \brief Reused SliderView instance pushed for brightness editing. */
static ui::SliderView s_slider;

/** \brief Entity id captured for the slider save callback. */
static char s_targetEntity[64] = {};

/**
 * \brief Slider step callback: small steps below 10%, larger steps above.
 *        Mirrors the brightness slider used in the system settings.
 */
static uint16_t brightnessStep(uint16_t v, bool increasing) {
    (void)increasing;
    if (v < 10)  return 1;
    if (v < 30)  return 5;
    return 10;
}

/**
 * \brief Slider save callback: posts brightness_pct to HA.
 */
static void onSliderSave(uint16_t value) {
    if (s_targetEntity[0] == '\0') return;
    char extra[48] = {};
    snprintf(extra, sizeof(extra), "\"brightness_pct\":%u", static_cast<unsigned>(value));
    HaClient client;
    if (!client.loadConfig()) {
        ui::showToastError(mstr(STR_HA_CONFIG_MISSING));
        return;
    }
    HaResult res = client.callService("light", "turn_on", s_targetEntity, extra);
    if (res != HaResult::OK) {
        LOG_W(TAG, "callService failed (res=%d, status=%d)",
              static_cast<int>(res), client.getLastHttpStatus());
        ui::showToastError(mstr(STR_HA_ERROR));
        return;
    }
    ui::showToastSuccess(mstr(STR_TOAST_OK));
}

void HaLightDetailView::init(const char* entityId,
                             const char* displayName,
                             uint8_t initialBrightness) {
    entityId_[0]    = '\0';
    displayName_[0] = '\0';
    if (entityId)    strncpy(entityId_,    entityId,    sizeof(entityId_)    - 1);
    if (displayName) strncpy(displayName_, displayName, sizeof(displayName_) - 1);
    initialValue_ = initialBrightness > 100 ? 100 : initialBrightness;

    strncpy(s_targetEntity, entityId_, sizeof(s_targetEntity) - 1);
    s_targetEntity[sizeof(s_targetEntity) - 1] = '\0';

    s_slider.init(displayName_[0] ? displayName_ : mstr(STR_BRIGHTNESS),
                  /*min=*/0, /*max=*/100, /*initial=*/initialValue_,
                  /*step=*/5, "%");
    s_slider.setStepCallback(brightnessStep);
    s_slider.setOnSave(onSliderSave);
    ui::ViewStack::instance().push(&s_slider);
}

void HaLightDetailView::onEnter(void* context) {
    (void)context;
    dirty_ = false;
}

void HaLightDetailView::render(bool) {
    clearDirty();
}

ui::InputResult HaLightDetailView::onKey(char) {
    return ui::InputResult::IGNORED;
}

void HaLightDetailView::onBrightnessSaved(uint16_t value) {
    (void)value;
}

} // namespace cdc::mod_homeassistant
