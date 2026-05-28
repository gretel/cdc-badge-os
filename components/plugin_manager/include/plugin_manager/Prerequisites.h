/**
 * \file Prerequisites.h
 * \brief Walks the prerequisites list of a plugin manifest before plugin_on_enter.
 *
 * Each prerequisite either checks a precondition (`time_synced`,
 * `battery_min`, `unlocked`, ...) or actively brings up a resource
 * (`wifi_connected`, `ble_active`). Successfully-acquired resources are
 * tracked on the Plugin so PluginManager can release them in reverse order
 * during stopPlugin / on_exit.
 */

#pragma once

#include "plugin_manager/Plugin.h"

namespace cdc::plugin_manager {

enum class PrereqResult {
    Ok,
    SoftFailed,    // on_fail = warn or callback - host may continue
    HardFailed,    // on_fail = abort - host must not start plugin
};

class Prerequisites {
public:
    /**
     * \brief Walk the plugin's prerequisite list in order. Marks acquired
     *        resources on the Plugin so release() can undo them later.
     * \param out_failed_name Set to the failing prerequisite name if !=Ok.
     * \param out_on_fail     Filled with the on_fail string for caller dispatch.
     */
    [[nodiscard]] static PrereqResult walk(Plugin& plugin,
                                           std::string& out_failed_name,
                                           std::string& out_on_fail);

    /**
     * \brief Release every resource the plugin acquired during walk(),
     *        in reverse order of acquisition.
     */
    static void release(Plugin& plugin);
};

}  // namespace cdc::plugin_manager
