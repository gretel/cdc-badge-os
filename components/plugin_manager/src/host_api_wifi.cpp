/**
 * \file host_api_wifi.cpp
 * \brief WiFi host API - reference-counted ensureConnected/disconnect.
 *
 * Plugins call host_wifi_request() to ask the host to establish a connection,
 * and host_wifi_release() to give it back. The host counts outstanding
 * requesters so concurrent users (PluginManager prerequisites + plugin code)
 * don't accidentally tear down the connection.
 *
 * The prerequisite system also calls into these functions, so a plugin that
 * declares `wifi_connected` as a prerequisite gets WiFi up before
 * `plugin_on_enter` is called - and does not need to call host_wifi_request
 * itself.
 */

#include "cdc_hal/IWifiController.h"
#include "cdc_os_ui/WifiHandlers.h"
#include "plugin_manager/host_api.h"

#include <cstring>

extern "C" void plg_log_warn(const char* msg);

namespace {

int s_refcount = 0;

cdc::hal::IWifiController* wifi() {
    return cdc::hal::getWifiControllerInstance();
}

}  // namespace

extern "C" {

int host_wifi_request(uint32_t /*timeout_ms*/)
{
    if (cdc::ui::WifiHandlers::instance().ensureConnected()) {
        ++s_refcount;
        return HOST_OK;
    }
    return HOST_ERR_TIMEOUT;
}

int host_wifi_release(void)
{
    if (s_refcount > 0) --s_refcount;
    if (s_refcount == 0) cdc::ui::WifiHandlers::instance().disconnect();
    return HOST_OK;
}

bool host_wifi_is_connected(void)
{
    return cdc::ui::WifiHandlers::instance().isConnected();
}

int host_wifi_ssid(char* out, size_t out_size)
{
    if (!out || out_size == 0) return HOST_ERR_INVALID_ARG;
    auto* w = wifi();
    if (!w) return HOST_ERR_NOT_FOUND;
    std::strncpy(out, w->getCurrentSsid(), out_size - 1);
    out[out_size - 1] = '\0';
    return HOST_OK;
}

int host_wifi_ip(char* out, size_t out_size)
{
    if (!out || out_size == 0) return HOST_ERR_INVALID_ARG;
    auto* w = wifi();
    if (!w) return HOST_ERR_NOT_FOUND;
    return w->getIpAddress(out, out_size) ? HOST_OK : HOST_ERR_GENERIC;
}

int8_t host_wifi_rssi(void)
{
    auto* w = wifi();
    return w ? w->getRssi() : 0;
}

int host_wifi_mac(uint8_t* out)
{
    if (!out) return HOST_ERR_INVALID_ARG;
    auto* w = wifi();
    if (!w) return HOST_ERR_NOT_FOUND;
    return w->getMacAddress(out) ? HOST_OK : HOST_ERR_GENERIC;
}

int host_wifi_start_scan(void)        { return HOST_ERR_NOT_SUPPORTED; }
bool host_wifi_scan_done(void)        { return false; }
int host_wifi_scan_results(wifi_scan_result_t*, size_t*) { return HOST_ERR_NOT_SUPPORTED; }

}  // extern "C"
