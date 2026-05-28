/**
 * \file PluginInfoView.h
 * \brief Read-only summary of an installed plugin (manifest + size + SHA256).
 */

#pragma once

#include "cdc_views/InfoView.h"

#include <string>

namespace cdc::plugin_manager {

class PluginInfoView : public cdc::ui::InfoView {
public:
    bool loadForPluginId(const std::string& id);

private:
    std::string body_;
};

}  // namespace cdc::plugin_manager
