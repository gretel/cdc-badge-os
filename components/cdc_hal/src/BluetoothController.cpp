/**
 * ESP32 Bluetooth (BLE) Controller Implementation
 * Full BLE stack management with NimBLE (when available)
 *
 * Requires CONFIG_BT_ENABLED and CONFIG_BT_NIMBLE_ENABLED in sdkconfig
 */

#include "cdc_hal/IBluetoothController.h"
#include "cdc_hal/hw_config.h"
#include "cdc_log.h"
#include "sdkconfig.h"

static const char* TAG = "BT-Ctrl";

// Check if NimBLE is enabled in sdkconfig
#if defined(CONFIG_BT_ENABLED) && defined(CONFIG_BT_NIMBLE_ENABLED)

#include "esp_bt.h"
#include "esp_mac.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <cstring>

namespace cdc::hal {

// Forward declaration for NimBLE host task
static void bleHostTask(void* param);

/**
 * ESP32 Bluetooth Controller Implementation using NimBLE
 */
class BluetoothController : public IBluetoothController {
public:
    BluetoothController() = default;

    // IService implementation
    bool init() override;
    bool start() override;
    void stop() override;
    core::ServiceState getState() const override { return state_; }
    const char* getName() const override { return "bluetooth"; }

    // IBluetoothController implementation
    bool enable() override;
    void disable() override;
    bool isEnabled() const override { return enabled_; }
    bool getMacAddress(uint8_t* mac) const override;
    void setDeviceName(const char* name) override;
    const char* getDeviceName() const override { return deviceName_; }
    bool isConnected() const override { return connHandle_ != BLE_HS_CONN_HANDLE_NONE; }
    void disconnect() override;
    int8_t getRssi() const override;

    // Connection event handlers (called from NimBLE callbacks)
    void onConnect(uint16_t connHandle);
    void onDisconnect(uint16_t connHandle, int reason);
    void onSync();

private:
    core::ServiceState state_ = core::ServiceState::UNINITIALIZED;
    bool enabled_ = false;
    bool synced_ = false;
    uint16_t connHandle_ = BLE_HS_CONN_HANDLE_NONE;
    char deviceName_[32] = "CDC Badge";
    uint8_t ownAddrType_ = BLE_OWN_ADDR_PUBLIC;

    // Singleton access for callbacks
public:
    static BluetoothController* instance_;
    friend void bleHostTask(void* param);
    friend int bleGapEventCallback(struct ble_gap_event* event, void* arg);
};

// Static instance pointer for callbacks
BluetoothController* BluetoothController::instance_ = nullptr;

// NimBLE GAP event callback
int bleGapEventCallback(struct ble_gap_event* event, void* arg) {
    (void)arg;
    auto* ctrl = BluetoothController::instance_;
    if (!ctrl) return 0;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                ctrl->onConnect(event->connect.conn_handle);
            } else {
                LOG_W(TAG, "Connection failed, status=%d", event->connect.status);
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ctrl->onDisconnect(event->disconnect.conn.conn_handle,
                              event->disconnect.reason);
            break;

        case BLE_GAP_EVENT_CONN_UPDATE:
            LOG_I(TAG, "Connection updated");
            break;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            LOG_D(TAG, "Advertising complete");
            break;

        case BLE_GAP_EVENT_MTU:
            LOG_I(TAG, "MTU updated: %d", event->mtu.value);
            break;

        default:
            break;
    }

    return 0;
}

// NimBLE sync callback
static void bleSyncCallback() {
    if (BluetoothController::instance_) {
        BluetoothController::instance_->onSync();
    }
}

// NimBLE reset callback
static void bleResetCallback(int reason) {
    LOG_E(TAG, "BLE host reset, reason=%d", reason);
}

