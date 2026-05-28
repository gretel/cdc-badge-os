/**
 * \file host_api_log.cpp
 * \brief Real implementation of host_log / host_log_hex.
 *
 * The remaining host_* functions still return HOST_ERR_NOT_SUPPORTED through
 * host_api_stubs.cpp. Each Phase 3 subsystem peels the relevant family out
 * of the stub file and lands its own implementation here.
 */

#include "plugin_manager/host_api.h"
#include "cdc_log.h"

#include <cstdio>

extern "C" {

void host_log(uint8_t level, const char* tag, const char* msg)
{
    if (!tag) tag = "plugin";
    if (!msg) msg = "";
    switch (level) {
        case LOG_LEVEL_ERROR:   LOG_E(tag, "%s", msg); break;
        case LOG_LEVEL_WARN:    LOG_W(tag, "%s", msg); break;
        case LOG_LEVEL_INFO:    LOG_I(tag, "%s", msg); break;
        case LOG_LEVEL_DEBUG:   LOG_D(tag, "%s", msg); break;
        default:                LOG_V(tag, "%s", msg); break;
    }
}

void host_log_hex(const char* tag, const char* label, const uint8_t* data, size_t len)
{
    if (!tag)   tag   = "plugin";
    if (!label) label = "";
    LOG_I(tag, "%s (%zu bytes)", label, len);
    char line[80];
    size_t off = 0;
    while (off < len) {
        size_t chunk = (len - off) > 16 ? 16 : (len - off);
        int written = 0;
        for (size_t i = 0; i < chunk; ++i) {
            written += std::snprintf(line + written, sizeof(line) - written,
                                     "%02X ", data[off + i]);
        }
        LOG_I(tag, "  %04zu  %s", off, line);
        off += chunk;
    }
}

}  // extern "C"
