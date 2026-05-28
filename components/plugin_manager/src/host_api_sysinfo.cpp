/**
 * \file host_api_sysinfo.cpp
 * \brief Firmware identity / feature flags. Plugins use these to gate on
 *        the runtime they are loaded into - e.g. show a different UI for
 *        debug builds, or refuse to run on incompatible firmware revisions.
 */

#include "plugin_manager/host_api.h"
#include "cdc_core/feature_flags.h"

#include <cstdio>
#include <cstring>

#ifndef APP_NAME
#define APP_NAME "CDCBos"
#endif
#ifndef APP_VERSION
#define APP_VERSION "unknown"
#endif

extern "C" {

int host_get_firmware_version(char* out, size_t out_size)
{
    if (!out || out_size == 0) return HOST_ERR_INVALID_ARG;
    std::snprintf(out, out_size, "%s %s", APP_NAME, APP_VERSION);
    return HOST_OK;
}

int host_get_build_profile(char* out, size_t out_size)
{
    if (!out || out_size == 0) return HOST_ERR_INVALID_ARG;
    // BUILD_PROFILE_BYTE is a compile-time constant in feature_flags.h.
    std::snprintf(out, out_size, "0x%02X", BUILD_PROFILE_BYTE);
    return HOST_OK;
}

bool host_feature_enabled(uint16_t feature_id)
{
    // Numeric IDs are loosely allocated:
    //  1 = FEATURE_USB, 2 = FEATURE_WIFI, 3 = FEATURE_BLE,
    //  4 = FEATURE_FIDO2, 5 = FEATURE_TOTP, 6 = FEATURE_GPG, 7 = DEBUG_MODE
    switch (feature_id) {
#ifdef FEATURE_USB
        case 1: return FEATURE_USB;
#endif
#ifdef FEATURE_WIFI
        case 2: return FEATURE_WIFI;
#endif
#ifdef FEATURE_BLE
        case 3: return FEATURE_BLE;
#endif
#ifdef FEATURE_FIDO2
        case 4: return FEATURE_FIDO2;
#endif
#ifdef FEATURE_TOTP
        case 5: return FEATURE_TOTP;
#endif
#ifdef FEATURE_GPG
        case 6: return FEATURE_GPG;
#endif
#ifdef DEBUG_MODE
        case 7: return DEBUG_MODE;
#endif
        default: return false;
    }
}

}  // extern "C"