// NimBLE host task
static void bleHostTask(void* param) {
    (void)param;
    LOG_I(TAG, "NimBLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

bool BluetoothController::init() {
    if (state_ != core::ServiceState::UNINITIALIZED) {
        return state_ == core::ServiceState::INITIALIZED ||
               state_ == core::ServiceState::STARTED;
    }

    instance_ = this;

    // Release classic BT memory (we only use BLE)
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    state_ = core::ServiceState::INITIALIZED;
    LOG_I(TAG, "Bluetooth controller initialized");
    return true;
}

bool BluetoothController::start() {
    if (state_ == core::ServiceState::INITIALIZED ||
        state_ == core::ServiceState::STOPPED) {
        state_ = core::ServiceState::STARTED;
        return true;
    }
    return state_ == core::ServiceState::STARTED;
}

void BluetoothController::stop() {
    if (state_ == core::ServiceState::STARTED) {
        if (enabled_) {
            disable();
        }
        state_ = core::ServiceState::STOPPED;
    }
}

bool BluetoothController::enable() {
    if (enabled_) {
        return true;
    }

    if (state_ != core::ServiceState::STARTED) {
        LOG_E(TAG, "Cannot enable - service not started");
        return false;
    }

    // Initialize NimBLE
    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        LOG_E(TAG, "nimble_port_init failed: %d", ret);
        return false;
    }

    // Configure NimBLE host
    ble_hs_cfg.reset_cb = bleResetCallback;
    ble_hs_cfg.sync_cb = bleSyncCallback;
    ble_hs_cfg.gatts_register_cb = nullptr;
    ble_hs_cfg.store_status_cb = nullptr;
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;

    // Set device name
    ble_svc_gap_device_name_set(deviceName_);

    // Start NimBLE host task
    nimble_port_freertos_init(bleHostTask);

    enabled_ = true;
    LOG_I(TAG, "Bluetooth enabled");
    return true;
}

void BluetoothController::disable() {
    if (!enabled_) {
        return;
    }

    // Disconnect any active connection
    if (connHandle_ != BLE_HS_CONN_HANDLE_NONE) {
        ble_gap_terminate(connHandle_, BLE_ERR_REM_USER_CONN_TERM);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Stop advertising if active
    ble_gap_adv_stop();

    // Shutdown NimBLE
    int rc = nimble_port_stop();
    if (rc == 0) {
        nimble_port_deinit();
    }

    enabled_ = false;
    synced_ = false;
    connHandle_ = BLE_HS_CONN_HANDLE_NONE;

    LOG_I(TAG, "Bluetooth disabled");
}

bool BluetoothController::getMacAddress(uint8_t* mac) const {
    if (!mac) return false;

    if (enabled_ && synced_) {
        // Get address from NimBLE
        int rc = ble_hs_id_copy_addr(ownAddrType_, mac, nullptr);
        return rc == 0;
    }

    // Fallback: read from efuse
    esp_read_mac(mac, ESP_MAC_BT);
    return true;
}

void BluetoothController::setDeviceName(const char* name) {
    if (!name) return;

    strncpy(deviceName_, name, sizeof(deviceName_) - 1);
    deviceName_[sizeof(deviceName_) - 1] = '\0';

    if (enabled_ && synced_) {
        ble_svc_gap_device_name_set(deviceName_);
    }
}

void BluetoothController::disconnect() {
    if (connHandle_ != BLE_HS_CONN_HANDLE_NONE) {
        ble_gap_terminate(connHandle_, BLE_ERR_REM_USER_CONN_TERM);
    }
}

int8_t BluetoothController::getRssi() const {
    if (connHandle_ == BLE_HS_CONN_HANDLE_NONE) {
        return 0;
    }

    int8_t rssi = 0;
    int rc = ble_gap_conn_rssi(connHandle_, &rssi);
    return (rc == 0) ? rssi : 0;
}

void BluetoothController::onConnect(uint16_t connHandle) {
    connHandle_ = connHandle;
    LOG_I(TAG, "Device connected (handle=%d)", connHandle);
}

void BluetoothController::onDisconnect(uint16_t connHandle, int reason) {
    (void)connHandle;
    connHandle_ = BLE_HS_CONN_HANDLE_NONE;
    LOG_I(TAG, "Device disconnected (reason=%d)", reason);
}

void BluetoothController::onSync() {
    synced_ = true;

    // Determine best address type
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        LOG_E(TAG, "Failed to ensure address: %d", rc);
        return;
    }

    rc = ble_hs_id_infer_auto(0, &ownAddrType_);
    if (rc != 0) {
        LOG_E(TAG, "Failed to infer address type: %d", rc);
        ownAddrType_ = BLE_OWN_ADDR_PUBLIC;
    }

    uint8_t addr[6];
    ble_hs_id_copy_addr(ownAddrType_, addr, nullptr);
    LOG_I(TAG, "BLE synced, addr=%02X:%02X:%02X:%02X:%02X:%02X",
             addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
}

// Singleton instance
static BluetoothController g_bluetoothController;

// Factory function
IBluetoothController* getBluetoothControllerInstance() {
    return &g_bluetoothController;
}

} // namespace cdc::hal

#else // NimBLE not enabled - stub implementation

#include "esp_mac.h"
#include <cstring>

namespace cdc::hal {

/**
 * Stub Bluetooth Controller when NimBLE is not enabled
 */
class BluetoothControllerStub : public IBluetoothController {
public:
    bool init() override {
        LOG_W(TAG, "Bluetooth disabled (NimBLE not configured)");
        state_ = core::ServiceState::INITIALIZED;
        return true;
    }
    bool start() override {
        state_ = core::ServiceState::STARTED;
        return true;
    }
    void stop() override { state_ = core::ServiceState::STOPPED; }
    core::ServiceState getState() const override { return state_; }
    const char* getName() const override { return "bluetooth"; }

    bool enable() override {
        LOG_W(TAG, "Cannot enable - NimBLE not configured in sdkconfig");
        return false;
    }
    void disable() override {}
    bool isEnabled() const override { return false; }
    bool getMacAddress(uint8_t* mac) const override {
        if (mac) esp_read_mac(mac, ESP_MAC_BT);
        return mac != nullptr;
    }
    void setDeviceName(const char* name) override {
        if (name) {
            strncpy(deviceName_, name, sizeof(deviceName_) - 1);
        }
    }
    const char* getDeviceName() const override { return deviceName_; }
    bool isConnected() const override { return false; }
    void disconnect() override {}
    int8_t getRssi() const override { return 0; }

private:
    core::ServiceState state_ = core::ServiceState::UNINITIALIZED;
    char deviceName_[32] = "CDC Badge";
};

static BluetoothControllerStub g_bluetoothController;

IBluetoothController* getBluetoothControllerInstance() {
    return &g_bluetoothController;
}

} // namespace cdc::hal

#endif // CONFIG_BT_ENABLED && CONFIG_BT_NIMBLE_ENABLED
