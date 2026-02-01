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

    // Advertising
    void startAdvertising() override;
    void stopAdvertising() override;
    bool isAdvertising() const override { return advertising_; }

    // Scanning
    bool startScan(uint32_t durationMs) override;
    void stopScan() override;
    bool isScanComplete() const override { return !scanning_; }
    uint8_t getScanResults(BleScanResult* results, uint8_t maxResults) override;

    // Connection event handlers (called from NimBLE callbacks)
    void onConnect(uint16_t connHandle);
    void onDisconnect(uint16_t connHandle, int reason);
    void onSync();
    void onScanResult(const ble_gap_disc_desc* disc);
    void onScanComplete();
    void onAdvComplete();

private:
    core::ServiceState state_ = core::ServiceState::UNINITIALIZED;
    bool enabled_ = false;
    bool synced_ = false;
    bool advertising_ = false;
    bool scanning_ = false;
    uint16_t connHandle_ = BLE_HS_CONN_HANDLE_NONE;
    char deviceName_[32] = "CDC Badge";
    uint8_t ownAddrType_ = BLE_OWN_ADDR_PUBLIC;

    // Scan results storage
    static constexpr uint8_t MAX_SCAN_RESULTS = 16;
    BleScanResult scanResults_[MAX_SCAN_RESULTS] = {};
    uint8_t scanResultCount_ = 0;

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
            ctrl->onAdvComplete();
            break;

        case BLE_GAP_EVENT_MTU:
            LOG_I(TAG, "MTU updated: %d", event->mtu.value);
            break;

        case BLE_GAP_EVENT_DISC:
            ctrl->onScanResult(&event->disc);
            break;

        case BLE_GAP_EVENT_DISC_COMPLETE:
            LOG_D(TAG, "Scan complete");
            ctrl->onScanComplete();
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

// =============================================================================
// Advertising
// =============================================================================

void BluetoothController::startAdvertising() {
    if (!enabled_ || !synced_ || advertising_) return;

    struct ble_gap_adv_params advParams = {};
    advParams.conn_mode = BLE_GAP_CONN_MODE_UND;
    advParams.disc_mode = BLE_GAP_DISC_MODE_GEN;
    advParams.itvl_min = BLE_GAP_ADV_FAST_INTERVAL1_MIN;
    advParams.itvl_max = BLE_GAP_ADV_FAST_INTERVAL1_MAX;

    // Build advertising data
    struct ble_hs_adv_fields fields = {};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    fields.name = (uint8_t*)deviceName_;
    fields.name_len = strlen(deviceName_);
    fields.name_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        LOG_E(TAG, "Failed to set adv fields: %d", rc);
        return;
    }

    rc = ble_gap_adv_start(ownAddrType_, nullptr, BLE_HS_FOREVER,
                           &advParams, bleGapEventCallback, nullptr);
    if (rc != 0) {
        LOG_E(TAG, "Failed to start advertising: %d", rc);
        return;
    }

    advertising_ = true;
    LOG_I(TAG, "Advertising started");
}

void BluetoothController::stopAdvertising() {
    if (!advertising_) return;

    ble_gap_adv_stop();
    advertising_ = false;
    LOG_I(TAG, "Advertising stopped");
}

void BluetoothController::onAdvComplete() {
    advertising_ = false;
}

// =============================================================================
// Scanning
// =============================================================================

bool BluetoothController::startScan(uint32_t durationMs) {
    if (!enabled_ || !synced_ || scanning_) return false;

    // Clear previous results
    scanResultCount_ = 0;
    memset(scanResults_, 0, sizeof(scanResults_));

    struct ble_gap_disc_params discParams = {};
    discParams.filter_duplicates = 1;
    discParams.passive = 0;  // Active scan to get names
    discParams.itvl = 0;     // Use defaults
    discParams.window = 0;
    discParams.filter_policy = 0;
    discParams.limited = 0;

    int rc = ble_gap_disc(ownAddrType_, durationMs, &discParams,
                          bleGapEventCallback, nullptr);
    if (rc != 0) {
        LOG_E(TAG, "Failed to start scan: %d", rc);
        return false;
    }

    scanning_ = true;
    LOG_I(TAG, "Scan started (%lu ms)", (unsigned long)durationMs);
    return true;
}

void BluetoothController::stopScan() {
    if (!scanning_) return;

    ble_gap_disc_cancel();
    scanning_ = false;
    LOG_I(TAG, "Scan stopped");
}

void BluetoothController::onScanResult(const ble_gap_disc_desc* disc) {
    if (!disc || scanResultCount_ >= MAX_SCAN_RESULTS) return;

    // Check if we already have this device
    for (uint8_t i = 0; i < scanResultCount_; i++) {
        if (memcmp(scanResults_[i].mac, disc->addr.val, 6) == 0) {
            // Update RSSI if stronger
            if (disc->rssi > scanResults_[i].rssi) {
                scanResults_[i].rssi = disc->rssi;
            }
            return;
        }
    }

    // Add new result
    BleScanResult& result = scanResults_[scanResultCount_];
    memcpy(result.mac, disc->addr.val, 6);
    result.rssi = disc->rssi;
    result.name[0] = '\0';

    // Parse advertising data for name
    struct ble_hs_adv_fields fields;
    if (ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data) == 0) {
        if (fields.name != nullptr && fields.name_len > 0) {
            size_t copyLen = (fields.name_len < sizeof(result.name) - 1)
                             ? fields.name_len : sizeof(result.name) - 1;
            memcpy(result.name, fields.name, copyLen);
            result.name[copyLen] = '\0';
        }
    }

    // Use MAC as name if no name found
    if (result.name[0] == '\0') {
        snprintf(result.name, sizeof(result.name), "%02X:%02X:%02X:%02X:%02X:%02X",
                 result.mac[5], result.mac[4], result.mac[3],
                 result.mac[2], result.mac[1], result.mac[0]);
    }

    scanResultCount_++;
    LOG_D(TAG, "Found: %s (RSSI %d)", result.name, result.rssi);
}

void BluetoothController::onScanComplete() {
    scanning_ = false;
    LOG_I(TAG, "Scan complete, found %d devices", scanResultCount_);
}

uint8_t BluetoothController::getScanResults(BleScanResult* results, uint8_t maxResults) {
    if (!results || maxResults == 0) return 0;

    uint8_t count = (scanResultCount_ < maxResults) ? scanResultCount_ : maxResults;
    memcpy(results, scanResults_, count * sizeof(BleScanResult));
    return count;
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
