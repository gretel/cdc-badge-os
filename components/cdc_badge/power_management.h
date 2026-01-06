#pragma once

// BQ25895 Power Management for CDC Badge
// Handles charging, battery monitoring, and power button long-press shutdown.
//
// The BQ25895 has NO persistent storage - all settings must be applied on every boot.
// Fast Charge: Up to 1000mA (safe for 1200mAh LiPo battery, 0.83C rate)

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Charge current limits (mA)
#define CHARGE_CURRENT_MIN      64      // Minimum settable (1 step)
#define CHARGE_CURRENT_SLOW     512     // Conservative/slow charge
#define CHARGE_CURRENT_FAST     1000    // Fast charge (safe for 1200mAh battery)
#define CHARGE_CURRENT_MAX      1024    // Hardware max we allow (battery is 1200mAh)

// Charge status
typedef enum {
    CHARGE_STATUS_NOT_CHARGING = 0,
    CHARGE_STATUS_PRE_CHARGE,       // Trickle charge for deeply discharged battery
    CHARGE_STATUS_FAST_CHARGE,      // CC/CV phase
    CHARGE_STATUS_DONE              // Charge complete
} charge_status_t;

// VBUS status
typedef enum {
    VBUS_STATUS_NONE = 0,
    VBUS_STATUS_USB_SDP,            // USB Standard Downstream Port (500mA)
    VBUS_STATUS_USB_CDP,            // USB Charging Downstream Port (1.5A)
    VBUS_STATUS_USB_DCP,            // USB Dedicated Charging Port (1.5A+)
    VBUS_STATUS_HVDCP,              // High Voltage DCP
    VBUS_STATUS_UNKNOWN_ADAPTER,
    VBUS_STATUS_NON_STANDARD,
    VBUS_STATUS_OTG
} vbus_status_t;

// Initialize power management (BQ25895 + IRQs)
// Returns true on success
bool power_management_init(void);

// Process pending IRQs (call from main loop or periodically)
// Handles charger status changes and power button long-press
void power_management_process(void);

// Set charge current in mA (will be rounded to nearest 64mA step)
// Valid range: 64-1024mA
bool power_set_charge_current_ma(uint16_t current_ma);

// Get currently configured charge current in mA
uint16_t power_get_charge_current_ma(void);

// Enable/disable fast charge mode
// fast=true: 1000mA, fast=false: 512mA
void power_set_fast_charge(bool fast);

// Check if fast charge is enabled
bool power_is_fast_charge(void);

// Get battery voltage in millivolts (0 on error)
uint16_t power_get_battery_voltage_mv(void);

// Get battery percentage (0-100, linear approximation 3200-4200mV)
uint8_t power_get_battery_percent(void);

// Check if USB power is connected
bool power_is_usb_connected(void);

// Get VBUS status
vbus_status_t power_get_vbus_status(void);

// Check if battery is charging
bool power_is_charging(void);

// Get detailed charge status
charge_status_t power_get_charge_status(void);

// Enter shipping mode (battery disconnect, only USB keeps system running)
// System will power off when USB is removed
bool power_enter_shipping_mode(void);

// Prepare GPIO before entering sleep (light or deep)
// Disables interrupt at hardware level to avoid conflicts during wakeup
void power_prepare_gpio_for_sleep(void);

// Stabilize GPIO after sleep wakeup
// Waits for key release, restores edge-trigger, re-enables interrupt
// Call after light sleep wakeup or after pin_expander_init when waking from deep sleep
void power_stabilize_gpio_after_sleep(void);

#ifdef __cplusplus
}
#endif
