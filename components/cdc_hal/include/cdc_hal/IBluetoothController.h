#pragma once

#include "cdc_core/IService.h"
#include <cstdint>
#include <cstring>
#include <functional>

namespace cdc::hal {

/**
 * BLE scan result with raw advertising data for module-specific parsing
 */
struct BleScanResult {
    char name[32];
    uint8_t mac[6];
    uint8_t addrType;          // 0=public, 1=random
    int8_t rssi;
    uint8_t advData[31];       // Raw advertising data (AD structures)
    uint8_t advDataLen;
};

/**
 * Stack-independent BLE UUID (no NimBLE/Bluedroid dependency)
 */
struct BleUuid {
    enum Type : uint8_t { UUID_16 = 0, UUID_128 = 1 };
    Type type;
    union {
        uint16_t u16;
        uint8_t u128[16];
    };

    static BleUuid from16(uint16_t v) {
        BleUuid u;
        u.type = UUID_16;
        u.u16 = v;
        std::memset(u.u128, 0, sizeof(u.u128)); // zero padding
        return u;
    }

    static BleUuid from128(const uint8_t v[16]) {
        BleUuid u;
        u.type = UUID_128;
        std::memcpy(u.u128, v, 16);
        return u;
    }

    bool operator==(const BleUuid& other) const {
        if (type != other.type) return false;
        if (type == UUID_16) return u16 == other.u16;
        return std::memcmp(u128, other.u128, 16) == 0;
    }
};

/**
 * Stack-independent GATT characteristic property flags
 * Values match BLE spec (Vol 3, Part G, 3.3.1.1)
 */
namespace GattProp {
    constexpr uint8_t READ         = 0x02;
    constexpr uint8_t WRITE_NO_RSP = 0x04;
    constexpr uint8_t WRITE        = 0x08;
    constexpr uint8_t NOTIFY       = 0x10;
    constexpr uint8_t INDICATE     = 0x20;
}

/**
 * Stack-independent GATT attribute permission flags
 */
namespace GattPerm {
    constexpr uint8_t READ         = 0x01;
    constexpr uint8_t WRITE        = 0x02;
    constexpr uint8_t READ_ENC     = 0x04;  // Requires encryption (pairing)
    constexpr uint8_t WRITE_ENC    = 0x08;  // Requires encryption (pairing)
}

/**
 * GATT write callback for characteristic writes
 */
using GattWriteCallback = std::function<int(uint16_t connHandle, uint16_t attrHandle,
                                             const uint8_t* data, uint16_t len)>;

/**
 * GATT read callback for characteristic reads
 */
using GattReadCallback = std::function<int(uint16_t connHandle, uint16_t attrHandle,
                                            uint8_t* buf, uint16_t* len)>;

/**
 * GATT characteristic definition for service registration
 */
struct GattCharacteristic {
    BleUuid uuid;
    uint8_t properties;        // GattProp flags
    uint8_t permissions;       // GattPerm flags
    uint16_t* valueHandle;     // Output: handle assigned by stack
    GattWriteCallback onWrite;
    GattReadCallback onRead;
};

/**
 * GATT service definition for registration via registerGattService()
 */
struct GattServiceDef {
    BleUuid uuid;
    GattCharacteristic* characteristics;
    uint8_t numCharacteristics;
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

    /**
     * Register a service UUID to include in advertising scan response.
     * Enables remote devices to discover services before connecting.
     * @param uuid Service UUID to advertise
     * @return true if registered (max 4 UUIDs)
     */
    virtual bool addAdvertisingUuid(const BleUuid& uuid) { (void)uuid; return false; }

    /**
     * Remove a service UUID from advertising scan response.
     * @param uuid Service UUID to remove
     */
    virtual void removeAdvertisingUuid(const BleUuid& uuid) { (void)uuid; }

    /**
     * Set manufacturer-specific data in advertising scan response.
     * Used for module-specific broadcast data (e.g., vCard minicard).
     * @param companyId Bluetooth company ID (0xFFFF for testing)
     * @param data Payload data
     * @param len Payload length (max ~27 bytes)
     * @return true if set successfully
     */
    virtual bool setAdvertisingManufacturerData(uint16_t companyId,
                                                 const uint8_t* data, uint16_t len) {
        (void)companyId; (void)data; (void)len;
        return false;
    }

