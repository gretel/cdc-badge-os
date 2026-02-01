#pragma once

#include "cdc_core/IService.h"
#include <cstdint>
#include <functional>

namespace cdc::hal {

/**
 * BLE scan result
 */
struct BleScanResult {
    char name[32];
    uint8_t mac[6];
    int8_t rssi;
};

/**
 * Bluetooth Controller Interface
 * Handles BLE stack initialization, power control, scanning and advertising
 */
class IBluetoothController : public core::IService {
public:
    virtual ~IBluetoothController() = default;

    // === Power Control ===

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

    // === Device Info ===

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

    // === Connection ===

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

    /**
     * Get connected device name
     * @param buf Output buffer
     * @param bufLen Buffer size
     * @return true if connected and name retrieved
     */
    virtual bool getConnectedDeviceName(char* buf, size_t bufLen) const { (void)buf; (void)bufLen; return false; }

    /**
     * Get number of bonded (paired) devices
     */
    virtual uint8_t getBondedDeviceCount() const { return 0; }

    // === Advertising ===

    /**
     * Start BLE advertising
     */
    virtual void startAdvertising() {}

    /**
     * Stop BLE advertising
     */
    virtual void stopAdvertising() {}

    /**
     * Check if currently advertising
     */
    virtual bool isAdvertising() const { return false; }

    // === Scanning ===

    /**
     * Start BLE scan
     * @param durationMs Scan duration in milliseconds
     * @return true if scan started
     */
    virtual bool startScan(uint32_t durationMs = 5000) { (void)durationMs; return false; }

    /**
     * Stop ongoing scan
     */
    virtual void stopScan() {}

    /**
     * Check if scan is complete
     */
    virtual bool isScanComplete() const { return true; }

    /**
     * Get scan results
     * @param results Output array
     * @param maxResults Maximum results to return
     * @return Number of results
     */
    virtual uint8_t getScanResults(BleScanResult* results, uint8_t maxResults) { (void)results; (void)maxResults; return 0; }

    // === Pairing Callbacks ===

    using PasskeyCallback = std::function<void(uint32_t passkey)>;
    using AuthCompleteCallback = std::function<void(bool success)>;

    /**
     * Set callback for passkey display during pairing
     */
    virtual void setPasskeyCallback(PasskeyCallback cb) { (void)cb; }

    /**
     * Set callback for authentication completion
     */
    virtual void setAuthCompleteCallback(AuthCompleteCallback cb) { (void)cb; }
};

// Factory function
IBluetoothController* getBluetoothControllerInstance();

} // namespace cdc::hal
