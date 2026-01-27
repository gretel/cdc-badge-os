// BLE HID Keyboard Service Implementation (HID-over-GATT Profile)
// Uses shared BLE Core for stack management

#include "ble_hid.h"

#if FEATURE_BLE_HID

#include "ble_core.h"
#include "cdc_log.h"
#include "cdc_time.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

#include <string.h>

#define BLE_HID_TAG "BLE_HID"

// GATT App ID (from ble_core.h)
#define HID_APP_ID BLE_APP_HID

// Number of GATT handles needed for HID service
// HID Service: 1 + Protocol Mode(2) + HID Info(2) + Control Point(2) + Report Map(2)
//            + Input Report(2 + CCCD + Report Ref) + Output Report(2 + Report Ref)
// Device Info: 1 + PnP ID(2) + Manufacturer(2) + Model(2)
// Total: ~22 handles
#define HID_NUM_HANDLES     16
#define DIS_NUM_HANDLES     8

// ============================================================================
// HID Keycodes (USB HID Usage Tables - Keyboard Page 0x07)
// ============================================================================

#define HID_KEY_NONE        0x00
#define HID_KEY_A           0x04
#define HID_KEY_B           0x05
#define HID_KEY_C           0x06
#define HID_KEY_D           0x07
#define HID_KEY_E           0x08
#define HID_KEY_F           0x09
#define HID_KEY_G           0x0A
#define HID_KEY_H           0x0B
#define HID_KEY_I           0x0C
#define HID_KEY_J           0x0D
#define HID_KEY_K           0x0E
#define HID_KEY_L           0x0F
#define HID_KEY_M           0x10
#define HID_KEY_N           0x11
#define HID_KEY_O           0x12
#define HID_KEY_P           0x13
#define HID_KEY_Q           0x14
#define HID_KEY_R           0x15
#define HID_KEY_S           0x16
#define HID_KEY_T           0x17
#define HID_KEY_U           0x18
#define HID_KEY_V           0x19
#define HID_KEY_W           0x1A
#define HID_KEY_X           0x1B
#define HID_KEY_Y           0x1C
#define HID_KEY_Z           0x1D
#define HID_KEY_1           0x1E
#define HID_KEY_2           0x1F
#define HID_KEY_3           0x20
#define HID_KEY_4           0x21
#define HID_KEY_5           0x22
#define HID_KEY_6           0x23
#define HID_KEY_7           0x24
#define HID_KEY_8           0x25
#define HID_KEY_9           0x26
#define HID_KEY_0           0x27
#define HID_KEY_ENTER       0x28
#define HID_KEY_ESCAPE      0x29
#define HID_KEY_BACKSPACE   0x2A
#define HID_KEY_TAB         0x2B
#define HID_KEY_SPACE       0x2C
#define HID_KEY_MINUS       0x2D
#define HID_KEY_EQUAL       0x2E
#define HID_KEY_BRACKET_L   0x2F
#define HID_KEY_BRACKET_R   0x30
#define HID_KEY_BACKSLASH   0x31
#define HID_KEY_SEMICOLON   0x33
#define HID_KEY_APOSTROPHE  0x34
#define HID_KEY_GRAVE       0x35
#define HID_KEY_COMMA       0x36
#define HID_KEY_PERIOD      0x37
#define HID_KEY_SLASH       0x38

// Modifier masks
#define MOD_LCTRL   0x01
#define MOD_LSHIFT  0x02
#define MOD_LALT    0x04
#define MOD_LGUI    0x08
#define MOD_RCTRL   0x10
#define MOD_RSHIFT  0x20
#define MOD_RALT    0x40
#define MOD_RGUI    0x80

// ============================================================================
// HID Report Descriptor (Keyboard)
// ============================================================================