    /**
     * Clear manufacturer data from advertising
     */
    virtual void clearAdvertisingManufacturerData() {}

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

    /**
     * Numeric comparison callback for secure pairing.
     * Called when a connecting device requires confirmation.
     * Display the passkey and let the user accept/reject.
     */
    using NumericComparisonCallback = std::function<void(uint16_t connHandle, uint32_t passkey)>;

    /**
     * Set callback for numeric comparison during pairing
     */
    virtual void setNumericComparisonCallback(NumericComparisonCallback cb) { (void)cb; }

    /**
     * Respond to a numeric comparison pairing request
     * @param connHandle Connection handle from the callback
     * @param accept true to accept, false to reject
     */
    virtual void respondToNumericComparison(uint16_t connHandle, bool accept) {
        (void)connHandle; (void)accept;
    }

    // === Connection Callbacks (multi-listener) ===

    using ConnectionCallback = std::function<void(uint16_t connHandle)>;
    using DisconnectionCallback = std::function<void(uint16_t connHandle, int reason)>;

    /**
     * Register a callback for device connection events.
     * Multiple callbacks are supported (up to 4).
     */
    virtual void addConnectionCallback(ConnectionCallback cb) { (void)cb; }

    /**
     * Register a callback for device disconnection events.
     * Multiple callbacks are supported (up to 4).
     */
    virtual void addDisconnectionCallback(DisconnectionCallback cb) { (void)cb; }

    // === GATT Server ===

    /**
     * Register a GATT service with characteristics.
     * Service definitions are translated to stack-native format internally.
     * Must be called after enable() and before advertising.
     * @param service Service definition (struct must remain valid until unregistered)
     * @return true if successfully registered
     */
    virtual bool registerGattService(const GattServiceDef& service) {
        (void)service;
        return false;
    }

    /**
     * Send notification on a characteristic
     * @param connHandle Connection handle (0xFFFF for all connections)
     * @param attrHandle Attribute handle of the characteristic
     * @param data Data to send
     * @param len Data length
     * @return true if notification sent
     */
    virtual bool sendNotification(uint16_t connHandle, uint16_t attrHandle,
                                  const uint8_t* data, uint16_t len) {
        (void)connHandle; (void)attrHandle; (void)data; (void)len;
        return false;
    }

    /**
     * Send indication on a characteristic (with acknowledgment)
     * @param connHandle Connection handle
     * @param attrHandle Attribute handle
     * @param data Data to send
     * @param len Data length
     * @return true if indication sent
     */
    virtual bool sendIndication(uint16_t connHandle, uint16_t attrHandle,
                                const uint8_t* data, uint16_t len) {
        (void)connHandle; (void)attrHandle; (void)data; (void)len;
        return false;
    }

    /**
     * Get negotiated MTU for current connection (payload size)
     * @return Usable payload size (MTU - 3 for ATT overhead), or 20 if not connected
     */
    virtual uint16_t getMtu() const { return 20; }

    // === Central Role (GATT Client) ===

    /**
     * Connect to a peripheral device
     * @param addr BLE address (6 bytes)
     * @param addrType Address type (0=public, 1=random)
     * @return true if connection initiated
     */
    virtual bool connect(const uint8_t* addr, uint8_t addrType = 0) {
        (void)addr; (void)addrType;
        return false;
    }

    /**
     * Discover a specific service by UUID on connected device.
     * Results delivered via ServiceDiscoveryCallback with discovered characteristics.
     * @param connHandle Connection handle
     * @param uuid Service UUID to discover
     * @return true if discovery started
     */
    virtual bool discoverServiceByUuid(uint16_t connHandle, const BleUuid& uuid) {
        (void)connHandle; (void)uuid;
        return false;
    }

    /**
     * Write to a characteristic on remote device
     * @param connHandle Connection handle
     * @param attrHandle Attribute handle
     * @param data Data to write
     * @param len Data length
     * @param withResponse true for write with response
     * @return true if write initiated
     */
    virtual bool writeCharacteristic(uint16_t connHandle, uint16_t attrHandle,
                                     const uint8_t* data, uint16_t len,
                                     bool withResponse = true) {
        (void)connHandle; (void)attrHandle; (void)data; (void)len; (void)withResponse;
        return false;
    }

