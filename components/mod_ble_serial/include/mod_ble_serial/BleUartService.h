#pragma once

#include "cdc_hal/IBluetoothController.h"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <functional>

namespace cdc::mod_ble_serial {

/**
 * BLE UART Service (Nordic UART Service compatible)
 *
 * Provides serial communication over BLE using IBluetoothController API only.
 * - Service UUID: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
 * - RX UUID: 6E400002-... (Write, Write Without Response)
 * - TX UUID: 6E400003-... (Notify)
 */
class BleUartService {
public:
    /**
     * Initialize the GATT service via IBluetoothController API
     * @return true if successful
     */
    bool init();

    /**
     * Deinitialize the GATT service
     */
    void deinit();

    /**
     * Check if service is initialized
     */
    bool isInitialized() const { return initialized_; }

    // === TX (Badge -> Phone) ===

    /**
     * Send data to connected device
     * @param data Data to send
     * @param len Data length
     * @return Number of bytes sent
     */
    size_t send(const uint8_t* data, size_t len);

    /**
     * Send string to connected device
     * @param str Null-terminated string
     * @return Number of bytes sent
     */
    size_t send(const char* str);

    /**
     * Check if TX is ready (not congested)
     */
    bool txReady() const;

    // === RX (Phone -> Badge) ===

    /**
     * Check if data is available to read
     * @return Number of bytes available
     */
    size_t available() const;

    /**
     * Read single character
     * @return Character or -1 if none available
     */
    int getchar();

    /**
     * Read data into buffer
     * @param buf Output buffer
     * @param maxLen Maximum bytes to read
     * @return Number of bytes read
     */
    size_t read(uint8_t* buf, size_t maxLen);

    // === Connection State ===

    /**
     * Check if a device is connected
     */
    bool isConnected() const;

    // === Callbacks (invoked by API) ===

    /**
     * Handle incoming RX data (called from GATT write callback)
     */
    void onRxData(const uint8_t* data, size_t len);

    /**
     * Handle connection state change (called from BLE connection callbacks)
     */
    void onConnectionChange(bool connected);

    // === Application Callbacks ===

    using ConnectCallback = std::function<void()>;
    using DisconnectCallback = std::function<void()>;

    void setOnConnect(ConnectCallback cb) { onConnect_ = cb; }
    void setOnDisconnect(DisconnectCallback cb) { onDisconnect_ = cb; }

    // === Singleton ===
    static BleUartService& instance();

private:
    BleUartService() = default;

    bool initialized_ = false;

    // GATT handle for TX characteristic (populated by registerGattService)
    uint16_t txCharHandle_ = 0;

    // Callbacks
    ConnectCallback onConnect_;
    DisconnectCallback onDisconnect_;

    // RX ring buffer with atomic indices for safe interaction between the
    // BLE host task (writer) and the application task (reader).
    static constexpr size_t RX_BUFFER_SIZE = 1024;
    uint8_t rxBuffer_[RX_BUFFER_SIZE] = {};
    std::atomic<size_t> rxHead_{0};
    std::atomic<size_t> rxTail_{0};

    // TX state
    volatile bool txCongested_ = false;
    volatile bool txInProgress_ = false;  // Recursion guard

    // BLE callback tokens for safe removal on deinit.
    cdc::hal::IBluetoothController::ListenerToken connToken_ =
        cdc::hal::IBluetoothController::INVALID_LISTENER;
    cdc::hal::IBluetoothController::ListenerToken disconnToken_ =
        cdc::hal::IBluetoothController::INVALID_LISTENER;
};

} // namespace cdc::mod_ble_serial
