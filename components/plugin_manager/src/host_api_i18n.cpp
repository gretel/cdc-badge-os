/**
 * \file host_api_i18n.cpp
 * \brief Manifest-driven i18n lookups for the currently active plugin.
 *
 * The plugin manager registers a plugin's `i18n.meta` and `i18n.strings`
 * entries when the plugin is loaded. Plugin code asks for a translation by
 * key (`host_i18n_tr_key("save", buf, sizeof(buf))`) or for a metadata field
 * (`host_i18n_tr_meta("name", buf, sizeof(buf))`). The host copies the
 * resolved string into the caller-provided buffer (NUL-terminated when there
 * is room) and returns the number of bytes written excluding the NUL, or -1
 * on error.
 */

#include "plugin_manager/host_api.h"
#include "plugin_manager/Plugin.h"
#include "cdc_ui/I18n.h"
#include "cdc_log.h"

#include <cstring>

extern "C" void* plg_get_active_plugin(void);

namespace {

cdc::plugin_manager::Plugin* active()
{
    return static_cast<cdc::plugin_manager::Plugin*>(plg_get_active_plugin());
}

uint8_t lang_index()
{
    const std::string& code = cdc::ui::I18n::instance().getLanguageCode();
    if (code == "de") return HOST_LANG_DE;
    return HOST_LANG_EN;
}

const std::string* lookup_localised(
    const std::map<std::string, cdc::plugin_manager::LocalizedString>& table,
    const std::string& key, const std::string& default_lang)
{
    auto it = table.find(key);
    if (it == table.end()) return nullptr;
    uint8_t lang = lang_index();
    const char* lang_name = (lang == HOST_LANG_DE) ? "de" : "en";
    auto by_lang_it = it->second.by_lang.find(lang_name);
    if (by_lang_it != it->second.by_lang.end()) return &by_lang_it->second;
    by_lang_it = it->second.by_lang.find(default_lang);
    if (by_lang_it != it->second.by_lang.end()) return &by_lang_it->second;
    if (!it->second.by_lang.empty()) return &it->second.by_lang.begin()->second;
    return nullptr;
}

int copy_to_buffer(const char* src, char* out, uint32_t out_cap)
{
    if (!out || out_cap == 0) return -1;
    if (!src) src = "";
    size_t len = std::strlen(src);
    size_t to_copy = (len < out_cap - 1) ? len : (out_cap - 1);
    std::memcpy(out, src, to_copy);
    out[to_copy] = '\0';
    return static_cast<int>(to_copy);
}

}  // namespace

extern "C" {

int host_i18n_tr_key(const char* key, char* out, uint32_t out_cap)
{
    if (!out || out_cap == 0) return -1;
    auto* p = active();
    if (!p || !key) return copy_to_buffer("", out, out_cap);
    if (const char* s = p->trKey(key)) return copy_to_buffer(s, out, out_cap);
    const auto& mf = p->manifest();
    const std::string* s = lookup_localised(mf.i18n_strings, key, mf.default_language);
    return copy_to_buffer(s ? s->c_str() : "", out, out_cap);
}

int host_i18n_tr_meta(const char* field, char* out, uint32_t out_cap)
{
    if (!out || out_cap == 0) return -1;
    auto* p = active();
    if (!p || !field) {
        LOG_W("I18N", "tr_meta: no active plugin (%p) or field (%p)", p, field);
        return copy_to_buffer("", out, out_cap);
    }
    {
        std::string composed = "meta.";
        composed += field;
        if (const char* s = p->trKey(composed.c_str())) return copy_to_buffer(s, out, out_cap);
    }
    const auto& mf = p->manifest();
    const std::string* s = lookup_localised(mf.i18n_meta, field, mf.default_language);
    if (!s) {
        LOG_W("I18N", "tr_meta('%s'): no entry; meta size=%u, default_lang='%s'",
              field, static_cast<unsigned>(mf.i18n_meta.size()), mf.default_language.c_str());
    }
    return copy_to_buffer(s ? s->c_str() : "", out, out_cap);
}

uint8_t host_i18n_current_language(void)
{
    return lang_index();
}

int host_i18n_tr_core(const char* key, char* out, uint32_t out_cap)
{
    if (!out || out_cap == 0) return -1;
    if (!key) return copy_to_buffer("", out, out_cap);
    return copy_to_buffer(cdc::ui::I18n::instance().tr(key), out, out_cap);
}

}  // extern "C"
