// BLE CTAP2 Transport Implementation
// Uses NimBLE stack (ESP-IDF default)

#include "ble_ctap.h"

#if FEATURE_FIDO2_BT

#include "cdc_log.h"
#include "ctap2.h"
#include "fido2.h"
#include "feature_flags.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

#include <string.h>

// ============================================================================
// Constants
// ============================================================================

#define DEVICE_NAME             "CDC Badge FIDO2"
#define GATTS_TAG               "BLE_CTAP"

// Service and characteristic handles (assigned by stack)
#define FIDO2_NUM_HANDLE        8       // Service + 4 characteristics * 2

// ============================================================================
// State
// ============================================================================

static bool g_initialized = false;
static bool g_connected = false;
static uint16_t g_conn_id = 0;
static uint16_t g_mtu = BLE_CTAP_MTU_DEFAULT;

// GATT handles
static uint16_t g_service_handle = 0;
static uint16_t g_control_point_handle = 0;
static uint16_t g_status_handle = 0;
static uint16_t g_status_ccc_handle = 0;  // Client Characteristic Configuration
static uint16_t g_cp_len_handle = 0;
static uint16_t g_revision_handle = 0;

// Message assembly buffer
static uint8_t g_rx_buffer[BLE_CTAP_MAX_MSG_SIZE];
static uint16_t g_rx_offset = 0;
static uint16_t g_rx_expected_len = 0;
static bool g_rx_in_progress = false;

// Response buffer
static uint8_t g_tx_buffer[BLE_CTAP_MAX_MSG_SIZE];
static uint16_t g_tx_len = 0;
static uint16_t g_tx_offset = 0;

// Notifications enabled
static bool g_status_notify_enabled = false;

// ============================================================================
// Service Definition
// ============================================================================

// FIDO2 BLE Service UUID (128-bit)
static const uint8_t fido2_service_uuid[16] = {
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0xFD, 0xFF, 0x00, 0x00
};

// Control Point Characteristic UUID
static const uint8_t control_point_uuid[16] = {
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0xF1, 0xFF, 0x00, 0x00
};

// Status Characteristic UUID
static const uint8_t status_uuid[16] = {
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0xF2, 0xFF, 0x00, 0x00
};

// Control Point Length UUID
static const uint8_t cp_len_uuid[16] = {
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0xF3, 0xFF, 0x00, 0x00
};

// Service Revision UUID
static const uint8_t revision_uuid[16] = {
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0xF4, 0xFF, 0x00, 0x00
};

// Service Revision Bitfield (CTAP2 BLE v1.1 = 0x20)
static const uint8_t service_revision_value = 0x20;

// Control Point Length (2 bytes, little endian)
static const uint8_t cp_len_value[2] = {
    (BLE_CTAP_MAX_MSG_SIZE & 0xFF),
    (BLE_CTAP_MAX_MSG_SIZE >> 8) & 0xFF
};

// ============================================================================
// Forward Declarations
// ============================================================================

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                 esp_ble_gatts_cb_param_t *param);
static void process_control_point_write(const uint8_t *data, uint16_t len);
static void send_response_fragment(void);
static void reset_rx_state(void);

// ============================================================================
// Advertising Configuration
// ============================================================================

static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,     // 20ms
    .adv_int_max        = 0x40,     // 40ms
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .peer_addr          = {0},
    .peer_addr_type     = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// Advertising data
static uint8_t adv_service_uuid[] = {
    0x03, 0x03, 0xFD, 0xFF  // Complete list of 16-bit UUIDs: FIDO2 Service
};

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp        = false,
    .include_name        = true,
    .include_txpower     = false,
    .min_interval        = 0x0006,
    .max_interval        = 0x0010,
    .appearance          = 0x00,
    .manufacturer_len    = 0,
    .p_manufacturer_data = NULL,
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = sizeof(adv_service_uuid),
    .p_service_uuid      = adv_service_uuid,
    .flag                = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// ============================================================================
