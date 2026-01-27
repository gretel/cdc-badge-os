// BLE Core - Centralized Bluetooth stack management
// Provides shared BT stack initialization and GAP event handling for all BLE services

#include "ble_core.h"

#if FEATURE_BLE_UART || FEATURE_BLE_BADGE || FEATURE_BLE_HID

#include "cdc_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gattc_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

#include <string.h>

#define BLE_CORE_TAG "BLE_CORE"
#define DEVICE_NAME_MAX 30

// State
static bool g_initialized = false;
static bool g_advertising = false;
static char g_device_name[DEVICE_NAME_MAX] = "CDC Badge";
static SemaphoreHandle_t g_init_mutex = NULL;
static uint8_t g_own_addr[6] = {0};

// Callbacks
static ble_core_passkey_cb_t g_passkey_cb = NULL;
static ble_core_auth_cb_t g_auth_cb = NULL;
static ble_core_nc_cb_t g_nc_cb = NULL;

// GAP event listeners (for scan results, etc.)
static ble_core_gap_event_cb_t g_gap_listeners[BLE_APP_MAX] = {NULL};

// Pending pairing address (for confirm_pairing)
static uint8_t g_pairing_addr[6] = {0};

// Advertising parameters
static esp_ble_adv_params_t g_adv_params = {
    .adv_int_min        = 0xA0,     // 100ms
    .adv_int_max        = 0x140,    // 200ms
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .peer_addr          = {0},
    .peer_addr_type     = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static esp_ble_adv_data_t g_adv_data = {
    .set_scan_rsp        = false,
    .include_name        = true,
    .include_txpower     = true,
    .min_interval        = 0x0006,
    .max_interval        = 0x0010,
    .appearance          = 0x00,
    .manufacturer_len    = 0,
    .p_manufacturer_data = NULL,
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = 0,
    .p_service_uuid      = NULL,
    .flag                = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_data_t g_scan_rsp_data = {
    .set_scan_rsp        = true,
    .include_name        = true,
    .include_txpower     = false,
    .min_interval        = 0x0006,
    .max_interval        = 0x0010,
    .appearance          = 0x00,
    .manufacturer_len    = 0,
    .p_manufacturer_data = NULL,
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = 0,
    .p_service_uuid      = NULL,
    .flag                = 0,
};

// Track adv data config state
static bool g_adv_data_set = false;
static bool g_scan_rsp_set = false;

// ============================================================================
// GAP Event Handler (shared for all services)
// ============================================================================

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    // Forward events to registered listeners (for scan results, etc.)
    for (int i = 0; i < BLE_APP_MAX; i++) {
        if (g_gap_listeners[i]) {
            g_gap_listeners[i]((int)event, param);
        }
    }

    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            g_adv_data_set = true;
            if (g_scan_rsp_set && g_advertising) {
                esp_ble_gap_start_advertising(&g_adv_params);
            }
            break;

        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
            g_scan_rsp_set = true;
            if (g_adv_data_set && g_advertising) {
                esp_ble_gap_start_advertising(&g_adv_params);
            }
            break;

        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                LOG_D(BLE_CORE_TAG, "Advertising started");
            }
            break;

        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            LOG_D(BLE_CORE_TAG, "Advertising stopped");
            break;

        // Security events
        case ESP_GAP_BLE_SEC_REQ_EVT:
            // Accept security request from client
            esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
            break;

        case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:
            // Display passkey for user to enter on remote device
            LOG_I(BLE_CORE_TAG, "Passkey: %06lu", (unsigned long)param->ble_security.key_notif.passkey);
            memcpy(g_pairing_addr, param->ble_security.key_notif.bd_addr, 6);
            if (g_passkey_cb) {
                g_passkey_cb(param->ble_security.key_notif.passkey);
            }
            break;

        case ESP_GAP_BLE_NC_REQ_EVT:
            // Numeric comparison - both devices show same number
            LOG_I(BLE_CORE_TAG, "Numeric comparison: %06lu", (unsigned long)param->ble_security.key_notif.passkey);
            memcpy(g_pairing_addr, param->ble_security.ble_req.bd_addr, 6);
            if (g_nc_cb) {
                g_nc_cb(param->ble_security.key_notif.passkey);
            } else {
                // Auto-accept if no callback registered
                esp_ble_confirm_reply(param->ble_security.ble_req.bd_addr, true);
            }
            break;

        case ESP_GAP_BLE_PASSKEY_REQ_EVT:
            // Remote requests passkey entry - not supported in display-only mode
            LOG_W(BLE_CORE_TAG, "Passkey request (unexpected in display-only mode)");
            break;

        case ESP_GAP_BLE_KEY_EVT:
            LOG_D(BLE_CORE_TAG, "Key exchange: type=%d", param->ble_security.ble_key.key_type);
            break;

        case ESP_GAP_BLE_AUTH_CMPL_EVT: {
            bool success = param->ble_security.auth_cmpl.success;
            LOG_I(BLE_CORE_TAG, "Auth complete: %s", success ? "success" : "failed");
            if (!success) {
                LOG_E(BLE_CORE_TAG, "Auth fail reason: 0x%x", param->ble_security.auth_cmpl.fail_reason);
            }
            if (g_auth_cb) {
                g_auth_cb(success);
            }
            break;
        }

        case ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT:
            LOG_I(BLE_CORE_TAG, "Bond removed");
            break;

        default:
            break;
    }
}

