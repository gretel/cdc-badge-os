/**
 * \file PluginSerialCommands.h
 * \brief Serial console PLUGIN command bundle. Call once from main.cpp
 *        after PluginManager::init() to expose LIST/INFO/UPLOAD/DELETE/
 *        START/STOP via USB-CDC.
 */

#pragma once

namespace cdc::plugin_manager {

void registerPluginSerialCommands();

}  // namespace cdc::plugin_manager
