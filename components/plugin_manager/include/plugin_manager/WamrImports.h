/**
 * \file WamrImports.h
 * \brief Registers the host API as WAMR native imports under module "cdc".
 *
 * Call once from PluginManager::init() after the WAMR runtime is up. WAMR
 * looks these up by name when instantiating a plugin's module. Unresolved
 * imports cause instantiation to fail with a clear error.
 */

#pragma once

namespace cdc::plugin_manager {

/** \brief Register the "cdc" import namespace with WAMR. */
bool register_host_imports();

/** \brief Unregister the imports (called from PluginManager::deinit()). */
void unregister_host_imports();

}  // namespace cdc::plugin_manager