static const uint8_t hid_report_map[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x06,        // Usage (Keyboard)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)

    // Modifier Keys (8 bits)
    0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
    0x19, 0xE0,        //   Usage Minimum (Left Control)
    0x29, 0xE7,        //   Usage Maximum (Right GUI)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x08,        //   Report Count (8)
    0x81, 0x02,        //   Input (Data, Variable, Absolute)

    // Reserved byte
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x01,        //   Input (Constant)

    // LED Output (Caps Lock, Num Lock, etc.)
    0x05, 0x08,        //   Usage Page (LEDs)
    0x19, 0x01,        //   Usage Minimum (Num Lock)
    0x29, 0x05,        //   Usage Maximum (Kana)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x05,        //   Report Count (5)
    0x91, 0x02,        //   Output (Data, Variable, Absolute)
    0x75, 0x03,        //   Report Size (3)
    0x95, 0x01,        //   Report Count (1)
    0x91, 0x01,        //   Output (Constant) - Padding

    // Key Array (6 keys)
    0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
    0x19, 0x00,        //   Usage Minimum (0)
    0x29, 0x65,        //   Usage Maximum (101)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x65,        //   Logical Maximum (101)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x06,        //   Report Count (6)
    0x81, 0x00,        //   Input (Data, Array)

    0xC0               // End Collection
};

// HID Information value (bcdHID=1.11, bCountryCode=0, Flags=0x02=Normally Connectable)
static const uint8_t hid_info_value[] = {0x11, 0x01, 0x00, 0x02};

// Report Reference Descriptor (Report ID, Report Type)
static const uint8_t input_report_ref[] = {0x01, 0x01};   // Report ID 1, Input
static const uint8_t output_report_ref[] = {0x01, 0x02};  // Report ID 1, Output

// Device Information values
static const char* dis_manufacturer = "CDC Badge";
static const char* dis_model = "v1.0";
static const uint8_t dis_pnp_id[] = {
    0x02,              // Vendor ID Source: USB-IF
    0xFF, 0xFF,        // Vendor ID (0xFFFF = test)
    0x01, 0x00,        // Product ID
    0x01, 0x00         // Product Version
};

// ============================================================================
// GATT Service UUIDs (16-bit standard UUIDs)
// ============================================================================

#define UUID_HID_SERVICE        0x1812
#define UUID_DIS_SERVICE        0x180A

// HID Characteristics
#define UUID_HID_PROTOCOL_MODE  0x2A4E
#define UUID_HID_REPORT         0x2A4D
#define UUID_HID_REPORT_MAP     0x2A4B
#define UUID_HID_INFO           0x2A4A
#define UUID_HID_CONTROL_POINT  0x2A4C

// Device Information Characteristics
#define UUID_DIS_MANUFACTURER   0x2A29
#define UUID_DIS_MODEL          0x2A24
#define UUID_DIS_PNP_ID         0x2A50

// Descriptors
#define UUID_CHAR_CLIENT_CONFIG 0x2902
#define UUID_REPORT_REFERENCE   0x2908

// ============================================================================
// State
// ============================================================================

static bool g_initialized = false;
static bool g_connected = false;
static uint16_t g_conn_id = 0;
static uint16_t g_mtu = 23;
static esp_gatt_if_t g_gatts_if = ESP_GATT_IF_NONE;

// HID Service handles
static uint16_t g_hid_service_handle = 0;
static uint16_t g_protocol_mode_handle = 0;
static uint16_t g_report_map_handle = 0;
static uint16_t g_hid_info_handle = 0;
static uint16_t g_control_point_handle = 0;
static uint16_t g_input_report_handle = 0;
static uint16_t g_input_report_cccd_handle = 0;
static uint16_t g_output_report_handle = 0;

// Device Information Service handles
static uint16_t g_dis_service_handle = 0;
static uint16_t g_manufacturer_handle = 0;
static uint16_t g_model_handle = 0;
static uint16_t g_pnp_id_handle = 0;

// Protocol mode (0 = Boot Protocol, 1 = Report Protocol)
static uint8_t g_protocol_mode = 1;

// Input report notifications enabled
static bool g_input_notify_enabled = false;

// TX semaphore for send confirmation
static SemaphoreHandle_t g_tx_sem = NULL;

// Service creation state machine
typedef enum {
    SVC_STATE_IDLE,
    SVC_STATE_CREATING_HID,
    SVC_STATE_CREATING_DIS,
    SVC_STATE_READY
} svc_state_t;

static svc_state_t g_svc_state = SVC_STATE_IDLE;

// ============================================================================
// Forward Declarations
// ============================================================================

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                esp_ble_gatts_cb_param_t *param);
static void create_hid_service(void);
static void create_dis_service(void);