    /**
     * Read a characteristic from remote device
     * @param connHandle Connection handle
     * @param attrHandle Attribute handle
     * @return true if read initiated
     */
    virtual bool readCharacteristic(uint16_t connHandle, uint16_t attrHandle) {
        (void)connHandle; (void)attrHandle;
        return false;
    }

    /**
     * Enable notifications on a remote characteristic
     * @param connHandle Connection handle
     * @param cccdHandle CCCD handle (usually attrHandle + 1)
     * @return true if successful
     */
    virtual bool enableNotifications(uint16_t connHandle, uint16_t cccdHandle) {
        (void)connHandle; (void)cccdHandle;
        return false;
    }

    /**
     * Disconnect a specific connection (central or peripheral)
     * @param connHandle Connection handle to disconnect
     */
    virtual void disconnectHandle(uint16_t connHandle) { (void)connHandle; }

    // === Central Role Callbacks ===

    /**
     * Discovered characteristic from service discovery
     */
    struct DiscoveredCharacteristic {
        BleUuid uuid;
        uint16_t valueHandle;
        uint8_t properties;  // GattProp flags
    };

    /**
     * Discovered service from service discovery
     */
    struct DiscoveredService {
        BleUuid uuid;
        static constexpr uint8_t MAX_DISCOVERED_CHARS = 8;
        DiscoveredCharacteristic characteristics[MAX_DISCOVERED_CHARS];
        uint8_t numCharacteristics;
    };

    /**
     * Called when service discovery completes.
     * @param connHandle Connection handle
     * @param service Discovered service (null if discovery failed)
     * @param complete true when discovery is finished
     */
    using ServiceDiscoveryCallback = std::function<void(uint16_t connHandle,
                                                         const DiscoveredService* service,
                                                         bool complete)>;

    /**
     * Called when characteristic read completes
     */
    using CharacteristicReadCallback = std::function<void(uint16_t connHandle,
                                                           uint16_t attrHandle,
                                                           const uint8_t* data, uint16_t len)>;

    /**
     * Called when notification/indication received from remote device
     */
    using NotificationCallback = std::function<void(uint16_t connHandle,
                                                      uint16_t attrHandle,
                                                      const uint8_t* data, uint16_t len)>;

    /**
     * Called when write to remote characteristic completes
     */
    using WriteCompleteCallback = std::function<void(uint16_t connHandle,
                                                       uint16_t attrHandle,
                                                       int status)>;

    virtual void setServiceDiscoveryCallback(ServiceDiscoveryCallback cb) { (void)cb; }
    virtual void setCharacteristicReadCallback(CharacteristicReadCallback cb) { (void)cb; }
    virtual void setNotificationCallback(NotificationCallback cb) { (void)cb; }
    virtual void setWriteCompleteCallback(WriteCompleteCallback cb) { (void)cb; }

    /**
     * Get connection handle for current peripheral connection
     * @return Connection handle or 0xFFFF if not connected
     */
    virtual uint16_t getConnectionHandle() const { return 0xFFFF; }
};

// Factory function
IBluetoothController* getBluetoothControllerInstance();

/**
 * Stack-independent BLE advertising data parser.
 * Parses raw AD structures per BLE Core Spec Vol 3, Part C, Section 11.
 */
namespace BleAdvParser {
    /**
     * Find manufacturer-specific data in advertising data
     * @return true if found
     */
    bool findManufacturerData(const uint8_t* advData, uint8_t len,
                               uint16_t* companyId, const uint8_t** data, uint8_t* dataLen);

    /**
     * Check if a 128-bit service UUID is advertised
     * @return true if found
     */
    bool findServiceUuid128(const uint8_t* advData, uint8_t len,
                              const uint8_t uuid128[16]);

    /**
     * Extract device name from advertising data
     * @return true if found
     */
    bool findName(const uint8_t* advData, uint8_t len,
                    char* name, uint8_t nameMaxLen);
}

} // namespace cdc::hal
