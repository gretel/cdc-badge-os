/**
 * \file plugin_log_bridge.cpp
 * \brief cdc_log bridge for the plugin manager translation units that need
 *        to coexist with wasm_export.h.
 *
 * `wasm_export.h` and `cdc_log.h` both define `log_level_t`. Any source file
 * that wants both has to go through this bridge. It also tracks the currently
 * active Plugin pointer so the per-call capability checks in the host_api_*
 * implementations can look up which manifest applies.
 */

#include "cdc_log.h"

#include <atomic>

static const char* TAG = "PLUGIN";
static std::atomic<void*> s_active_plugin{nullptr};

extern "C" void plg_log_info(const char* msg)  { LOG_I(TAG, "%s", msg ? msg : ""); }
extern "C" void plg_log_warn(const char* msg)  { LOG_W(TAG, "%s", msg ? msg : ""); }
extern "C" void plg_log_error(const char* msg) { LOG_E(TAG, "%s", msg ? msg : ""); }

extern "C" void  plg_set_active_plugin(void* p) { s_active_plugin.store(p, std::memory_order_release); }
extern "C" void* plg_get_active_plugin(void)    { return s_active_plugin.load(std::memory_order_acquire); }
