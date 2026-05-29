/**
 * \file host_api_cmd.cpp
 * \brief Plugin command channel - the plugin pulls a host-pushed command
 *        string buffered by PluginManager during a plugin_on_cmd dispatch.
 */

#include "plugin_manager/host_api.h"
#include "plugin_manager/PluginManager.h"

extern "C" {

int host_cmd_consume(char* out, size_t out_size)
{
    return cdc::plugin_manager::PluginManager::instance().consumeCmd(out, out_size);
}

}  // extern "C"