// ============================================================================
// GATTS Event Handler
// ============================================================================

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT:
            if (param->reg.status == ESP_GATT_OK && param->reg.app_id == HID_APP_ID) {
                g_gatts_if = gatts_if;
                LOG_I(BLE_HID_TAG, "GATT server registered (app_id=%d)", HID_APP_ID);

                // Start creating services
                g_svc_state = SVC_STATE_CREATING_HID;
                create_hid_service();
            }
            break;

        case ESP_GATTS_CREATE_EVT:
            if (param->create.status == ESP_GATT_OK) {
                if (g_svc_state == SVC_STATE_CREATING_HID) {
                    g_hid_service_handle = param->create.service_handle;
                    LOG_D(BLE_HID_TAG, "HID service created, handle=%d", g_hid_service_handle);
                    esp_ble_gatts_start_service(g_hid_service_handle);

                    // Add Protocol Mode characteristic
                    esp_bt_uuid_t uuid;
                    uuid.len = ESP_UUID_LEN_16;
                    uuid.uuid.uuid16 = UUID_HID_PROTOCOL_MODE;
                    esp_ble_gatts_add_char(g_hid_service_handle, &uuid,
                                           ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                           ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE_NR,
                                           NULL, NULL);
                }
                else if (g_svc_state == SVC_STATE_CREATING_DIS) {
                    g_dis_service_handle = param->create.service_handle;
                    LOG_D(BLE_HID_TAG, "DIS service created, handle=%d", g_dis_service_handle);
                    esp_ble_gatts_start_service(g_dis_service_handle);

                    // Add Manufacturer Name characteristic
                    esp_bt_uuid_t uuid;
                    uuid.len = ESP_UUID_LEN_16;
                    uuid.uuid.uuid16 = UUID_DIS_MANUFACTURER;
                    esp_ble_gatts_add_char(g_dis_service_handle, &uuid,
                                           ESP_GATT_PERM_READ,
                                           ESP_GATT_CHAR_PROP_BIT_READ,
                                           NULL, NULL);
                }
            }
            break;

        case ESP_GATTS_ADD_CHAR_EVT:
            if (param->add_char.status != ESP_GATT_OK) break;

            if (g_svc_state == SVC_STATE_CREATING_HID) {
                esp_bt_uuid_t uuid;
                uuid.len = ESP_UUID_LEN_16;

                if (g_protocol_mode_handle == 0) {
                    g_protocol_mode_handle = param->add_char.attr_handle;
                    LOG_D(BLE_HID_TAG, "Protocol Mode added, handle=%d", g_protocol_mode_handle);

                    // Add Report Map characteristic
                    uuid.uuid.uuid16 = UUID_HID_REPORT_MAP;
                    esp_ble_gatts_add_char(g_hid_service_handle, &uuid,
                                           ESP_GATT_PERM_READ,
                                           ESP_GATT_CHAR_PROP_BIT_READ,
                                           NULL, NULL);
                }
                else if (g_report_map_handle == 0) {
                    g_report_map_handle = param->add_char.attr_handle;
                    LOG_D(BLE_HID_TAG, "Report Map added, handle=%d", g_report_map_handle);

                    // Add HID Information characteristic
                    uuid.uuid.uuid16 = UUID_HID_INFO;
                    esp_ble_gatts_add_char(g_hid_service_handle, &uuid,
                                           ESP_GATT_PERM_READ,
                                           ESP_GATT_CHAR_PROP_BIT_READ,
                                           NULL, NULL);
                }
                else if (g_hid_info_handle == 0) {
                    g_hid_info_handle = param->add_char.attr_handle;
                    LOG_D(BLE_HID_TAG, "HID Info added, handle=%d", g_hid_info_handle);

                    // Add HID Control Point characteristic
                    uuid.uuid.uuid16 = UUID_HID_CONTROL_POINT;
                    esp_ble_gatts_add_char(g_hid_service_handle, &uuid,
                                           ESP_GATT_PERM_WRITE,
                                           ESP_GATT_CHAR_PROP_BIT_WRITE_NR,
                                           NULL, NULL);
                }
                else if (g_control_point_handle == 0) {
                    g_control_point_handle = param->add_char.attr_handle;
                    LOG_D(BLE_HID_TAG, "Control Point added, handle=%d", g_control_point_handle);

                    // Add Input Report characteristic (keyboard reports)
                    uuid.uuid.uuid16 = UUID_HID_REPORT;
                    esp_ble_gatts_add_char(g_hid_service_handle, &uuid,
                                           ESP_GATT_PERM_READ_ENCRYPTED,
                                           ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                                           NULL, NULL);
                }
                else if (g_input_report_handle == 0) {
                    g_input_report_handle = param->add_char.attr_handle;
                    LOG_D(BLE_HID_TAG, "Input Report added, handle=%d", g_input_report_handle);

                    // Add CCCD for Input Report
                    uuid.uuid.uuid16 = UUID_CHAR_CLIENT_CONFIG;
                    esp_ble_gatts_add_char_descr(g_hid_service_handle, &uuid,
                                                 ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                 NULL, NULL);
                }
                else if (g_output_report_handle == 0) {
                    g_output_report_handle = param->add_char.attr_handle;
                    LOG_D(BLE_HID_TAG, "Output Report added, handle=%d", g_output_report_handle);

                    // HID service complete - create Device Information Service
                    g_svc_state = SVC_STATE_CREATING_DIS;
                    create_dis_service();
                }
            }
            else if (g_svc_state == SVC_STATE_CREATING_DIS) {
                esp_bt_uuid_t uuid;
                uuid.len = ESP_UUID_LEN_16;

                if (g_manufacturer_handle == 0) {
                    g_manufacturer_handle = param->add_char.attr_handle;

                    // Add Model Number characteristic
                    uuid.uuid.uuid16 = UUID_DIS_MODEL;
                    esp_ble_gatts_add_char(g_dis_service_handle, &uuid,
                                           ESP_GATT_PERM_READ,
                                           ESP_GATT_CHAR_PROP_BIT_READ,
                                           NULL, NULL);
                }
                else if (g_model_handle == 0) {
                    g_model_handle = param->add_char.attr_handle;

                    // Add PnP ID characteristic
                    uuid.uuid.uuid16 = UUID_DIS_PNP_ID;
                    esp_ble_gatts_add_char(g_dis_service_handle, &uuid,
                                           ESP_GATT_PERM_READ,
                                           ESP_GATT_CHAR_PROP_BIT_READ,
                                           NULL, NULL);
                }
                else if (g_pnp_id_handle == 0) {
                    g_pnp_id_handle = param->add_char.attr_handle;

                    // All services created
                    g_svc_state = SVC_STATE_READY;
                    LOG_I(BLE_HID_TAG, "HID service ready");

                    // Start advertising
                    ble_core_start_advertising();
                }
            }
            break;

        case ESP_GATTS_ADD_CHAR_DESCR_EVT:
            if (param->add_char_descr.status == ESP_GATT_OK) {
                if (g_input_report_cccd_handle == 0) {
                    g_input_report_cccd_handle = param->add_char_descr.attr_handle;
                    LOG_D(BLE_HID_TAG, "Input CCCD added, handle=%d", g_input_report_cccd_handle);

                    // Add Output Report characteristic
                    esp_bt_uuid_t uuid;
                    uuid.len = ESP_UUID_LEN_16;
                    uuid.uuid.uuid16 = UUID_HID_REPORT;
                    esp_ble_gatts_add_char(g_hid_service_handle, &uuid,
                                           ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                           ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_WRITE_NR,
                                           NULL, NULL);
                }
            }
            break;

        case ESP_GATTS_CONNECT_EVT:
            LOG_I(BLE_HID_TAG, "Client connected, conn_id=%d", param->connect.conn_id);
            g_connected = true;
            g_conn_id = param->connect.conn_id;
            g_input_notify_enabled = false;
            // Request encryption
            esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);
            break;

        case ESP_GATTS_DISCONNECT_EVT:
            LOG_I(BLE_HID_TAG, "Client disconnected");
            g_connected = false;
            g_input_notify_enabled = false;
            g_conn_id = 0;
            // Restart advertising
            ble_core_start_advertising();
            break;

        case ESP_GATTS_MTU_EVT:
            g_mtu = param->mtu.mtu - 3;
            LOG_D(BLE_HID_TAG, "MTU=%d", param->mtu.mtu);
            break;

        case ESP_GATTS_READ_EVT: {
            esp_gatt_rsp_t rsp;
            memset(&rsp, 0, sizeof(rsp));
            rsp.attr_value.handle = param->read.handle;

            if (param->read.handle == g_protocol_mode_handle) {
                rsp.attr_value.len = 1;
                rsp.attr_value.value[0] = g_protocol_mode;
            }
            else if (param->read.handle == g_report_map_handle) {
                rsp.attr_value.len = sizeof(hid_report_map);
                memcpy(rsp.attr_value.value, hid_report_map, sizeof(hid_report_map));
            }
            else if (param->read.handle == g_hid_info_handle) {
                rsp.attr_value.len = sizeof(hid_info_value);
                memcpy(rsp.attr_value.value, hid_info_value, sizeof(hid_info_value));
            }
            else if (param->read.handle == g_manufacturer_handle) {
                rsp.attr_value.len = strlen(dis_manufacturer);
                memcpy(rsp.attr_value.value, dis_manufacturer, rsp.attr_value.len);
            }
            else if (param->read.handle == g_model_handle) {
                rsp.attr_value.len = strlen(dis_model);
                memcpy(rsp.attr_value.value, dis_model, rsp.attr_value.len);
            }
            else if (param->read.handle == g_pnp_id_handle) {
                rsp.attr_value.len = sizeof(dis_pnp_id);
                memcpy(rsp.attr_value.value, dis_pnp_id, sizeof(dis_pnp_id));
            }
            else if (param->read.handle == g_input_report_handle) {
                // Return empty report
                rsp.attr_value.len = 8;
                memset(rsp.attr_value.value, 0, 8);
            }

            esp_ble_gatts_send_response(gatts_if, param->read.conn_id,
                                        param->read.trans_id, ESP_GATT_OK, &rsp);
            break;
        }

        case ESP_GATTS_WRITE_EVT:
            if (param->write.handle == g_input_report_cccd_handle && param->write.len == 2) {
                uint16_t cccd = param->write.value[0] | (param->write.value[1] << 8);
                g_input_notify_enabled = (cccd & 0x0001);
                LOG_I(BLE_HID_TAG, "Input notifications %s", g_input_notify_enabled ? "ENABLED" : "disabled");
            }
            else if (param->write.handle == g_protocol_mode_handle && param->write.len == 1) {
                g_protocol_mode = param->write.value[0];
                LOG_D(BLE_HID_TAG, "Protocol mode: %d", g_protocol_mode);
            }
            else if (param->write.handle == g_control_point_handle) {
                // HID Control Point: 0=Suspend, 1=Exit Suspend
                LOG_D(BLE_HID_TAG, "Control point: %d", param->write.value[0]);
            }
            else if (param->write.handle == g_output_report_handle) {
                // LED state from host (Caps Lock, Num Lock, etc.)
                LOG_D(BLE_HID_TAG, "LED state: 0x%02x", param->write.value[0]);
            }

            if (param->write.need_rsp) {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
                                            param->write.trans_id, ESP_GATT_OK, NULL);
            }
            break;

        case ESP_GATTS_CONF_EVT:
            // Notification confirmed
            if (g_tx_sem) {
                xSemaphoreGive(g_tx_sem);
            }
            break;

        default:
            break;
    }
}

