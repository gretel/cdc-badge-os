/**
 * \file GpioSerialCommands.h
 * \brief Native serial commands for direct GPIO / ADC / I2C poking.
 *        Independent of plugins - the user controls the badge from a USB-CDC
 *        terminal. Same hard block list as the plugin GPIO API protects
 *        firmware-internal hardware.
 */

#pragma once

namespace cdc::plugin_manager {

void registerGpioSerialCommands();

}  // namespace cdc::plugin_manager
