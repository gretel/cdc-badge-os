/**
 * \file host_api_ble.cpp
 * \brief BLE host API - read-only state queries for plugins.
 *
 * GATT server/client registration, pairing, and scan are intentionally
 * deferred: they need a per-plugin handle registry and capability-checked
 * UUID whitelist that would otherwise let a plugin shadow the system's
 * vCard / HID / serial services. Until that is built, plugins can only
 * read whether BLE is up and inspect MAC/name/RSSI.
 */

#include "cdc_hal/IBluetoothController.h"
#include "plugin_manager/host_api.h"

#include <cstring>

using cdc::hal::IBluetoothController;
using cdc::hal::getBluetoothControllerInstance;

extern "C" {

bool host_ble_is_enabled(void)
{
    auto* b = getBluetoothControllerInstance();
    return b ? b->isEnabled() : false;
}

int host_ble_mac(uint8_t out[6])
{
    if (!out) return HOST_ERR_INVALID_ARG;
    auto* b = getBluetoothControllerInstance();
    if (!b) return HOST_ERR_NOT_FOUND;
    return b->getMacAddress(out) ? HOST_OK : HOST_ERR_GENERIC;
}

int host_ble_device_name(char* out, size_t out_size)
{
    if (!out || out_size == 0) return HOST_ERR_INVALID_ARG;
    auto* b = getBluetoothControllerInstance();
    if (!b) return HOST_ERR_NOT_FOUND;
    const char* name = b->getDeviceName();
    std::strncpy(out, name ? name : "", out_size - 1);
    out[out_size - 1] = '\0';
    return HOST_OK;
}

int8_t host_ble_rssi(void)
{
    auto* b = getBluetoothControllerInstance();
    return b ? b->getRssi() : 0;
}

}  // extern "C"