// ============================================================================
// Service Creation
// ============================================================================

static void create_hid_service(void) {
    esp_gatt_srvc_id_t service_id;
    memset(&service_id, 0, sizeof(service_id));
    service_id.is_primary = true;
    service_id.id.inst_id = 0;
    service_id.id.uuid.len = ESP_UUID_LEN_16;
    service_id.id.uuid.uuid.uuid16 = UUID_HID_SERVICE;
    esp_ble_gatts_create_service(g_gatts_if, &service_id, HID_NUM_HANDLES);
}

static void create_dis_service(void) {
    esp_gatt_srvc_id_t service_id;
    memset(&service_id, 0, sizeof(service_id));
    service_id.is_primary = true;
    service_id.id.inst_id = 0;
    service_id.id.uuid.len = ESP_UUID_LEN_16;
    service_id.id.uuid.uuid.uuid16 = UUID_DIS_SERVICE;
    esp_ble_gatts_create_service(g_gatts_if, &service_id, DIS_NUM_HANDLES);
}

// ============================================================================
// Keyboard Input
// ============================================================================

static bool send_keyboard_report(uint8_t modifier, const uint8_t keys[6]) {
    if (!g_connected || !g_input_notify_enabled) return false;

    // Report format: [Report ID][Modifier][Reserved][Keys x 6]
    uint8_t report[9];
    report[0] = 0x01;  // Report ID
    report[1] = modifier;
    report[2] = 0x00;  // Reserved
    memcpy(&report[3], keys, 6);

    // Wait for previous send to complete
    if (g_tx_sem && xSemaphoreTake(g_tx_sem, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }

    esp_err_t ret = esp_ble_gatts_send_indicate(g_gatts_if, g_conn_id, g_input_report_handle,
                                                  sizeof(report), report, false);
    if (ret != ESP_OK) {
        if (g_tx_sem) xSemaphoreGive(g_tx_sem);
        return false;
    }

    return true;
}

// ============================================================================
// Public API
// ============================================================================

bool ble_hid_init(void) {
    if (g_initialized) return true;

    LOG_I(BLE_HID_TAG, "Initializing BLE HID...");

    // Initialize shared BLE core
    if (!ble_core_init()) {
        LOG_E(BLE_HID_TAG, "BLE core init failed");
        return false;
    }

    // Create TX semaphore
    g_tx_sem = xSemaphoreCreateBinary();
    if (!g_tx_sem) {
        LOG_E(BLE_HID_TAG, "Failed to create TX semaphore");
        return false;
    }
    xSemaphoreGive(g_tx_sem);

    // Register GATTS callback
    esp_err_t ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret != ESP_OK) {
        LOG_E(BLE_HID_TAG, "GATTS callback register failed: %s", esp_err_to_name(ret));
        return false;
    }

    // Register GATT app
    ret = esp_ble_gatts_app_register(HID_APP_ID);
    if (ret != ESP_OK) {
        LOG_E(BLE_HID_TAG, "GATTS app register failed: %s", esp_err_to_name(ret));
        return false;
    }

    g_initialized = true;
    LOG_I(BLE_HID_TAG, "BLE HID initialized");
    return true;
}

