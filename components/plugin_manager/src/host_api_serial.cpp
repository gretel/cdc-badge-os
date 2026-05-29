/**
 * \file host_api_serial.cpp
 * \brief PLUGIN CMD argument buffer consumed by host_serial_consume_args.
 */

#include "plugin_manager/host_api.h"
#include <cstring>

namespace cdc::plugin_manager {

static char  s_cmd_buf[256];
static bool  s_cmd_pending = false;

void plugin_cmd_set_args(const char* args) {
    if (!args || !*args) {
        s_cmd_buf[0] = '\0';
        s_cmd_pending = false;
        return;
    }
    std::strncpy(s_cmd_buf, args, sizeof(s_cmd_buf) - 1);
    s_cmd_buf[sizeof(s_cmd_buf) - 1] = '\0';
    s_cmd_pending = true;
}

}  // namespace cdc::plugin_manager

extern "C" int host_serial_consume_args(char* buf, size_t buf_size) {
    if (!buf || buf_size == 0) return 0;
    if (!cdc::plugin_manager::s_cmd_pending) {
        buf[0] = '\0';
        return 0;
    }
    size_t len = std::strlen(cdc::plugin_manager::s_cmd_buf);
    if (len >= buf_size) len = buf_size - 1;
    std::memcpy(buf, cdc::plugin_manager::s_cmd_buf, len);
    buf[len] = '\0';
    cdc::plugin_manager::s_cmd_pending = false;
    return static_cast<int>(len);
}