// ============================================================================
// Security Configuration
// ============================================================================

static void configure_security(void) {
    // IO Capability: Display Only (badge shows passkey, remote enters it)
    esp_ble_io_cap_t iocap = ESP_IO_CAP_OUT;
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(iocap));

    // Auth requirements: Secure Connections + MITM protection + Bonding
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(auth_req));

    // Max encryption key size (128-bit)
    uint8_t key_size = 16;
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(key_size));

    // Enable bonding - store keys for reconnection
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(init_key));

    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(rsp_key));

    // Only accept authenticated (MITM protected) connections
    uint8_t auth_option = ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_ENABLE;
    esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH, &auth_option, sizeof(auth_option));

    LOG_I(BLE_CORE_TAG, "Security: Passkey + MITM + Bonding");
}

// ============================================================================
// Public API
// ============================================================================

bool ble_core_init(void) {
    // Create mutex on first call
    if (!g_init_mutex) {
        g_init_mutex = xSemaphoreCreateMutex();
        if (!g_init_mutex) {
            LOG_E(BLE_CORE_TAG, "Failed to create init mutex");
            return false;
        }
    }

    xSemaphoreTake(g_init_mutex, portMAX_DELAY);

    if (g_initialized) {
        xSemaphoreGive(g_init_mutex);
        return true;
    }

    LOG_I(BLE_CORE_TAG, "Initializing BLE stack...");

    // Release classic BT memory (we only use BLE)
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    // Initialize BT controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        LOG_E(BLE_CORE_TAG, "BT controller init failed: %s", esp_err_to_name(ret));
        xSemaphoreGive(g_init_mutex);
        return false;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) {
        LOG_E(BLE_CORE_TAG, "BT controller enable failed: %s", esp_err_to_name(ret));
        esp_bt_controller_deinit();
        xSemaphoreGive(g_init_mutex);
        return false;
    }

    // Initialize Bluedroid
    ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        LOG_E(BLE_CORE_TAG, "Bluedroid init failed: %s", esp_err_to_name(ret));
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        xSemaphoreGive(g_init_mutex);
        return false;
    }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        LOG_E(BLE_CORE_TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret));
        esp_bluedroid_deinit();
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        xSemaphoreGive(g_init_mutex);
        return false;
    }

    // Register GAP callback
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret != ESP_OK) {
        LOG_E(BLE_CORE_TAG, "GAP callback register failed: %s", esp_err_to_name(ret));
        esp_bluedroid_disable();
        esp_bluedroid_deinit();
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        xSemaphoreGive(g_init_mutex);
        return false;
    }

    // Configure security parameters
    configure_security();

    // Set MTU to maximum for better throughput
    esp_ble_gatt_set_local_mtu(517);

    // Get and store own BLE address
    const uint8_t *addr = esp_bt_dev_get_address();
    if (addr) {
        memcpy(g_own_addr, addr, 6);
        LOG_I(BLE_CORE_TAG, "BLE Address: %02X:%02X:%02X:%02X:%02X:%02X",
              g_own_addr[0], g_own_addr[1], g_own_addr[2],
              g_own_addr[3], g_own_addr[4], g_own_addr[5]);
    }

    // Set device name
    esp_ble_gap_set_device_name(g_device_name);

    g_initialized = true;
    LOG_I(BLE_CORE_TAG, "BLE stack initialized");

    xSemaphoreGive(g_init_mutex);
    return true;
}

