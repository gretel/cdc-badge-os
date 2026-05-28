/**
 * \file LockscreenRegistry.h
 * \brief Internal registry of plugin lockscreen quick-actions.
 *
 * Owned by host_api_lockscreen.cpp; queried by PluginManager and the
 * lockscreen view. Plugins register through host_lockscreen_register_action.
 */

#pragma once

#include <cstdint>
#include <cstddef>

namespace cdc::plugin_manager {

struct LockscreenRegistration {
    void*    plugin    = nullptr;
    char     label_key[32] = {0};
    uint32_t action_id = 0;
    bool     used      = false;
};

uint8_t collectLockscreenItems(LockscreenRegistration* out, uint8_t max);
void    clearLockscreenRegistrationFor(void* plugin);

}  // namespace cdc::plugin_manager