// GATTS Profile
// ============================================================================

static esp_gatt_if_t g_gatts_if = ESP_GATT_IF_NONE;

// ============================================================================
// GAP Event Handler
// ============================================================================

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            LOG_D(GATTS_TAG, "Advertising data set");
            esp_ble_gap_start_advertising(&adv_params);
            break;

        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                LOG_E(GATTS_TAG, "Advertising start failed: %d", param->adv_start_cmpl.status);
            } else {
                LOG_I(GATTS_TAG, "Advertising started");
            }
            break;

        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            LOG_I(GATTS_TAG, "Advertising stopped");
            break;

        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            LOG_D(GATTS_TAG, "Connection params updated");
            break;

        default:
            break;
    }
}

// ============================================================================
// GATTS Event Handler
// ============================================================================

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                 esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT:
            if (param->reg.status == ESP_GATT_OK) {
                g_gatts_if = gatts_if;
                LOG_I(GATTS_TAG, "GATT server registered");

                // Set device name
                esp_ble_gap_set_device_name(DEVICE_NAME);

                // Configure advertising data
                esp_ble_gap_config_adv_data(&adv_data);

                // Create FIDO2 service
                esp_gatt_srvc_id_t service_id;
                memset(&service_id, 0, sizeof(service_id));
                service_id.is_primary = true;
                service_id.id.inst_id = 0;
                service_id.id.uuid.len = ESP_UUID_LEN_128;
                memcpy(service_id.id.uuid.uuid.uuid128, fido2_service_uuid, 16);
                esp_ble_gatts_create_service(gatts_if, &service_id, FIDO2_NUM_HANDLE);
            } else {
                LOG_E(GATTS_TAG, "GATT register failed: %d", param->reg.status);
            }
            break;

        case ESP_GATTS_CREATE_EVT:
            if (param->create.status == ESP_GATT_OK) {
                g_service_handle = param->create.service_handle;
                LOG_D(GATTS_TAG, "Service created, handle: %d", g_service_handle);

                // Start service
                esp_ble_gatts_start_service(g_service_handle);

                // Add Control Point characteristic (write)
                esp_bt_uuid_t cp_uuid;
                memset(&cp_uuid, 0, sizeof(cp_uuid));
                cp_uuid.len = ESP_UUID_LEN_128;
                memcpy(cp_uuid.uuid.uuid128, control_point_uuid, 16);
                esp_ble_gatts_add_char(g_service_handle, &cp_uuid,
                                        ESP_GATT_PERM_WRITE,
                                        ESP_GATT_CHAR_PROP_BIT_WRITE,
                                        NULL, NULL);
            }
            break;

        case ESP_GATTS_ADD_CHAR_EVT:
            if (param->add_char.status == ESP_GATT_OK) {
                LOG_D(GATTS_TAG, "Characteristic added, handle: %d", param->add_char.attr_handle);

                // Determine which characteristic was added and add next
                if (g_control_point_handle == 0) {
                    g_control_point_handle = param->add_char.attr_handle;

                    // Add Status characteristic (notify)
                    esp_bt_uuid_t status_char_uuid;
                    memset(&status_char_uuid, 0, sizeof(status_char_uuid));
                    status_char_uuid.len = ESP_UUID_LEN_128;
                    memcpy(status_char_uuid.uuid.uuid128, status_uuid, 16);
                    esp_ble_gatts_add_char(g_service_handle, &status_char_uuid,
                                            ESP_GATT_PERM_READ,
                                            ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                                            NULL, NULL);
                }
                else if (g_status_handle == 0) {
                    g_status_handle = param->add_char.attr_handle;

                    // Add CCC descriptor for notifications
                    esp_bt_uuid_t ccc_uuid;
                    memset(&ccc_uuid, 0, sizeof(ccc_uuid));
                    ccc_uuid.len = ESP_UUID_LEN_16;
                    ccc_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
                    esp_ble_gatts_add_char_descr(g_service_handle, &ccc_uuid,
                                                  ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                  NULL, NULL);
                }
                else if (g_cp_len_handle == 0) {
                    g_cp_len_handle = param->add_char.attr_handle;

                    // Add Service Revision characteristic (read)
                    esp_bt_uuid_t rev_uuid;
                    memset(&rev_uuid, 0, sizeof(rev_uuid));
                    rev_uuid.len = ESP_UUID_LEN_128;
                    memcpy(rev_uuid.uuid.uuid128, revision_uuid, 16);
                    esp_attr_value_t rev_val;
                    memset(&rev_val, 0, sizeof(rev_val));
                    rev_val.attr_max_len = 1;
                    rev_val.attr_len = 1;
                    rev_val.attr_value = (uint8_t*)&service_revision_value;
                    esp_ble_gatts_add_char(g_service_handle, &rev_uuid,
                                            ESP_GATT_PERM_READ,
                                            ESP_GATT_CHAR_PROP_BIT_READ,
                                            &rev_val, NULL);
                }
                else if (g_revision_handle == 0) {
                    g_revision_handle = param->add_char.attr_handle;
                    LOG_I(GATTS_TAG, "All characteristics added");
                }
            }
            break;

        case ESP_GATTS_ADD_CHAR_DESCR_EVT:
            if (param->add_char_descr.status == ESP_GATT_OK) {
                g_status_ccc_handle = param->add_char_descr.attr_handle;
                LOG_D(GATTS_TAG, "CCC descriptor added, handle: %d", g_status_ccc_handle);

                // Add Control Point Length characteristic (read)
                esp_bt_uuid_t len_uuid;
                memset(&len_uuid, 0, sizeof(len_uuid));
                len_uuid.len = ESP_UUID_LEN_128;
                memcpy(len_uuid.uuid.uuid128, cp_len_uuid, 16);
                esp_attr_value_t len_val;
                memset(&len_val, 0, sizeof(len_val));
                len_val.attr_max_len = 2;
                len_val.attr_len = 2;
                len_val.attr_value = (uint8_t*)cp_len_value;
                esp_ble_gatts_add_char(g_service_handle, &len_uuid,
                                        ESP_GATT_PERM_READ,
                                        ESP_GATT_CHAR_PROP_BIT_READ,
                                        &len_val, NULL);
            }
            break;

        case ESP_GATTS_START_EVT:
            LOG_I(GATTS_TAG, "Service started");
            break;

        case ESP_GATTS_CONNECT_EVT:
            LOG_I(GATTS_TAG, "Client connected, conn_id: %d", param->connect.conn_id);
            g_connected = true;
            g_conn_id = param->connect.conn_id;
            g_mtu = BLE_CTAP_MTU_DEFAULT;
            reset_rx_state();
            break;

        case ESP_GATTS_DISCONNECT_EVT:
            LOG_I(GATTS_TAG, "Client disconnected");
            g_connected = false;
            g_status_notify_enabled = false;
            reset_rx_state();
            // Restart advertising
            esp_ble_gap_start_advertising(&adv_params);
            break;

        case ESP_GATTS_MTU_EVT:
            g_mtu = param->mtu.mtu - 3;  // ATT MTU - 3 (ATT header)
            LOG_I(GATTS_TAG, "MTU exchanged: %d (usable: %d)", param->mtu.mtu, g_mtu);
            break;

        case ESP_GATTS_WRITE_EVT:
            if (param->write.handle == g_control_point_handle) {
                // Control Point write - CTAP2 command data
                process_control_point_write(param->write.value, param->write.len);
            }
            else if (param->write.handle == g_status_ccc_handle) {
                // CCC write - enable/disable notifications
                if (param->write.len == 2) {
                    uint16_t ccc_value = param->write.value[0] | (param->write.value[1] << 8);
                    g_status_notify_enabled = (ccc_value == 0x0001);
                    LOG_D(GATTS_TAG, "Notifications %s", g_status_notify_enabled ? "enabled" : "disabled");
                }
            }

            // Send write response if needed
            if (param->write.need_rsp) {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
                                            param->write.trans_id, ESP_GATT_OK, NULL);
            }
            break;

        case ESP_GATTS_READ_EVT:
            // Handle read requests (CP Length and Service Revision have static values)
            LOG_D(GATTS_TAG, "Read request, handle: %d", param->read.handle);
            break;

        case ESP_GATTS_CONF_EVT:
            // Notification confirmed, send next fragment if available
            if (g_tx_offset < g_tx_len) {
                send_response_fragment();
            }
            break;

        default:
            break;
    }
}

