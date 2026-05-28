/**
 * \file host_api_strings.cpp
 * \brief Thin WAMR-facing wrapper around cdc::ui::render::decodeWebText.
 */

#include "plugin_manager/host_api.h"
#include "cdc_views/RenderHelpers.h"

extern "C" int host_str_to_display(const char* in, char* out, size_t out_size, uint32_t target)
{
    if (!in || !out || out_size == 0) return HOST_ERR_INVALID_ARG;
    auto t = (target == HOST_STR_TARGET_LATIN1)
                 ? cdc::ui::render::DisplayTarget::Latin1
                 : cdc::ui::render::DisplayTarget::Cp437;
    cdc::ui::render::decodeWebText(in, out, out_size, t);
    return HOST_OK;
}