void ble_hid_deinit(void) {
    if (!g_initialized) return;

    LOG_I(BLE_HID_TAG, "Deinitializing BLE HID...");

    g_connected = false;
    g_input_notify_enabled = false;
    g_initialized = false;

    ble_core_stop_advertising();

    if (g_gatts_if != ESP_GATT_IF_NONE) {
        esp_ble_gatts_app_unregister(g_gatts_if);
        g_gatts_if = ESP_GATT_IF_NONE;
    }

    if (g_tx_sem) {
        vSemaphoreDelete(g_tx_sem);
        g_tx_sem = NULL;
    }

    // Reset handles
    g_hid_service_handle = 0;
    g_protocol_mode_handle = 0;
    g_report_map_handle = 0;
    g_hid_info_handle = 0;
    g_control_point_handle = 0;
    g_input_report_handle = 0;
    g_input_report_cccd_handle = 0;
    g_output_report_handle = 0;
    g_dis_service_handle = 0;
    g_manufacturer_handle = 0;
    g_model_handle = 0;
    g_pnp_id_handle = 0;
    g_svc_state = SVC_STATE_IDLE;

    LOG_I(BLE_HID_TAG, "BLE HID deinitialized");
}

bool ble_hid_is_initialized(void) {
    return g_initialized;
}

