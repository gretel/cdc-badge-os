/**
 * \file host_api_nvs.cpp
 * \brief Plugin-namespaced NVS key/value storage.
 *
 * Each plugin gets its own NVS namespace `plugin_<id>` derived from the
 * active plugin's manifest. Cross-plugin reads are physically impossible -
 * the namespace is selected by the host, not by the plugin.
 */

#include "plugin_manager/host_api.h"
#include "plugin_manager/Plugin.h"
#include "cdc_core/Raii.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <cstring>
#include <string>

extern "C" void* plg_get_active_plugin(void);
extern "C" void  plg_log_warn(const char* msg);

namespace {

std::string activeNamespace()
{
    auto* plugin = static_cast<cdc::plugin_manager::Plugin*>(plg_get_active_plugin());
    if (!plugin) return {};
    const auto& cap = plugin->manifest().capabilities;
    if (!cap.nvs_namespace.empty()) return cap.nvs_namespace;
    return std::string("plugin_") + plugin->id();
}

bool namespaceOk(const std::string& ns)
{
    constexpr size_t NVS_MAX = 15;
    return !ns.empty() && ns.size() <= NVS_MAX;
}

int openScope(bool write, ::cdc::core::NvsScope& out)
{
    std::string ns = activeNamespace();
    if (!namespaceOk(ns)) return HOST_ERR_NO_CAPABILITY;
    out = ::cdc::core::NvsScope(ns.c_str(), write ? NVS_READWRITE : NVS_READONLY);
    if (!out) {
        return (out.status() == ESP_ERR_NVS_NOT_FOUND) ? HOST_ERR_NOT_FOUND
                                                       : HOST_ERR_GENERIC;
    }
    return HOST_OK;
}

int translateErr(esp_err_t err)
{
    switch (err) {
        case ESP_OK:                       return HOST_OK;
        case ESP_ERR_NVS_NOT_FOUND:        return HOST_ERR_NOT_FOUND;
        case ESP_ERR_NVS_INVALID_LENGTH:   return HOST_ERR_NO_MEMORY;
        default:                           return HOST_ERR_GENERIC;
    }
}

int writeAndCommit(::cdc::core::NvsScope& h, esp_err_t writeErr)
{
    if (writeErr == ESP_OK) writeErr = h.commit();
    return translateErr(writeErr);
}

}  // namespace

extern "C" {

int host_nvs_get_blob(const char* key, uint8_t* buf, size_t* len)
{
    if (!key || !len) return HOST_ERR_INVALID_ARG;
    cdc::core::NvsScope h;
    int rc = openScope(false, h);
    if (rc != HOST_OK) return rc;
    return translateErr(nvs_get_blob(h, key, buf, len));
}

int host_nvs_set_blob(const char* key, const uint8_t* buf, size_t len)
{
    if (!key || (!buf && len > 0)) return HOST_ERR_INVALID_ARG;
    cdc::core::NvsScope h;
    int rc = openScope(true, h);
    if (rc != HOST_OK) return rc;
    return writeAndCommit(h, nvs_set_blob(h, key, buf, len));
}

int host_nvs_get_u32(const char* key, uint32_t* out)
{
    if (!key || !out) return HOST_ERR_INVALID_ARG;
    cdc::core::NvsScope h;
    int rc = openScope(false, h);
    if (rc != HOST_OK) return rc;
    return translateErr(nvs_get_u32(h, key, out));
}

int host_nvs_set_u32(const char* key, uint32_t value)
{
    if (!key) return HOST_ERR_INVALID_ARG;
    cdc::core::NvsScope h;
    int rc = openScope(true, h);
    if (rc != HOST_OK) return rc;
    return writeAndCommit(h, nvs_set_u32(h, key, value));
}

int host_nvs_get_str(const char* key, char* buf, size_t buf_size)
{
    if (!key || !buf || buf_size == 0) return HOST_ERR_INVALID_ARG;
    cdc::core::NvsScope h;
    int rc = openScope(false, h);
    if (rc != HOST_OK) return rc;
    size_t n = buf_size;
    return translateErr(nvs_get_str(h, key, buf, &n));
}

int host_nvs_set_str(const char* key, const char* value)
{
    if (!key || !value) return HOST_ERR_INVALID_ARG;
    cdc::core::NvsScope h;
    int rc = openScope(true, h);
    if (rc != HOST_OK) return rc;
    return writeAndCommit(h, nvs_set_str(h, key, value));
}

int host_nvs_erase(const char* key)
{
    if (!key) return HOST_ERR_INVALID_ARG;
    cdc::core::NvsScope h;
    int rc = openScope(true, h);
    if (rc != HOST_OK) return rc;
    return writeAndCommit(h, nvs_erase_key(h, key));
}

int host_nvs_erase_all(void)
{
    cdc::core::NvsScope h;
    int rc = openScope(true, h);
    if (rc != HOST_OK) return rc;
    return writeAndCommit(h, nvs_erase_all(h));
}

int host_nvs_list_keys(char* out, size_t* out_len)
{
    if (!out_len) return HOST_ERR_INVALID_ARG;
    std::string ns = activeNamespace();
    if (!namespaceOk(ns)) return HOST_ERR_NO_CAPABILITY;

    nvs_iterator_t it = nullptr;
    esp_err_t err = nvs_entry_find(NVS_DEFAULT_PART_NAME, ns.c_str(), NVS_TYPE_ANY, &it);
    size_t written = 0;
    const size_t max = (out ? *out_len : 0);
    while (err == ESP_OK && it) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        size_t need = std::strlen(info.key) + 1;
        if (out && (written + need) <= max) {
            std::memcpy(out + written, info.key, need - 1);
            out[written + need - 1] = '\n';
        }
        written += need;
        err = nvs_entry_next(&it);
    }
    nvs_release_iterator(it);
    *out_len = written;
    return HOST_OK;
}

}  // extern "C"
