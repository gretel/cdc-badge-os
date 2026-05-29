#include "plugin_manager/PluginInfoView.h"
#include "plugin_manager/PluginManager.h"
#include "plugin_manager/PluginStorage.h"

#include <sys/stat.h>
#include <cstdio>
#include "esp_attr.h"

namespace cdc::plugin_manager {

static std::string default_lang_value(const std::map<std::string, std::string>& by_lang,
                                      const std::string& dflt_lang)
{
    if (by_lang.empty()) return {};
    auto it = by_lang.find(dflt_lang);
    if (it != by_lang.end()) return it->second;
    return by_lang.begin()->second;
}

bool PluginInfoView::loadForPluginId(const std::string& id)
{
    auto mf = PluginManager::instance().getManifest(id);
    if (!mf) return false;

    std::string name = id;
    std::string desc;
    if (auto it = mf->i18n_meta.find("name"); it != mf->i18n_meta.end()) {
        name = default_lang_value(it->second.by_lang, mf->default_language);
    }
    if (auto it = mf->i18n_meta.find("description"); it != mf->i18n_meta.end()) {
        desc = default_lang_value(it->second.by_lang, mf->default_language);
    }

    struct stat st;
    long wasm_bytes = 0;
    if (stat(PluginStorage::wasmPath(id).c_str(), &st) == 0) {
        wasm_bytes = static_cast<long>(st.st_size);
    }

    EXT_RAM_BSS_ATTR static char buf[512];
    std::snprintf(buf, sizeof(buf),
                  "%s\n\nVersion: %s\nAuthor:  %s\nAPI:     %s\nMemory:  %u KB\nWASM:    %ld B\n\n%s",
                  name.c_str(),
                  mf->version.c_str(),
                  mf->author.c_str(),
                  mf->host_api_level_min.c_str(),
                  static_cast<unsigned>(mf->linear_memory_kb),
                  wasm_bytes,
                  desc.c_str());

    body_ = buf;
    init(name.c_str(), body_.c_str());
    return true;
}

}  // namespace cdc::plugin_manager
