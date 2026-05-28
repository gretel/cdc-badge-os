/**
 * \file host_api_lockscreen.cpp
 * \brief Lockscreen quick-action registry exposed to plugins.
 *
 * Plugins call host_lockscreen_register_action() during plugin_init to add
 * a single item to the lockscreen's context menu. The label is resolved
 * via i18n at render time so it follows the active language without
 * re-registration.
 */

#include "plugin_manager/host_api.h"
#include "plugin_manager/Plugin.h"
#include "plugin_manager/LockscreenRegistry.h"
#include "plugin_manager/SlotTable.h"

#include <cstring>

extern "C" void* plg_get_active_plugin(void);

namespace cdc::plugin_manager {

namespace {

constexpr size_t MAX_LOCKSCREEN_ITEMS = 8;
SlotTable<LockscreenRegistration, MAX_LOCKSCREEN_ITEMS> s_items{};

LockscreenRegistration* slotFor(void* plugin)
{
    for (auto& s : s_items.slots) if (s.used && s.plugin == plugin) return &s;
    return nullptr;
}

}  // namespace

uint8_t collectLockscreenItems(LockscreenRegistration* out, uint8_t max)
{
    uint8_t n = 0;
    for (auto& s : s_items.slots) {
        if (!s.used) continue;
        if (n >= max) break;
        out[n++] = s;
    }
    return n;
}

void clearLockscreenRegistrationFor(void* plugin)
{
    if (auto* slot = slotFor(plugin)) {
        *slot = LockscreenRegistration{};
    }
}

}  // namespace cdc::plugin_manager

extern "C" {

int host_lockscreen_register_action(const char* label_key, uint32_t action_id)
{
    auto* plugin = plg_get_active_plugin();
    if (!plugin)      return HOST_ERR_NO_CAPABILITY;
    if (!label_key)   return HOST_ERR_INVALID_ARG;

    using cdc::plugin_manager::LockscreenRegistration;

    LockscreenRegistration* slot = nullptr;
    for (auto& s : cdc::plugin_manager::s_items.slots) {
        if (s.used && s.plugin == plugin) { slot = &s; break; }
    }
    if (!slot) {
        int slot_id = 0;
        slot = cdc::plugin_manager::s_items.allocate(slot_id);
        if (!slot) return HOST_ERR_NO_MEMORY;
    }
    slot->plugin    = plugin;
    slot->action_id = action_id;
    std::strncpy(slot->label_key, label_key, sizeof(slot->label_key) - 1);
    slot->label_key[sizeof(slot->label_key) - 1] = '\0';
    slot->used = true;
    return HOST_OK;
}

int host_lockscreen_unregister_action(void)
{
    auto* plugin = plg_get_active_plugin();
    if (!plugin) return HOST_ERR_NO_CAPABILITY;
    cdc::plugin_manager::clearLockscreenRegistrationFor(plugin);
    return HOST_OK;
}

}  // extern "C"
