/**
 * \file host_api_ui.cpp
 * \brief Real implementations for the UI subset that plugins use most.
 *
 * Pushes pre-built cdc_views onto the ViewStack. The remaining UI host API
 * (T9, PIN, slider, date/time, list with action callbacks) lands in later
 * Phase-3 steps once the Plugin object can route action-id callbacks back
 * into the WASM module.
 */

#include "plugin_manager/host_api.h"
#include "cdc_hal/IDisplay.h"
#include "cdc_views/ToastView.h"
#include "cdc_views/InfoView.h"
#include "cdc_ui/ViewStack.h"

namespace {

cdc::ui::ToastView::Icon toIcon(uint8_t v)
{
    switch (v) {
        case UI_ICON_SUCCESS: return cdc::ui::ToastView::Icon::SUCCESS;
        case UI_ICON_ERROR:   return cdc::ui::ToastView::Icon::ERROR;
        case UI_ICON_INFO:    return cdc::ui::ToastView::Icon::INFO;
        case UI_ICON_TASK:    return cdc::ui::ToastView::Icon::TASK;
        case UI_ICON_ALERT:   return cdc::ui::ToastView::Icon::ALERT;
        default:              return cdc::ui::ToastView::Icon::NONE;
    }
}

}  // namespace

extern "C" {

int host_ui_push_toast(const char* text, uint8_t icon, uint16_t duration_ms)
{
    if (!text) return HOST_ERR_INVALID_ARG;
    static cdc::ui::ToastView s_pluginToast;
    s_pluginToast.init(text, toIcon(icon), duration_ms, true);
    cdc::ui::ViewStack::instance().showModal(&s_pluginToast);
    cdc::ui::ViewStack::instance().render();
    return HOST_OK;
}

int host_ui_push_message(const char* text, uint8_t icon, uint32_t duration_ms)
{
    return host_ui_push_toast(text, icon, static_cast<uint16_t>(duration_ms));
}

int host_ui_push_info(const char* title, const char* body)
{
    if (!title || !body) return HOST_ERR_INVALID_ARG;
    auto* info = new cdc::ui::InfoView();
    info->init(title, body);
    cdc::ui::ViewStack::instance().push(info);
    return HOST_OK;
}

int host_ui_pop(void)
{
    cdc::ui::ViewStack::instance().pop();
    return HOST_OK;
}

int host_ui_pop_to_plugin(void)
{
    cdc::ui::ViewStack::instance().pop();
    return HOST_OK;
}

int host_ui_repaint(void)
{
    return HOST_OK;
}

int host_ui_wink(uint8_t count, uint16_t period_ms)
{
    cdc::hal::winkBacklight(count ? count : 2, period_ms ? period_ms : 150);
    return HOST_OK;
}

}  // extern "C"