void ble_core_deinit(void) {
    if (!g_initialized) return;

    if (g_init_mutex) {
        xSemaphoreTake(g_init_mutex, portMAX_DELAY);
    }

    LOG_I(BLE_CORE_TAG, "Deinitializing BLE stack...");

    g_advertising = false;
    g_initialized = false;

    esp_ble_gap_stop_advertising();
    vTaskDelay(pdMS_TO_TICKS(50));

    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();

    g_adv_data_set = false;
    g_scan_rsp_set = false;

    LOG_I(BLE_CORE_TAG, "BLE stack deinitialized");

    if (g_init_mutex) {
        xSemaphoreGive(g_init_mutex);
    }
}

bool ble_core_is_initialized(void) {
    return g_initialized;
}

const char* ble_core_get_device_name(void) {
    return g_device_name;
}

void ble_core_set_device_name(const char* name) {
    if (!name) return;
    strncpy(g_device_name, name, DEVICE_NAME_MAX - 1);
    g_device_name[DEVICE_NAME_MAX - 1] = '\0';

    if (g_initialized) {
        esp_ble_gap_set_device_name(g_device_name);
    }
}

void ble_core_set_passkey_callback(ble_core_passkey_cb_t cb) {
    g_passkey_cb = cb;
}

void ble_core_set_auth_callback(ble_core_auth_cb_t cb) {
    g_auth_cb = cb;
}

void ble_core_set_nc_callback(ble_core_nc_cb_t cb) {
    g_nc_cb = cb;
}

void ble_core_confirm_pairing(const uint8_t addr[6], bool accept) {
    if (addr) {
        esp_ble_confirm_reply((uint8_t*)addr, accept);
    } else {
        esp_ble_confirm_reply(g_pairing_addr, accept);
    }
}

void ble_core_start_advertising(void) {
    if (!g_initialized) return;

    g_advertising = true;

    // Configure advertising data
    esp_ble_gap_config_adv_data(&g_adv_data);
    esp_ble_gap_config_adv_data(&g_scan_rsp_data);

    // Actual start happens in ADV_DATA_SET_COMPLETE callback
}

void ble_core_stop_advertising(void) {
    if (!g_initialized) return;

    g_advertising = false;
    esp_ble_gap_stop_advertising();
}

bool ble_core_is_advertising(void) {
    return g_advertising;
}

bool ble_core_get_address(uint8_t addr[6]) {
    if (!g_initialized || !addr) return false;
    memcpy(addr, g_own_addr, 6);
    return true;
}

void ble_core_register_gap_listener(ble_core_gap_event_cb_t cb, ble_app_id_t id) {
    if (id < BLE_APP_MAX) {
        g_gap_listeners[id] = cb;
    }
}

#endif // FEATURE_BLE_UART || FEATURE_BLE_BADGE || FEATURE_BLE_HID
