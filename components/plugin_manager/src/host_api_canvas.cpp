/**
 * \file host_api_canvas.cpp
 * \brief extern-C adapter for the plugin canvas view.
 *
 * Push a CanvasView, then forward draw/widget/focus calls to it. All state
 * lives in PluginUiState / CanvasView; this TU only marshals C arguments.
 */

#include "plugin_manager/PluginUiState.h"
#include "plugin_manager/host_api.h"
#include "cdc_views/CanvasView.h"

namespace {

cdc::ui::CanvasView* canvas()
{
    return cdc::plugin_manager::PluginUiState::instance().canvasView();
}

}  // namespace

extern "C" {

int host_view_canvas_push(const char* title, uint32_t key_action_id,
                          uint32_t widget_action_id)
{
    return cdc::plugin_manager::PluginUiState::instance()
        .pushCanvas(title, key_action_id, widget_action_id);
}

int host_view_canvas_get_body_size(uint16_t* w, uint16_t* h)
{
    auto* c = canvas();
    if (!c || !w || !h) return HOST_ERR_NOT_FOUND;
    c->getBodySize(w, h);
    return HOST_OK;
}

int host_view_canvas_set_footer(const char* hint)
{
    return cdc::plugin_manager::PluginUiState::instance().setViewFooter(hint);
}

int host_view_canvas_clear(void)
{
    auto* c = canvas();
    if (!c) return HOST_ERR_NOT_FOUND;
    c->clearBody();
    return HOST_OK;
}

int host_view_canvas_set_text_size(uint8_t size)
{
    auto* c = canvas();
    if (!c) return HOST_ERR_NOT_FOUND;
    c->setTextSize(size);
    return HOST_OK;
}

int host_view_canvas_set_text_color(bool inverted)
{
    auto* c = canvas();
    if (!c) return HOST_ERR_NOT_FOUND;
    c->setTextInverted(inverted);
    return HOST_OK;
}

int host_view_canvas_draw_text(int16_t x, int16_t y, const char* text)
{
    auto* c = canvas();
    if (!c) return HOST_ERR_NOT_FOUND;
    c->drawText(x, y, text);
    return HOST_OK;
}

int host_view_canvas_draw_text_aligned(int16_t x, int16_t y, int16_t w,
                                       const char* text, uint8_t align)
{
    auto* c = canvas();
    if (!c) return HOST_ERR_NOT_FOUND;
    c->drawTextAligned(x, y, w, text, align);
    return HOST_OK;
}

int host_view_canvas_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, bool filled)
{
    auto* c = canvas();
    if (!c) return HOST_ERR_NOT_FOUND;
    c->drawRect(x, y, w, h, filled);
    return HOST_OK;
}

int host_view_canvas_invert_rect(int16_t x, int16_t y, int16_t w, int16_t h)
{
    auto* c = canvas();
    if (!c) return HOST_ERR_NOT_FOUND;
    c->invertRect(x, y, w, h);
    return HOST_OK;
}

int host_view_canvas_hline(int16_t x, int16_t y, int16_t w)
{
    auto* c = canvas();
    if (!c) return HOST_ERR_NOT_FOUND;
    c->drawHLine(x, y, w);
    return HOST_OK;
}

int host_view_canvas_vline(int16_t x, int16_t y, int16_t h)
{
    auto* c = canvas();
    if (!c) return HOST_ERR_NOT_FOUND;
    c->drawVLine(x, y, h);
    return HOST_OK;
}

int host_view_canvas_commit(bool full_refresh)
{
    auto* c = canvas();
    if (!c) return HOST_ERR_NOT_FOUND;
    c->commit(full_refresh);
    return HOST_OK;
}

int host_view_canvas_add_slider(uint32_t widget_id, int32_t min, int32_t max,
                                int32_t initial, int32_t step)
{
    auto* c = canvas();
    if (!c) return HOST_ERR_NOT_FOUND;
    return c->addSlider(widget_id, min, max, initial, step) ? HOST_OK : HOST_ERR_INVALID_ARG;
}

int host_view_canvas_add_text(uint32_t widget_id, uint16_t max_len, const char* initial)
{
    auto* c = canvas();
    if (!c) return HOST_ERR_NOT_FOUND;
    return c->addText(widget_id, max_len, initial) ? HOST_OK : HOST_ERR_INVALID_ARG;
}

int host_view_canvas_add_button(uint32_t widget_id)
{
    auto* c = canvas();
    if (!c) return HOST_ERR_NOT_FOUND;
    return c->addButton(widget_id) ? HOST_OK : HOST_ERR_INVALID_ARG;
}

int host_view_canvas_remove_widget(uint32_t widget_id)
{
    auto* c = canvas();
    if (!c) return HOST_ERR_NOT_FOUND;
    return c->removeWidget(widget_id) ? HOST_OK : HOST_ERR_NOT_FOUND;
}

int host_view_canvas_set_value(uint32_t widget_id, int32_t value)
{
    auto* c = canvas();
    if (!c) return HOST_ERR_NOT_FOUND;
    return c->setValue(widget_id, value) ? HOST_OK : HOST_ERR_NOT_FOUND;
}

int host_view_canvas_get_value(uint32_t widget_id, int32_t* out)
{
    auto* c = canvas();
    if (!c) return HOST_ERR_NOT_FOUND;
    return c->getValue(widget_id, out) ? HOST_OK : HOST_ERR_NOT_FOUND;
}

int host_view_canvas_set_text(uint32_t widget_id, const char* text)
{
    auto* c = canvas();
    if (!c) return HOST_ERR_NOT_FOUND;
    return c->setText(widget_id, text) ? HOST_OK : HOST_ERR_NOT_FOUND;
}

int host_view_canvas_get_text(uint32_t widget_id, char* out, size_t cap)
{
    auto* c = canvas();
    if (!c) return HOST_ERR_NOT_FOUND;
    int n = c->getText(widget_id, out, cap);
    return n < 0 ? HOST_ERR_NOT_FOUND : n;
}

int host_view_canvas_set_focus(uint32_t widget_id)
{
    auto* c = canvas();
    if (!c) return HOST_ERR_NOT_FOUND;
    return c->setFocus(widget_id) ? HOST_OK : HOST_ERR_NOT_FOUND;
}

int host_view_canvas_get_focus(uint32_t* out)
{
    auto* c = canvas();
    if (!c || !out) return HOST_ERR_NOT_FOUND;
    *out = c->getFocus();
    return HOST_OK;
}

int host_view_canvas_set_key_repeat(uint16_t initial_ms, uint16_t repeat_ms)
{
    auto* c = canvas();
    if (!c) return HOST_ERR_NOT_FOUND;
    c->setKeyRepeat(initial_ms, repeat_ms);
    return HOST_OK;
}

}  // extern "C"
