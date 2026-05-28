#include "plugin_manager/PluginListView.h"
#include "plugin_manager/PluginManager.h"
#include "cdc_views/ToastView.h"
#include "cdc_log.h"

#include <cstdio>

namespace cdc::plugin_manager {

static const char* TAG = "PLG_UI";
static PluginListView* s_active = nullptr;

PluginListView* PluginListView::active() noexcept { return s_active; }

PluginListView::PluginListView() = default;

void PluginListView::onEnter(void* /*context*/)
{
    s_active = this;
    list_.setOnSelect(&PluginListView::onSelectStatic);
    list_.setOnMenu  (&PluginListView::onMenuStatic);
    rebuildItems();
}

void PluginListView::onExit()
{
    if (s_active == this) s_active = nullptr;
}

void PluginListView::onResume()
{
    s_active = this;
    PluginManager::instance().requestStopActivePlugin();
    rebuildItems();
    list_.markDirty();
}

void PluginListView::render(bool partial)
{
    list_.render(partial);
}

cdc::ui::InputResult PluginListView::onKey(char key)
{
    return list_.onKey(key);
}

const char* PluginListView::getFooterHint() const
{
    return "Y=Start  3=Menu  N=Back";
}

void PluginListView::rebuildItems()
{
    ids_     = PluginManager::instance().listInstalledIds();
    labels_.clear();
    items_.clear();
    labels_.reserve(ids_.size());
    items_.reserve(ids_.size());

    for (const auto& id : ids_) {
        std::string display = id;
        if (auto mf = PluginManager::instance().getManifest(id)) {
            auto it = mf->i18n_meta.find("name");
            if (it != mf->i18n_meta.end() && !it->second.by_lang.empty()) {
                auto def = it->second.by_lang.find(mf->default_language);
                if (def != it->second.by_lang.end()) {
                    display = def->second;
                } else {
                    display = it->second.by_lang.begin()->second;
                }
            }
        }
        labels_.push_back(std::move(display));
        items_.push_back(cdc::ui::ListItem{labels_.back().c_str(), 0, false, nullptr});
    }

    list_.init("Plugins", items_.data(), static_cast<uint16_t>(items_.size()));
    list_.setEmptyText("No plugins installed");
}

void PluginListView::onSelectStatic(uint16_t index, void*)
{
    if (s_active) s_active->onSelect(index);
}

void PluginListView::onMenuStatic(uint16_t index, void*)
{
    if (s_active) s_active->onMenu(index);
}

void PluginListView::onSelect(uint16_t index)
{
    if (index >= ids_.size()) return;
    const auto& id = ids_[index];
    LOG_I(TAG, "start plugin %s", id.c_str());

    auto result = PluginManager::instance().startPlugin(id);
    if (result != StartResult::Ok) {
        static char msg[64];
        const char* label = "Start failed";
        switch (result) {
            case StartResult::PluginAlreadyRunning: label = "Already running"; break;
            case StartResult::ManifestInvalid:      label = "Manifest invalid"; break;
            case StartResult::CapabilityRejected:   label = "Capabilities rejected"; break;
            case StartResult::WamrLoadFailed:       label = "WASM load failed"; break;
            case StartResult::PluginInitFailed:     label = "plugin_init failed"; break;
            case StartResult::PrerequisiteFailed:   label = "Prerequisite failed"; break;
            case StartResult::PluginOnEnterFailed:  label = "plugin_on_enter missing"; break;
            default: break;
        }
        std::snprintf(msg, sizeof(msg), "%s\nErr %d", label, static_cast<int>(result));
        cdc::ui::showToastError(msg, 3000);
    }
}

void PluginListView::onMenu(uint16_t /*index*/)
{
    cdc::ui::showToastInfo("Context menu coming soon", 1200);
}

}  // namespace cdc::plugin_manager
