/**
 * BLE UART Service Implementation
 *
 * Nordic UART Service (NUS) compatible implementation using NimBLE.
 *
 * NOTE: This is a framework implementation. The actual NimBLE GATT
 * service registration requires integration with BluetoothController
 * which needs to be extended to support custom GATT services.
 */

#include "mod_ble_serial/BleUartService.h"
#include "cdc_hal/IBluetoothController.h"
#include "cdc_log.h"
#include <cstring>

// NimBLE includes (only when BLE is enabled)
#if CONFIG_BT_NIMBLE_ENABLED
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#endif

static const char* TAG = "BLE_UART";

namespace cdc::mod_ble_serial {

// Nordic UART Service UUIDs
// Service: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
// RX:      6E400002-B5A3-F393-E0A9-E50E24DCCA9E
// TX:      6E400003-B5A3-F393-E0A9-E50E24DCCA9E

#if CONFIG_BT_NIMBLE_ENABLED
static const ble_uuid128_t s_nus_svc_uuid = BLE_UUID128_INIT(
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e
);

static const ble_uuid128_t s_nus_rx_uuid = BLE_UUID128_INIT(
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e
);

static const ble_uuid128_t s_nus_tx_uuid = BLE_UUID128_INIT(
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e
);
#endif

// =============================================================================
// Singleton
// =============================================================================

BleUartService& BleUartService::instance() {
    static BleUartService inst;
    return inst;
}

// =============================================================================
// Initialization
// =============================================================================

bool BleUartService::init() {
    if (initialized_) {
        return true;
    }

    auto* ble = cdc::hal::getBluetoothControllerInstance();
    if (!ble || !ble->isEnabled()) {
        LOG_E(TAG, "BLE not enabled");
        return false;
    }

#if CONFIG_BT_NIMBLE_ENABLED
    // TODO: Register GATT service with NimBLE
    // This requires extending BluetoothController to support
    // custom GATT service registration.
    //
    // For now, we log a placeholder message.
    LOG_I(TAG, "BLE UART Service initialized (GATT registration pending)");

    // Reset RX buffer
    rxHead_ = 0;
    rxTail_ = 0;

    initialized_ = true;
    return true;
#else
    LOG_E(TAG, "NimBLE not enabled in config");
    return false;
#endif
}

void BleUartService::deinit() {
    if (!initialized_) {
        return;
    }

#if CONFIG_BT_NIMBLE_ENABLED
    // TODO: Unregister GATT service
    LOG_I(TAG, "BLE UART Service deinitialized");
#endif

    initialized_ = false;
}

// =============================================================================
// TX (Badge -> Phone)
// =============================================================================

size_t BleUartService::send(const uint8_t* data, size_t len) {
    if (!initialized_ || !isConnected() || !data || len == 0) {
        return 0;
    }

    // Recursion guard - prevent logging from causing infinite loop
    if (txInProgress_) {
        return 0;
    }
    txInProgress_ = true;

#if CONFIG_BT_NIMBLE_ENABLED
    // TODO: Send data via BLE notification
    // This requires the TX characteristic handle and ble_gatts_notify_custom()
    //
    // For now, we just return 0 to indicate nothing was sent.
    // When implemented:
    // - Split data into MTU-sized chunks
    // - Send each chunk via notification
    // - Handle flow control / congestion
#endif

    txInProgress_ = false;
    return 0;  // Placeholder
}

size_t BleUartService::send(const char* str) {
    if (!str) return 0;
    return send(reinterpret_cast<const uint8_t*>(str), strlen(str));
}

bool BleUartService::txReady() const {
    return initialized_ && isConnected() && !txCongested_;
}

// =============================================================================
// RX (Phone -> Badge)
// =============================================================================

size_t BleUartService::available() const {
    if (!initialized_) return 0;

    size_t head = rxHead_;
    size_t tail = rxTail_;

    if (head >= tail) {
        return head - tail;
    } else {
        return RX_BUFFER_SIZE - tail + head;
    }
}

int BleUartService::getchar() {
    if (!initialized_ || available() == 0) {
        return -1;
    }

    uint8_t c = rxBuffer_[rxTail_];
    rxTail_ = (rxTail_ + 1) % RX_BUFFER_SIZE;
    return c;
}

size_t BleUartService::read(uint8_t* buf, size_t maxLen) {
    if (!buf || maxLen == 0) return 0;

    size_t count = 0;
    while (count < maxLen && available() > 0) {
        int c = getchar();
        if (c < 0) break;
        buf[count++] = static_cast<uint8_t>(c);
    }
    return count;
}

// =============================================================================
// Connection State
// =============================================================================

bool BleUartService::isConnected() const {
    auto* ble = cdc::hal::getBluetoothControllerInstance();
    return ble && ble->isConnected();
}

// =============================================================================
// Internal Callbacks
// =============================================================================

void BleUartService::onRxData(const uint8_t* data, size_t len) {
    if (!data || len == 0) return;

    // Add data to ring buffer
    for (size_t i = 0; i < len; i++) {
        size_t nextHead = (rxHead_ + 1) % RX_BUFFER_SIZE;
        if (nextHead == rxTail_) {
            // Buffer full - drop oldest
            rxTail_ = (rxTail_ + 1) % RX_BUFFER_SIZE;
        }
        rxBuffer_[rxHead_] = data[i];
        rxHead_ = nextHead;
    }
}

void BleUartService::onMtuUpdate(uint16_t mtu) {
    // MTU includes 3 bytes overhead, so max data is MTU - 3
    mtu_ = (mtu > 3) ? (mtu - 3) : 20;
    LOG_I(TAG, "MTU updated: %d (max payload: %d)", mtu, mtu_);
}

void BleUartService::onConnectionChange(bool connected) {
    if (connected) {
        LOG_I(TAG, "Device connected");
        if (onConnect_) onConnect_();
    } else {
        LOG_I(TAG, "Device disconnected");
        // Clear RX buffer on disconnect
        rxHead_ = 0;
        rxTail_ = 0;
        if (onDisconnect_) onDisconnect_();
    }
}

int BleUartService::gattAccessCallback(uint16_t connHandle, uint16_t attrHandle,
                                         void* ctxtPtr, void* arg) {
    (void)connHandle;
    (void)attrHandle;
    (void)arg;

#if CONFIG_BT_NIMBLE_ENABLED
    auto* ctxt = static_cast<struct ble_gatt_access_ctxt*>(ctxtPtr);
    auto& self = instance();

    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            // RX characteristic - data from phone
            if (ctxt->om) {
                uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
                uint8_t buf[256];
                if (len > sizeof(buf)) len = sizeof(buf);
                ble_hs_mbuf_to_flat(ctxt->om, buf, len, nullptr);
                self.onRxData(buf, len);
            }
            return 0;

        case BLE_GATT_ACCESS_OP_READ_CHR:
            // TX characteristic - should not be read directly
            return 0;

        default:
            return BLE_ATT_ERR_UNLIKELY;
    }
#else
    (void)ctxtPtr;
    return 0;
#endif
}

} // namespace cdc::mod_ble_serial