// ============================================================================
// BLE Frame Processing
// ============================================================================

static void reset_rx_state(void) {
    g_rx_offset = 0;
    g_rx_expected_len = 0;
    g_rx_in_progress = false;
}

static void process_control_point_write(const uint8_t *data, uint16_t len) {
    if (len < 1) return;

    uint8_t frame_type = data[0];

    if (frame_type == BLE_CTAP_FRAME_CANCEL) {
        // Cancel command
        LOG_D(GATTS_TAG, "Cancel received");
        ctap2_cancel();
        reset_rx_state();
        return;
    }

    if (frame_type == BLE_CTAP_FRAME_INIT) {
        // Initial frame: [0x83] [LEN_HI] [LEN_LO] [DATA...]
        if (len < 3) {
            LOG_W(GATTS_TAG, "Init frame too short");
            return;
        }

        g_rx_expected_len = (data[1] << 8) | data[2];
        if (g_rx_expected_len > BLE_CTAP_MAX_MSG_SIZE) {
            LOG_E(GATTS_TAG, "Message too large: %d", g_rx_expected_len);
            reset_rx_state();
            return;
        }

        g_rx_offset = 0;
        g_rx_in_progress = true;

        uint16_t payload_len = len - 3;
        if (payload_len > g_rx_expected_len) payload_len = g_rx_expected_len;
        memcpy(g_rx_buffer, data + 3, payload_len);
        g_rx_offset = payload_len;

        LOG_D(GATTS_TAG, "Init frame: expected=%d, received=%d", g_rx_expected_len, g_rx_offset);
    }
    else if ((frame_type & 0x80) == 0) {
        // Continuation frame: [SEQ] [DATA...]
        if (!g_rx_in_progress) {
            LOG_W(GATTS_TAG, "Unexpected continuation frame");
            return;
        }

        uint16_t payload_len = len - 1;
        uint16_t remaining = g_rx_expected_len - g_rx_offset;
        if (payload_len > remaining) payload_len = remaining;

        memcpy(g_rx_buffer + g_rx_offset, data + 1, payload_len);
        g_rx_offset += payload_len;

        LOG_D(GATTS_TAG, "Cont frame: seq=%d, total=%d/%d", frame_type, g_rx_offset, g_rx_expected_len);
    }
    else {
        LOG_W(GATTS_TAG, "Unknown frame type: 0x%02X", frame_type);
        return;
    }

    // Check if message complete
    if (g_rx_in_progress && g_rx_offset >= g_rx_expected_len) {
        LOG_D(GATTS_TAG, "Message complete, processing...");

        // Process CTAP2 command
        uint16_t response_len = sizeof(g_tx_buffer);
        uint8_t status = ctap2_process_command(g_rx_buffer, g_rx_expected_len,
                                                g_tx_buffer + 1, &response_len);
        g_tx_buffer[0] = status;
        g_tx_len = response_len + 1;
        g_tx_offset = 0;

        reset_rx_state();

        // Send response
        if (g_status_notify_enabled) {
            send_response_fragment();
        }
    }
}