bool ble_hid_ready(void) {
    return g_connected && g_input_notify_enabled;
}

bool ble_hid_is_connected(void) {
    return g_connected;
}

bool ble_hid_send_key(uint8_t modifier, uint8_t keycode) {
    uint8_t keys[6] = {keycode, 0, 0, 0, 0, 0};
    if (!send_keyboard_report(modifier, keys)) return false;
    vTaskDelay(pdMS_TO_TICKS(10));
    return true;
}

void ble_hid_release_keys(void) {
    uint8_t keys[6] = {0};
    send_keyboard_report(0, keys);
}

bool ble_hid_type(const char* str) {
    if (!str || !ble_hid_ready()) return false;

    uint8_t keys[6] = {0};
    uint8_t empty[6] = {0};

    // Small delay before first keystroke
    vTaskDelay(pdMS_TO_TICKS(50));

    for (const char* p = str; *p; p++) {
        uint8_t keycode = 0;
        uint8_t modifier = 0;
        char c = *p;

        // US keyboard layout mapping
        if (c >= 'a' && c <= 'z') {
            keycode = HID_KEY_A + (c - 'a');
        } else if (c >= 'A' && c <= 'Z') {
            keycode = HID_KEY_A + (c - 'A');
            modifier = MOD_LSHIFT;
        } else if (c >= '1' && c <= '9') {
            keycode = HID_KEY_1 + (c - '1');
        } else if (c == '0') {
            keycode = HID_KEY_0;
        } else if (c == '\n' || c == '\r') {
            keycode = HID_KEY_ENTER;
        } else if (c == ' ') {
            keycode = HID_KEY_SPACE;
        } else if (c == '-') {
            keycode = HID_KEY_MINUS;
        } else if (c == '=') {
            keycode = HID_KEY_EQUAL;
        } else if (c == '[') {
            keycode = HID_KEY_BRACKET_L;
        } else if (c == ']') {
            keycode = HID_KEY_BRACKET_R;
        } else if (c == '\\') {
            keycode = HID_KEY_BACKSLASH;
        } else if (c == ';') {
            keycode = HID_KEY_SEMICOLON;
        } else if (c == '\'') {
            keycode = HID_KEY_APOSTROPHE;
        } else if (c == '`') {
            keycode = HID_KEY_GRAVE;
        } else if (c == ',') {
            keycode = HID_KEY_COMMA;
        } else if (c == '.') {
            keycode = HID_KEY_PERIOD;
        } else if (c == '/') {
            keycode = HID_KEY_SLASH;
        } else if (c == '!') {
            keycode = HID_KEY_1;
            modifier = MOD_LSHIFT;
        } else if (c == '@') {
            keycode = HID_KEY_2;
            modifier = MOD_LSHIFT;
        } else if (c == '#') {
            keycode = HID_KEY_3;
            modifier = MOD_LSHIFT;
        } else if (c == '$') {
            keycode = HID_KEY_4;
            modifier = MOD_LSHIFT;
        } else if (c == '%') {
            keycode = HID_KEY_5;
            modifier = MOD_LSHIFT;
        } else if (c == '^') {
            keycode = HID_KEY_6;
            modifier = MOD_LSHIFT;
        } else if (c == '&') {
            keycode = HID_KEY_7;
            modifier = MOD_LSHIFT;
        } else if (c == '*') {
            keycode = HID_KEY_8;
            modifier = MOD_LSHIFT;
        } else if (c == '(') {
            keycode = HID_KEY_9;
            modifier = MOD_LSHIFT;
        } else if (c == ')') {
            keycode = HID_KEY_0;
            modifier = MOD_LSHIFT;
        } else if (c == '_') {
            keycode = HID_KEY_MINUS;
            modifier = MOD_LSHIFT;
        } else if (c == '+') {
            keycode = HID_KEY_EQUAL;
            modifier = MOD_LSHIFT;
        } else if (c == ':') {
            keycode = HID_KEY_SEMICOLON;
            modifier = MOD_LSHIFT;
        } else if (c == '"') {
            keycode = HID_KEY_APOSTROPHE;
            modifier = MOD_LSHIFT;
        } else if (c == '<') {
            keycode = HID_KEY_COMMA;
            modifier = MOD_LSHIFT;
        } else if (c == '>') {
            keycode = HID_KEY_PERIOD;
            modifier = MOD_LSHIFT;
        } else if (c == '?') {
            keycode = HID_KEY_SLASH;
            modifier = MOD_LSHIFT;
        } else if (c == '{') {
            keycode = HID_KEY_BRACKET_L;
            modifier = MOD_LSHIFT;
        } else if (c == '}') {
            keycode = HID_KEY_BRACKET_R;
            modifier = MOD_LSHIFT;
        } else if (c == '|') {
            keycode = HID_KEY_BACKSLASH;
            modifier = MOD_LSHIFT;
        } else if (c == '~') {
            keycode = HID_KEY_GRAVE;
            modifier = MOD_LSHIFT;
        } else if (c == '\t') {
            keycode = HID_KEY_TAB;
        }

        if (keycode) {
            // Key down
            keys[0] = keycode;
            if (!send_keyboard_report(modifier, keys)) return false;
            vTaskDelay(pdMS_TO_TICKS(20));

            // Key up
            keys[0] = 0;
            if (!send_keyboard_report(0, empty)) return false;
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    return true;
}

bool ble_hid_type_with_enter(const char* str, bool press_enter) {
    if (!ble_hid_type(str)) return false;
    if (press_enter) {
        return ble_hid_press_enter();
    }
    return true;
}

bool ble_hid_press_enter(void) {
    if (!ble_hid_ready()) return false;

    uint8_t keys[6] = {HID_KEY_ENTER, 0, 0, 0, 0, 0};
    uint8_t empty[6] = {0};

    if (!send_keyboard_report(0, keys)) return false;
    vTaskDelay(pdMS_TO_TICKS(20));
    send_keyboard_report(0, empty);

    return true;
}

#endif // FEATURE_BLE_HID
