#pragma once

#include "cdc_core/IService.h"
#include <cstdint>

namespace cdc::hal {

/**
 * Bluetooth Controller Interface
 * Handles BLE stack initialization and power control
 */
class IBluetoothController : public core::IService {
public:
    virtual ~IBluetoothController() = default;

    /**
     * Enable Bluetooth (initialize BLE stack)
     * @return true if successfully enabled
     */
    virtual bool enable() = 0;

    /**
     * Disable Bluetooth (shutdown BLE stack to save power)
     */
    virtual void disable() = 0;

    /**
     * Check if Bluetooth is currently enabled
     */
    virtual bool isEnabled() const = 0;

    /**
     * Get current Bluetooth MAC address
     * @param mac Output buffer (6 bytes)
     * @return true if address retrieved successfully
     */
    virtual bool getMacAddress(uint8_t* mac) const = 0;

    /**
     * Set device name for BLE advertising
     * @param name Device name (null-terminated)
     */
    virtual void setDeviceName(const char* name) = 0;

    /**
     * Get device name
     */
    virtual const char* getDeviceName() const = 0;

    /**
     * Check if a device is currently connected
     */
    virtual bool isConnected() const = 0;

    /**
     * Disconnect current connection if any
     */
    virtual void disconnect() = 0;

    /**
     * Get RSSI of connected device
     * @return RSSI in dBm, or 0 if not connected
     */
    virtual int8_t getRssi() const = 0;
};

// Factory function
IBluetoothController* getBluetoothControllerInstance();

} // namespace cdc::hal