static void send_response_fragment(void) {
    if (!g_connected || !g_status_notify_enabled || g_tx_offset >= g_tx_len) return;

    uint8_t frame[BLE_CTAP_MAX_MSG_SIZE];
    uint16_t frame_len;

    if (g_tx_offset == 0) {
        // Initial frame: [0x83] [LEN_HI] [LEN_LO] [DATA...]
        frame[0] = BLE_CTAP_FRAME_INIT;
        frame[1] = (g_tx_len >> 8) & 0xFF;
        frame[2] = g_tx_len & 0xFF;

        uint16_t payload_max = g_mtu - 3;
        uint16_t payload_len = (g_tx_len < payload_max) ? g_tx_len : payload_max;
        memcpy(frame + 3, g_tx_buffer, payload_len);
        frame_len = payload_len + 3;
        g_tx_offset = payload_len;
    }
    else {
        // Continuation frame: [SEQ] [DATA...]
        static uint8_t seq = 0;
        frame[0] = seq++;

        uint16_t remaining = g_tx_len - g_tx_offset;
        uint16_t payload_max = g_mtu - 1;
        uint16_t payload_len = (remaining < payload_max) ? remaining : payload_max;
        memcpy(frame + 1, g_tx_buffer + g_tx_offset, payload_len);
        frame_len = payload_len + 1;
        g_tx_offset += payload_len;
    }

    esp_ble_gatts_send_indicate(g_gatts_if, g_conn_id, g_status_handle,
                                 frame_len, frame, false);
}

