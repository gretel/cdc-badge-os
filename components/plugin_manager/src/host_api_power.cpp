/**
 * \file host_api_power.cpp
 * \brief Real implementations of the power-state host API.
 */

#include "cdc_hal/IPowerManager.h"
#include "plugin_manager/host_api.h"

using cdc::hal::IPowerManager;
using cdc::hal::getPowerManagerInstance;

extern "C" {

uint16_t host_battery_mv(void)
{
    auto* p = getPowerManagerInstance();
    return p ? static_cast<uint16_t>(p->getBatteryVoltage()) : 0;
}

uint8_t host_battery_pct(void)
{
    auto* p = getPowerManagerInstance();
    return p ? p->getBatteryPercent() : 0;
}

bool host_is_usb_connected(void)
{
    auto* p = getPowerManagerInstance();
    return p ? p->isUsbConnected() : false;
}

uint8_t host_power_source(void)
{
    auto* p = getPowerManagerInstance();
    return p ? static_cast<uint8_t>(p->getPowerSource()) : POWER_SRC_UNKNOWN;
}

uint8_t host_charge_status(void)
{
    auto* p = getPowerManagerInstance();
    return p ? static_cast<uint8_t>(p->getChargeStatus()) : CHARGE_NOT_CHARGING;
}

bool host_is_battery_low(void)
{
    auto* p = getPowerManagerInstance();
    return p ? p->isBatteryLow() : false;
}

bool host_is_battery_critical(void)
{
    auto* p = getPowerManagerInstance();
    return p ? p->isBatteryCritical() : false;
}

}  // extern "C"