// ============================================================================
// Public API
// ============================================================================

bool ble_ctap_init(void) {
    if (g_initialized) return true;

    LOG_I(GATTS_TAG, "Initializing BLE CTAP...");

    // Release classic BT memory (BLE only)
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    // Initialize BT controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        LOG_E(GATTS_TAG, "BT controller init failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) {
        LOG_E(GATTS_TAG, "BT controller enable failed: %s", esp_err_to_name(ret));
        return false;
    }

    // Initialize Bluedroid
    ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        LOG_E(GATTS_TAG, "Bluedroid init failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        LOG_E(GATTS_TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret));
        return false;
    }

    // Register callbacks
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret != ESP_OK) {
        LOG_E(GATTS_TAG, "GAP callback register failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret != ESP_OK) {
        LOG_E(GATTS_TAG, "GATTS callback register failed: %s", esp_err_to_name(ret));
        return false;
    }

    // Register GATT app
    ret = esp_ble_gatts_app_register(0);
    if (ret != ESP_OK) {
        LOG_E(GATTS_TAG, "GATTS app register failed: %s", esp_err_to_name(ret));
        return false;
    }

    // Set MTU
    esp_ble_gatt_set_local_mtu(517);

    g_initialized = true;
    LOG_I(GATTS_TAG, "BLE CTAP initialized");
    return true;
}

bool ble_ctap_is_initialized(void) {
    return g_initialized;
}

bool ble_ctap_is_connected(void) {
    return g_connected;
}

void ble_ctap_start_advertising(void) {
    if (g_initialized && !g_connected) {
        esp_ble_gap_start_advertising(&adv_params);
    }
}

void ble_ctap_stop_advertising(void) {
    if (g_initialized) {
        esp_ble_gap_stop_advertising();
    }
}

void ble_ctap_send_keepalive(uint8_t status) {
    if (!g_connected || !g_status_notify_enabled) return;

    uint8_t frame[2] = {BLE_CTAP_FRAME_KEEPALIVE, status};
    esp_ble_gatts_send_indicate(g_gatts_if, g_conn_id, g_status_handle, 2, frame, false);
}

void ble_ctap_deinit(void) {
    if (!g_initialized) return;

    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();

    g_initialized = false;
    g_connected = false;
    LOG_I(GATTS_TAG, "BLE CTAP deinitialized");
}

#endif // FEATURE_FIDO2_BT
