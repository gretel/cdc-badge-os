#include "ble_badge.h"

#if FEATURE_BLE_BADGE

#include "ble_core.h"
#include "badge_settings.h"
#include "cdc_log.h"
#include "cdc_time.h"
#include "vcard_store.h"

#if FEATURE_GPG
#include "gpg.h"
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gattc_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_err.h"

#include <string.h>

#define BLE_BADGE_TAG "BLE_BADGE"
#define MAX_PEERS 16
#define BLACKLIST_MAX 32
#define BLACKLIST_TTL_MS (60 * 60 * 1000)
#define BLE_BADGE_APP_ID BLE_APP_BADGE  // From ble_core.h
#define BLE_BADGE_TX_MTU_DEFAULT 23
#define BLE_BADGE_TX_PAYLOAD_DEFAULT 20
#define EXCHANGE_TIMEOUT_MS 30000

#define VCARD_OP_WRITE_START 0x01
#define VCARD_OP_WRITE_CONT  0x02
#define VCARD_OP_WRITE_END   0x03

#define VCARD_OP_DATA_START  0x81
#define VCARD_OP_DATA_CONT   0x82
#define VCARD_OP_DATA_END    0x83

#if FEATURE_GPG
// GPG Key Exchange Opcodes (separate namespace from vCard)
#define GPG_OP_WRITE_START   0x11
#define GPG_OP_WRITE_CONT    0x12
#define GPG_OP_WRITE_END     0x13

#define GPG_OP_DATA_START    0x91
#define GPG_OP_DATA_CONT     0x92
#define GPG_OP_DATA_END      0x93

// GPG payload format:
// [1]  curve
// [1]  pubkey_len
// [N]  pubkey (32 or 64 bytes)
// [20] fingerprint
// [1]  user_id_len
// [N]  user_id (max 63 bytes)
// Total max: 1+1+64+20+1+63 = 150 bytes
#define GPG_PAYLOAD_MAX_LEN  150
#endif

static bool g_initialized = false;
static bool g_adv_requested = false;
static bool g_scan_requested = false;
static bool g_adv_active = false;
static bool g_scan_active = false;
static bool g_receive_enabled = false;
static bool g_exchange_enabled = false;

static uint32_t g_adv_interval_ms = 60000;
static uint32_t g_scan_window_ms = 10000;
static uint32_t g_scan_pause_ms = 30000;
static int8_t g_rssi_threshold = -75;

static TimerHandle_t g_adv_timer = nullptr;
static TimerHandle_t g_adv_stop_timer = nullptr;
static TimerHandle_t g_scan_timer = nullptr;
static TimerHandle_t g_scan_stop_timer = nullptr;

static ble_badge_peer_t g_peers[MAX_PEERS];
static uint32_t g_peers_last_seen[MAX_PEERS];

static ble_badge_peer_t g_pending_nearby;
static bool g_pending_nearby_set = false;

static esp_gatt_if_t g_gatts_if = ESP_GATT_IF_NONE;
static esp_gatt_if_t g_gattc_if = ESP_GATT_IF_NONE;
static uint16_t g_service_handle = 0;
static uint16_t g_char_data_handle = 0;
static uint16_t g_char_ctrl_handle = 0;
static uint16_t g_char_rx_handle = 0;
static uint16_t g_char_status_handle = 0;
static uint16_t g_char_data_ccc_handle = 0;
static uint16_t g_char_status_ccc_handle = 0;
static bool g_data_indicate_enabled = false;
static bool g_status_indicate_enabled = false;

static bool g_gatts_connected = false;
static uint16_t g_gatts_conn_id = 0;
static uint16_t g_gatts_mtu_payload = BLE_BADGE_TX_PAYLOAD_DEFAULT;
static esp_bd_addr_t g_gatts_bda = {0};
static esp_bd_addr_t g_own_bda = {0};  // Own BLE address to filter self from scan

static bool g_pairing_pending = false;
static bool g_pairing_passkey_req = false;
static bool g_pairing_display_only = false;
static uint32_t g_pairing_passkey = 0;
static esp_bd_addr_t g_pairing_addr = {0};

static bool g_srv_tx_active = false;
static size_t g_srv_tx_len = 0;
static size_t g_srv_tx_offset = 0;
static char g_srv_tx_buf[VCARD_MAX_LEN + 1];
static bool g_srv_rx_active = false;
static size_t g_srv_rx_len = 0;
static size_t g_srv_rx_expected = 0;
static char g_srv_rx_buf[VCARD_MAX_LEN + 1];

typedef enum {
    EXCHANGE_IDLE = 0,
    EXCHANGE_CONNECTING,
    EXCHANGE_DISCOVERING,
    EXCHANGE_ENABLING_NOTIFY,
    EXCHANGE_REQUESTING_REMOTE,
    EXCHANGE_RECEIVING_REMOTE,
    EXCHANGE_SENDING_LOCAL,
    EXCHANGE_WAITING_ACK,
} exchange_state_t;

static exchange_state_t g_exchange_state = EXCHANGE_IDLE;
static bool g_exchange_result_pending = false;
static bool g_exchange_result_success = false;
static TimerHandle_t g_exchange_timer = nullptr;
static bool g_exchange_restore_adv = false;
static bool g_exchange_restore_scan = false;
static bool g_exchange_force_receive = false;

static uint16_t g_exchange_conn_id = 0;
static esp_bd_addr_t g_exchange_addr = {0};
static uint16_t g_exchange_mtu_payload = BLE_BADGE_TX_PAYLOAD_DEFAULT;
static uint16_t g_remote_service_start = 0;
static uint16_t g_remote_service_end = 0;
static uint16_t g_remote_data_handle = 0;
static uint16_t g_remote_ctrl_handle = 0;
static uint16_t g_remote_rx_handle = 0;
static uint16_t g_remote_status_handle = 0;
static uint16_t g_remote_data_ccc_handle = 0;
static uint16_t g_remote_status_ccc_handle = 0;
static bool g_remote_data_ccc_set = false;
static bool g_remote_status_ccc_set = false;

static size_t g_remote_rx_len = 0;
static size_t g_remote_rx_expected = 0;
static char g_remote_rx_buf[VCARD_MAX_LEN + 1];

static size_t g_local_tx_len = 0;
static size_t g_local_tx_offset = 0;
static char g_local_tx_buf[VCARD_MAX_LEN + 1];

#if FEATURE_GPG
// GPG exchange state (reuses vCard exchange infrastructure)
static bool g_gpg_exchange_mode = false;
static bool g_gpg_exchange_result_pending = false;
static bool g_gpg_exchange_result_success = false;
static uint8_t g_gpg_rx_buf[GPG_PAYLOAD_MAX_LEN];
static size_t g_gpg_rx_len = 0;
static size_t g_gpg_rx_expected = 0;
static uint8_t g_gpg_tx_buf[GPG_PAYLOAD_MAX_LEN];
static size_t g_gpg_tx_len = 0;
static size_t g_gpg_tx_offset = 0;
#endif

typedef struct {
    bool used;
    uint8_t addr[6];
    uint32_t last_notify_ms;
} ble_badge_blacklist_t;

static ble_badge_blacklist_t g_blacklist[BLACKLIST_MAX];

static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0xA0,     // 100ms
    .adv_int_max        = 0x140,    // 200ms
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .peer_addr          = {0},
    .peer_addr_type     = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static esp_ble_scan_params_t scan_params = {
    .scan_type          = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval      = 0x50,
    .scan_window        = 0x30,
    .scan_duplicate     = BLE_SCAN_DUPLICATE_DISABLE
};

static esp_ble_adv_data_t adv_data = {
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

static esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp        = true,
    .include_name        = false,
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

// Forward declarations
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                esp_ble_gatts_cb_param_t *param);
static void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                esp_ble_gattc_cb_param_t *param);
static void exchange_timeout_cb(TimerHandle_t timer);
static void exchange_finish(bool success);
static void exchange_reset_remote(void);
static void exchange_reset_local(void);
static void server_send_next_chunk(void);
static void client_send_next_chunk(void);
static void update_adv_state(void);
static void update_scan_state(void);
#if FEATURE_GPG
static bool gpg_deserialize_and_store(const uint8_t *buf, size_t len);
#endif

// UUIDs (128-bit, little-endian)
static const uint8_t vcard_service_uuid[16] = {
    0x01, 0x1A, 0x8B, 0x6A, 0x9D, 0x4C, 0x6E, 0x9A,
    0x7A, 0x4D, 0x5D, 0x8B, 0x20, 0x1F, 0x2F, 0x8E
};
static const uint8_t vcard_data_uuid[16] = {
    0x01, 0x1A, 0x8B, 0x6A, 0x9D, 0x4C, 0x6E, 0x9A,
    0x7A, 0x4D, 0x5D, 0x8B, 0x21, 0x1F, 0x2F, 0x8E
};
static const uint8_t vcard_ctrl_uuid[16] = {
    0x01, 0x1A, 0x8B, 0x6A, 0x9D, 0x4C, 0x6E, 0x9A,
    0x7A, 0x4D, 0x5D, 0x8B, 0x22, 0x1F, 0x2F, 0x8E
};
static const uint8_t vcard_rx_uuid[16] = {
    0x01, 0x1A, 0x8B, 0x6A, 0x9D, 0x4C, 0x6E, 0x9A,
    0x7A, 0x4D, 0x5D, 0x8B, 0x23, 0x1F, 0x2F, 0x8E
};
static const uint8_t vcard_status_uuid[16] = {
    0x01, 0x1A, 0x8B, 0x6A, 0x9D, 0x4C, 0x6E, 0x9A,
    0x7A, 0x4D, 0x5D, 0x8B, 0x24, 0x1F, 0x2F, 0x8E
};

static void update_adv_data(void) {
    if (!g_initialized) return;
    const char *name = badge_settings_get_name();
    const char *slogan = badge_settings_get_info();
    esp_ble_gap_set_device_name(name ? name : "Badge");

    static uint8_t mfg_data[24];
    size_t slogan_len = 0;
    if (g_adv_requested && slogan) {
        slogan_len = strnlen(slogan, 12);
    }
    memset(mfg_data, 0, sizeof(mfg_data));
    if (slogan_len > 0) {
        memcpy(mfg_data, slogan, slogan_len);
    }
    adv_data.manufacturer_len = (uint16_t)slogan_len;
    adv_data.p_manufacturer_data = slogan_len ? mfg_data : NULL;
    scan_rsp_data.manufacturer_len = 0;
    scan_rsp_data.p_manufacturer_data = NULL;

    if (g_exchange_enabled) {
        scan_rsp_data.service_uuid_len = sizeof(vcard_service_uuid);
        scan_rsp_data.p_service_uuid = (uint8_t *)vcard_service_uuid;
    } else {
        scan_rsp_data.service_uuid_len = 0;
        scan_rsp_data.p_service_uuid = NULL;
    }

    esp_ble_gap_config_adv_data(&adv_data);
    esp_ble_gap_config_adv_data(&scan_rsp_data);
}

static void exchange_reset_remote(void) {
    g_remote_service_start = 0;
    g_remote_service_end = 0;
    g_remote_data_handle = 0;
    g_remote_ctrl_handle = 0;
    g_remote_rx_handle = 0;
    g_remote_status_handle = 0;
    g_remote_data_ccc_handle = 0;
    g_remote_status_ccc_handle = 0;
    g_remote_data_ccc_set = false;
    g_remote_status_ccc_set = false;
    g_remote_rx_len = 0;
    g_remote_rx_expected = 0;
}

static void exchange_reset_local(void) {
    g_local_tx_len = 0;
    g_local_tx_offset = 0;
    g_exchange_mtu_payload = BLE_BADGE_TX_PAYLOAD_DEFAULT;
}

static void exchange_finish(bool success) {
    if (g_exchange_state == EXCHANGE_IDLE) return;
    g_exchange_state = EXCHANGE_IDLE;
    g_exchange_result_pending = true;
    g_exchange_result_success = success;
    g_exchange_force_receive = false;

#if FEATURE_GPG
    // Handle GPG exchange result
    if (g_gpg_exchange_mode) {
        g_gpg_exchange_result_pending = true;
        g_gpg_exchange_result_success = success;
        g_gpg_exchange_mode = false;
        g_gpg_tx_len = 0;
        g_gpg_tx_offset = 0;
        g_gpg_rx_len = 0;
        g_gpg_rx_expected = 0;
    }
#endif

    if (g_exchange_timer) {
        xTimerStop(g_exchange_timer, 0);
    }

    if (g_gattc_if != ESP_GATT_IF_NONE && g_exchange_conn_id != 0) {
        esp_ble_gattc_close(g_gattc_if, g_exchange_conn_id);
    }
    g_exchange_conn_id = 0;

    ble_badge_set_adv_enabled(g_exchange_restore_adv);
    ble_badge_set_scan_enabled(g_exchange_restore_scan);
    g_exchange_restore_adv = false;
    g_exchange_restore_scan = false;

    exchange_reset_remote();
    exchange_reset_local();
}

static void exchange_timeout_cb(TimerHandle_t) {
    exchange_finish(false);
}

static void server_send_next_chunk(void) {
    if (!g_srv_tx_active || !g_gatts_connected || !g_data_indicate_enabled) return;
    if (g_srv_tx_offset >= g_srv_tx_len) {
        g_srv_tx_active = false;
        return;
    }

    uint8_t buf[64];
    size_t max_payload = g_gatts_mtu_payload > 0 ? g_gatts_mtu_payload : BLE_BADGE_TX_PAYLOAD_DEFAULT;
    if (max_payload > sizeof(buf)) max_payload = sizeof(buf);

    size_t remaining = g_srv_tx_len - g_srv_tx_offset;
    size_t hdr_len = 1;
    uint8_t opcode = VCARD_OP_DATA_CONT;
    if (g_srv_tx_offset == 0) {
        opcode = VCARD_OP_DATA_START;
        hdr_len = 3;
    } else if (remaining <= (max_payload - 1)) {
        opcode = VCARD_OP_DATA_END;
        hdr_len = 1;
    }

    size_t copy_len = remaining;
    if (copy_len > (max_payload - hdr_len)) {
        copy_len = max_payload - hdr_len;
    }

    buf[0] = opcode;
    size_t idx = 1;
    if (opcode == VCARD_OP_DATA_START) {
        buf[idx++] = (uint8_t)(g_srv_tx_len & 0xFF);
        buf[idx++] = (uint8_t)((g_srv_tx_len >> 8) & 0xFF);
    }
    memcpy(&buf[idx], &g_srv_tx_buf[g_srv_tx_offset], copy_len);
    g_srv_tx_offset += copy_len;
    size_t total_len = idx + copy_len;

    esp_ble_gatts_send_indicate(g_gatts_if, g_gatts_conn_id, g_char_data_handle,
                                total_len, buf, true);

    if (g_srv_tx_offset >= g_srv_tx_len && opcode == VCARD_OP_DATA_END) {
        g_srv_tx_active = false;
    }
}

static void client_send_next_chunk(void) {
    if (g_exchange_state != EXCHANGE_SENDING_LOCAL) return;

#if FEATURE_GPG
    // GPG mode uses separate buffers and opcodes
    if (g_gpg_exchange_mode) {
        if (g_gpg_tx_offset >= g_gpg_tx_len) {
            g_exchange_state = EXCHANGE_WAITING_ACK;
            return;
        }
        if (g_remote_rx_handle == 0) {
            exchange_finish(false);
            return;
        }

        uint8_t buf[64];
        size_t max_payload = g_exchange_mtu_payload > 0 ? g_exchange_mtu_payload : BLE_BADGE_TX_PAYLOAD_DEFAULT;
        if (max_payload > sizeof(buf)) max_payload = sizeof(buf);

        size_t remaining = g_gpg_tx_len - g_gpg_tx_offset;
        size_t hdr_len = 1;
        uint8_t opcode = GPG_OP_WRITE_CONT;
        if (g_gpg_tx_offset == 0) {
            opcode = GPG_OP_WRITE_START;
            hdr_len = 3;
        } else if (remaining <= (max_payload - 1)) {
            opcode = GPG_OP_WRITE_END;
            hdr_len = 1;
        }

        size_t copy_len = remaining;
        if (copy_len > (max_payload - hdr_len)) {
            copy_len = max_payload - hdr_len;
        }

        buf[0] = opcode;
        size_t idx = 1;
        if (opcode == GPG_OP_WRITE_START) {
            buf[idx++] = (uint8_t)(g_gpg_tx_len & 0xFF);
            buf[idx++] = (uint8_t)((g_gpg_tx_len >> 8) & 0xFF);
        }
        memcpy(&buf[idx], &g_gpg_tx_buf[g_gpg_tx_offset], copy_len);
        g_gpg_tx_offset += copy_len;
        size_t total_len = idx + copy_len;

        esp_ble_gattc_write_char(g_gattc_if, g_exchange_conn_id, g_remote_rx_handle,
                                 total_len, buf, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_MITM);
        return;
    }
#endif

    // vCard mode (original logic)
    if (g_local_tx_offset >= g_local_tx_len) {
        g_exchange_state = EXCHANGE_WAITING_ACK;
        return;
    }
    if (g_remote_rx_handle == 0) {
        exchange_finish(false);
        return;
    }

    uint8_t buf[64];
    size_t max_payload = g_exchange_mtu_payload > 0 ? g_exchange_mtu_payload : BLE_BADGE_TX_PAYLOAD_DEFAULT;
    if (max_payload > sizeof(buf)) max_payload = sizeof(buf);

    size_t remaining = g_local_tx_len - g_local_tx_offset;
    size_t hdr_len = 1;
    uint8_t opcode = VCARD_OP_WRITE_CONT;
    if (g_local_tx_offset == 0) {
        opcode = VCARD_OP_WRITE_START;
        hdr_len = 3;
    } else if (remaining <= (max_payload - 1)) {
        opcode = VCARD_OP_WRITE_END;
        hdr_len = 1;
    }

    size_t copy_len = remaining;
    if (copy_len > (max_payload - hdr_len)) {
        copy_len = max_payload - hdr_len;
    }

    buf[0] = opcode;
    size_t idx = 1;
    if (opcode == VCARD_OP_WRITE_START) {
        buf[idx++] = (uint8_t)(g_local_tx_len & 0xFF);
        buf[idx++] = (uint8_t)((g_local_tx_len >> 8) & 0xFF);
    }
    memcpy(&buf[idx], &g_local_tx_buf[g_local_tx_offset], copy_len);
    g_local_tx_offset += copy_len;
    size_t total_len = idx + copy_len;

    esp_ble_gattc_write_char(g_gattc_if, g_exchange_conn_id, g_remote_rx_handle,
                             total_len, buf, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_MITM);
}

static void server_send_status(uint8_t status) {
    if (!g_gatts_connected || !g_status_indicate_enabled) return;
    esp_ble_gatts_send_indicate(g_gatts_if, g_gatts_conn_id, g_char_status_handle,
                                1, &status, true);
}

static void server_handle_rx_chunk(const uint8_t *data, size_t len) {
    if (!data || len == 0) return;
    uint8_t opcode = data[0];
    size_t idx = 1;

#if FEATURE_GPG
    // Route GPG opcodes to dedicated handler
    if (opcode == GPG_OP_WRITE_START || opcode == GPG_OP_WRITE_CONT || opcode == GPG_OP_WRITE_END) {
        // Forward declaration - defined in GPG section
        extern void server_handle_gpg_rx_chunk(const uint8_t *data, size_t len);
        server_handle_gpg_rx_chunk(data, len);
        return;
    }
#endif

    if (opcode == VCARD_OP_WRITE_START) {
        if (len < 3) return;
        g_srv_rx_expected = (size_t)data[1] | ((size_t)data[2] << 8);
        g_srv_rx_len = 0;
        g_srv_rx_active = true;
        idx = 3;
        if (g_srv_rx_expected > VCARD_MAX_LEN) {
            g_srv_rx_active = false;
            g_srv_rx_expected = 0;
            server_send_status(0x00);
            return;
        }
    } else if (!g_srv_rx_active) {
        return;
    }

    if (idx < len && g_srv_rx_len < VCARD_MAX_LEN) {
        size_t copy_len = len - idx;
        if (copy_len > (VCARD_MAX_LEN - g_srv_rx_len)) {
            copy_len = VCARD_MAX_LEN - g_srv_rx_len;
        }
        memcpy(&g_srv_rx_buf[g_srv_rx_len], &data[idx], copy_len);
        g_srv_rx_len += copy_len;
    }

    bool done = (g_srv_rx_expected > 0 && g_srv_rx_len >= g_srv_rx_expected) || (opcode == VCARD_OP_WRITE_END);
    if (done) {
        g_srv_rx_active = false;
        g_srv_rx_buf[g_srv_rx_len] = '\0';
        char err[64] = {0};
        bool ok = vcard_store_add(g_srv_rx_buf, g_srv_rx_len, err, sizeof(err));
        if (!ok && strncmp(err, "Duplicate", 9) == 0) {
            ok = true;
        }
        server_send_status(ok ? 0x01 : 0x00);
        g_srv_rx_len = 0;
        g_srv_rx_expected = 0;
    }
}

static void client_handle_data_chunk(const uint8_t *data, size_t len) {
    if (!data || len == 0) return;
    uint8_t opcode = data[0];
    size_t idx = 1;

#if FEATURE_GPG
    // Handle GPG data chunks (Server → Client response)
    if (g_gpg_exchange_mode) {
        if (opcode == GPG_OP_DATA_START) {
            if (len < 3) return;
            g_gpg_rx_expected = (size_t)data[1] | ((size_t)data[2] << 8);
            g_gpg_rx_len = 0;
            idx = 3;
            if (g_gpg_rx_expected > GPG_PAYLOAD_MAX_LEN) {
                exchange_finish(false);
                return;
            }
        }

        if (idx < len && g_gpg_rx_len < GPG_PAYLOAD_MAX_LEN) {
            size_t copy_len = len - idx;
            if (copy_len > (GPG_PAYLOAD_MAX_LEN - g_gpg_rx_len)) {
                copy_len = GPG_PAYLOAD_MAX_LEN - g_gpg_rx_len;
            }
            memcpy(&g_gpg_rx_buf[g_gpg_rx_len], &data[idx], copy_len);
            g_gpg_rx_len += copy_len;
        }

        bool done = (g_gpg_rx_expected > 0 && g_gpg_rx_len >= g_gpg_rx_expected) ||
                    (opcode == GPG_OP_DATA_END);
        if (done) {
            // Store received GPG key
            bool ok = gpg_deserialize_and_store(g_gpg_rx_buf, g_gpg_rx_len);
            if (!ok) {
                exchange_finish(false);
                return;
            }
            g_gpg_rx_len = 0;
            g_gpg_rx_expected = 0;
            // Now send our GPG key (already serialized in g_gpg_tx_buf)
            g_exchange_state = EXCHANGE_SENDING_LOCAL;
            client_send_next_chunk();
        }
        return;
    }
#endif

    // vCard mode (original logic)
    if (opcode == VCARD_OP_DATA_START) {
        if (len < 3) return;
        g_remote_rx_expected = (size_t)data[1] | ((size_t)data[2] << 8);
        g_remote_rx_len = 0;
        idx = 3;
        if (g_remote_rx_expected > VCARD_MAX_LEN) {
            exchange_finish(false);
            return;
        }
    }

    if (idx < len && g_remote_rx_len < VCARD_MAX_LEN) {
        size_t copy_len = len - idx;
        if (copy_len > (VCARD_MAX_LEN - g_remote_rx_len)) {
            copy_len = VCARD_MAX_LEN - g_remote_rx_len;
        }
        memcpy(&g_remote_rx_buf[g_remote_rx_len], &data[idx], copy_len);
        g_remote_rx_len += copy_len;
    }

    bool done = (g_remote_rx_expected > 0 && g_remote_rx_len >= g_remote_rx_expected) || (opcode == VCARD_OP_DATA_END);
    if (done) {
        g_remote_rx_buf[g_remote_rx_len] = '\0';
        char err[64] = {0};
        bool ok = vcard_store_add(g_remote_rx_buf, g_remote_rx_len, err, sizeof(err));
        if (!ok && strncmp(err, "Duplicate", 9) == 0) {
            ok = true;
        }
        if (!ok) {
            exchange_finish(false);
            return;
        }

        if (vcard_store_get_own(g_local_tx_buf, sizeof(g_local_tx_buf)) == 0) {
            exchange_finish(false);
            return;
        }
        g_remote_rx_len = 0;
        g_remote_rx_expected = 0;
        g_local_tx_len = strnlen(g_local_tx_buf, VCARD_MAX_LEN);
        g_local_tx_offset = 0;
        g_exchange_state = EXCHANGE_SENDING_LOCAL;
        client_send_next_chunk();
    }
}

static bool adv_has_service_uuid(const uint8_t *data, uint8_t len, const uint8_t uuid[16]) {
    if (!data || len == 0) return false;
    uint8_t idx = 0;
    while (idx < len) {
        uint8_t field_len = data[idx];
        if (field_len == 0) break;
        if (idx + field_len >= len) break;
        uint8_t type = data[idx + 1];
        if (type == ESP_BLE_AD_TYPE_128SRV_CMPL || type == ESP_BLE_AD_TYPE_128SRV_PART) {
            uint8_t count = (field_len - 1) / 16;
            const uint8_t *p = &data[idx + 2];
            for (uint8_t i = 0; i < count; i++) {
                if (memcmp(p + i * 16, uuid, 16) == 0) {
                    return true;
                }
            }
        }
        idx += field_len + 1;
    }
    return false;
}

static bool blacklist_allows(const uint8_t addr[6]) {
    uint32_t now = millis();
    for (int i = 0; i < BLACKLIST_MAX; i++) {
        if (!g_blacklist[i].used) continue;
        if (memcmp(g_blacklist[i].addr, addr, 6) == 0) {
            if (now - g_blacklist[i].last_notify_ms < BLACKLIST_TTL_MS) {
                return false;
            }
            g_blacklist[i].last_notify_ms = now;
            return true;
        }
    }

    // Insert new entry
    for (int i = 0; i < BLACKLIST_MAX; i++) {
        if (!g_blacklist[i].used) {
            g_blacklist[i].used = true;
            memcpy(g_blacklist[i].addr, addr, 6);
            g_blacklist[i].last_notify_ms = now;
            return true;
        }
    }
    return true;  // If blacklist full, allow
}

static int find_peer(const uint8_t addr[6]) {
    for (int i = 0; i < MAX_PEERS; i++) {
        if (g_peers[i].name[0] == '\0') continue;
        if (memcmp(g_peers[i].addr, addr, 6) == 0) return i;
    }
    return -1;
}

static int alloc_peer_slot(void) {
    for (int i = 0; i < MAX_PEERS; i++) {
        if (g_peers[i].name[0] == '\0') return i;
    }
    return 0; // overwrite oldest
}

static void handle_scan_result(const esp_ble_gap_cb_param_t *param) {
    auto *scan = &param->scan_rst;
    if (scan->rssi < g_rssi_threshold) return;

    // Filter out own device from scan results
    // Also filter if own_bda is all zeros (not yet initialized)
    bool own_bda_valid = false;
    for (int i = 0; i < 6; i++) {
        if (g_own_bda[i] != 0) { own_bda_valid = true; break; }
    }
    if (own_bda_valid && memcmp(scan->bda, g_own_bda, sizeof(esp_bd_addr_t)) == 0) {
        LOG_D(BLE_BADGE_TAG, "Filtered self from scan");
        return;
    }

    uint8_t name_len = 0;
    uint8_t *name = esp_ble_resolve_adv_data((uint8_t *)scan->ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &name_len);
    char name_buf[BLE_BADGE_NAME_MAX] = {0};
    if (name && name_len > 0) {
        size_t copy_len = name_len < (BLE_BADGE_NAME_MAX - 1) ? name_len : (BLE_BADGE_NAME_MAX - 1);
        memcpy(name_buf, name, copy_len);
        name_buf[copy_len] = '\0';
    }

    uint8_t slogan_len = 0;
    uint8_t *slogan = esp_ble_resolve_adv_data((uint8_t *)scan->ble_adv, ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE, &slogan_len);
    char slogan_buf[BLE_BADGE_SLOGAN_MAX] = {0};
    if (slogan && slogan_len > 0) {
        size_t copy_len = slogan_len < (BLE_BADGE_SLOGAN_MAX - 1) ? slogan_len : (BLE_BADGE_SLOGAN_MAX - 1);
        memcpy(slogan_buf, slogan, copy_len);
        slogan_buf[copy_len] = '\0';
    }

    int idx = find_peer(scan->bda);
    if (idx < 0) idx = alloc_peer_slot();

    strncpy(g_peers[idx].name, name_buf, sizeof(g_peers[idx].name) - 1);
    g_peers[idx].name[sizeof(g_peers[idx].name) - 1] = '\0';
    strncpy(g_peers[idx].slogan, slogan_buf, sizeof(g_peers[idx].slogan) - 1);
    g_peers[idx].slogan[sizeof(g_peers[idx].slogan) - 1] = '\0';
    g_peers[idx].rssi = scan->rssi;
    memcpy(g_peers[idx].addr, scan->bda, 6);
    g_peers[idx].exchange_ready = adv_has_service_uuid(scan->ble_adv,
                                                       scan->adv_data_len + scan->scan_rsp_len,
                                                       vcard_service_uuid);
    g_peers_last_seen[idx] = millis();

    // Only trigger nearby alert for CDC Badge devices (with vCard service UUID)
    if (!g_pending_nearby_set && g_peers[idx].exchange_ready && blacklist_allows(scan->bda)) {
        LOG_I(BLE_BADGE_TAG, "Nearby CDC Badge: %s BDA=%02X:%02X:%02X:%02X:%02X:%02X RSSI=%d",
              name_buf, scan->bda[0], scan->bda[1], scan->bda[2],
              scan->bda[3], scan->bda[4], scan->bda[5], scan->rssi);
        g_pending_nearby = g_peers[idx];
        g_pending_nearby_set = true;
    }
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_SCAN_RESULT_EVT:
            if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
                handle_scan_result(param);
            }
            break;
        case ESP_GAP_BLE_SEC_REQ_EVT:
            esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
            break;
        case ESP_GAP_BLE_NC_REQ_EVT:
            g_pairing_pending = true;
            g_pairing_passkey = param->ble_security.key_notif.passkey;
            memcpy(g_pairing_addr, param->ble_security.ble_req.bd_addr, sizeof(esp_bd_addr_t));
            break;
        case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:
            g_pairing_display_only = true;
            g_pairing_passkey = param->ble_security.key_notif.passkey;
            memcpy(g_pairing_addr, param->ble_security.key_notif.bd_addr, sizeof(esp_bd_addr_t));
            break;
        case ESP_GAP_BLE_PASSKEY_REQ_EVT:
            g_pairing_passkey_req = true;
            memcpy(g_pairing_addr, param->ble_security.ble_req.bd_addr, sizeof(esp_bd_addr_t));
            break;
        case ESP_GAP_BLE_AUTH_CMPL_EVT:
            if (!param->ble_security.auth_cmpl.success && g_exchange_state != EXCHANGE_IDLE) {
                exchange_finish(false);
            }
            break;
        default:
            break;
    }
}

static void adv_timer_cb(TimerHandle_t) {
    if (!g_adv_active || !g_initialized) return;
    update_adv_data();
    esp_ble_gap_start_advertising(&adv_params);
    if (g_adv_stop_timer) {
        xTimerStart(g_adv_stop_timer, 0);
    }
}

static void adv_stop_timer_cb(TimerHandle_t) {
    esp_ble_gap_stop_advertising();
}

static void scan_timer_cb(TimerHandle_t) {
    if (!g_scan_active || !g_initialized) return;
    esp_ble_gap_start_scanning((uint32_t)(g_scan_window_ms / 1000));
    if (g_scan_stop_timer) {
        xTimerStart(g_scan_stop_timer, 0);
    }
}

static void scan_stop_timer_cb(TimerHandle_t) {
    esp_ble_gap_stop_scanning();
}

static void update_adv_state(void) {
    bool desired = g_adv_requested || g_exchange_enabled;
    if (!g_initialized) {
        g_adv_active = desired;
        return;
    }
    if (desired && !g_adv_active) {
        g_adv_active = true;
        xTimerStart(g_adv_timer, 0);
        adv_timer_cb(nullptr);
    } else if (!desired && g_adv_active) {
        g_adv_active = false;
        xTimerStop(g_adv_timer, 0);
        xTimerStop(g_adv_stop_timer, 0);
        esp_ble_gap_stop_advertising();
    }
}

static void update_scan_state(void) {
    bool desired = g_scan_requested || g_exchange_enabled;
    if (!g_initialized) {
        g_scan_active = desired;
        return;
    }
    if (desired && !g_scan_active) {
        g_scan_active = true;
        xTimerStart(g_scan_timer, 0);
        scan_timer_cb(nullptr);
    } else if (!desired && g_scan_active) {
        g_scan_active = false;
        xTimerStop(g_scan_timer, 0);
        xTimerStop(g_scan_stop_timer, 0);
        esp_ble_gap_stop_scanning();
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT: {
            g_gatts_if = gatts_if;
            esp_gatt_srvc_id_t service_id = {};
            service_id.is_primary = true;
            service_id.id.inst_id = 0;
            service_id.id.uuid.len = ESP_UUID_LEN_128;
            memcpy(service_id.id.uuid.uuid.uuid128, vcard_service_uuid, 16);
            esp_ble_gatts_create_service(gatts_if, &service_id, 10);
            break;
        }
        case ESP_GATTS_CREATE_EVT: {
            g_service_handle = param->create.service_handle;
            esp_ble_gatts_start_service(g_service_handle);

            esp_bt_uuid_t data_uuid = {};
            data_uuid.len = ESP_UUID_LEN_128;
            memcpy(data_uuid.uuid.uuid128, vcard_data_uuid, 16);
            esp_ble_gatts_add_char(g_service_handle, &data_uuid,
                                   ESP_GATT_PERM_READ_ENCRYPTED,
                                   ESP_GATT_CHAR_PROP_BIT_INDICATE,
                                   NULL, NULL);
            break;
        }
        case ESP_GATTS_ADD_CHAR_EVT: {
            if (param->add_char.status != ESP_GATT_OK) break;
            if (g_char_data_handle == 0) {
                g_char_data_handle = param->add_char.attr_handle;
                esp_bt_uuid_t ccc_uuid = {};
                ccc_uuid.len = ESP_UUID_LEN_16;
                ccc_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
                esp_ble_gatts_add_char_descr(g_service_handle, &ccc_uuid,
                                             ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                             NULL, NULL);
            } else if (g_char_ctrl_handle == 0) {
                g_char_ctrl_handle = param->add_char.attr_handle;
                esp_bt_uuid_t rx_uuid = {};
                rx_uuid.len = ESP_UUID_LEN_128;
                memcpy(rx_uuid.uuid.uuid128, vcard_rx_uuid, 16);
                esp_ble_gatts_add_char(g_service_handle, &rx_uuid,
                                       ESP_GATT_PERM_WRITE_ENCRYPTED,
                                       ESP_GATT_CHAR_PROP_BIT_WRITE,
                                       NULL, NULL);
            } else if (g_char_rx_handle == 0) {
                g_char_rx_handle = param->add_char.attr_handle;
                esp_bt_uuid_t status_uuid = {};
                status_uuid.len = ESP_UUID_LEN_128;
                memcpy(status_uuid.uuid.uuid128, vcard_status_uuid, 16);
                esp_ble_gatts_add_char(g_service_handle, &status_uuid,
                                       ESP_GATT_PERM_READ_ENCRYPTED,
                                       ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_INDICATE,
                                       NULL, NULL);
            } else if (g_char_status_handle == 0) {
                g_char_status_handle = param->add_char.attr_handle;
                esp_bt_uuid_t ccc_uuid = {};
                ccc_uuid.len = ESP_UUID_LEN_16;
                ccc_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
                esp_ble_gatts_add_char_descr(g_service_handle, &ccc_uuid,
                                             ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                             NULL, NULL);
            }
            break;
        }
        case ESP_GATTS_ADD_CHAR_DESCR_EVT: {
            if (param->add_char_descr.status != ESP_GATT_OK) break;
            if (g_char_data_ccc_handle == 0) {
                g_char_data_ccc_handle = param->add_char_descr.attr_handle;
                esp_bt_uuid_t ctrl_uuid = {};
                ctrl_uuid.len = ESP_UUID_LEN_128;
                memcpy(ctrl_uuid.uuid.uuid128, vcard_ctrl_uuid, 16);
                esp_ble_gatts_add_char(g_service_handle, &ctrl_uuid,
                                       ESP_GATT_PERM_WRITE_ENCRYPTED,
                                       ESP_GATT_CHAR_PROP_BIT_WRITE,
                                       NULL, NULL);
            } else if (g_char_status_ccc_handle == 0) {
                g_char_status_ccc_handle = param->add_char_descr.attr_handle;
            }
            break;
        }
        case ESP_GATTS_CONNECT_EVT:
            g_gatts_connected = true;
            g_gatts_conn_id = param->connect.conn_id;
            g_gatts_mtu_payload = BLE_BADGE_TX_PAYLOAD_DEFAULT;
            memcpy(g_gatts_bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            g_data_indicate_enabled = false;
            g_status_indicate_enabled = false;
            g_srv_tx_active = false;
            g_srv_rx_active = false;
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            g_gatts_connected = false;
            g_data_indicate_enabled = false;
            g_status_indicate_enabled = false;
            g_srv_tx_active = false;
            g_srv_rx_active = false;
            if (g_adv_active) {
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GATTS_MTU_EVT:
            if (param->mtu.mtu > 3) {
                g_gatts_mtu_payload = param->mtu.mtu - 3;
            }
            break;
        case ESP_GATTS_WRITE_EVT: {
            esp_gatt_status_t status = ESP_GATT_OK;
            if (param->write.handle == g_char_data_ccc_handle && param->write.len == 2) {
                uint16_t ccc = param->write.value[0] | (param->write.value[1] << 8);
                g_data_indicate_enabled = (ccc == 0x0002);
            } else if (param->write.handle == g_char_status_ccc_handle && param->write.len == 2) {
                uint16_t ccc = param->write.value[0] | (param->write.value[1] << 8);
                g_status_indicate_enabled = (ccc == 0x0002);
            } else if (param->write.handle == g_char_ctrl_handle) {
                if (param->write.len > 0 && param->write.value[0] == 0x01) {
                    if (!g_exchange_enabled) {
                        status = ESP_GATT_WRITE_NOT_PERMIT;
                    } else if (!vcard_store_has_own()) {
                        status = ESP_GATT_WRITE_NOT_PERMIT;
                    } else if (g_data_indicate_enabled) {
                        size_t len = vcard_store_get_own(g_srv_tx_buf, sizeof(g_srv_tx_buf));
                        g_srv_tx_len = len;
                        g_srv_tx_offset = 0;
                        g_srv_tx_active = true;
                        server_send_next_chunk();
                    }
                }
            } else if (param->write.handle == g_char_rx_handle) {
                if (!g_exchange_enabled || !(g_receive_enabled || g_exchange_force_receive)) {
                    status = ESP_GATT_WRITE_NOT_PERMIT;
                } else {
                    server_handle_rx_chunk(param->write.value, param->write.len);
                }
            }
            if (param->write.need_rsp) {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id,
                                            status, NULL);
            }
            break;
        }
        case ESP_GATTS_CONF_EVT:
            if (param->conf.handle == g_char_data_handle) {
                server_send_next_chunk();
            }
            break;
        default:
            break;
    }
}

static void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                esp_ble_gattc_cb_param_t *param) {
    switch (event) {
        case ESP_GATTC_REG_EVT:
            g_gattc_if = gattc_if;
            break;
        case ESP_GATTC_CONNECT_EVT:
            g_exchange_conn_id = param->connect.conn_id;
            memcpy(g_exchange_addr, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            g_exchange_state = EXCHANGE_DISCOVERING;
            exchange_reset_remote();
            exchange_reset_local();
            esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);
            esp_ble_gattc_send_mtu_req(gattc_if, g_exchange_conn_id);
            {
                esp_bt_uuid_t svc_uuid = {};
                svc_uuid.len = ESP_UUID_LEN_128;
                memcpy(svc_uuid.uuid.uuid128, vcard_service_uuid, 16);
                esp_ble_gattc_search_service(gattc_if, g_exchange_conn_id, &svc_uuid);
            }
            break;
        case ESP_GATTC_CFG_MTU_EVT:
            if (param->cfg_mtu.mtu > 3) {
                g_exchange_mtu_payload = param->cfg_mtu.mtu - 3;
            }
            break;
        case ESP_GATTC_SEARCH_RES_EVT: {
            esp_gatt_id_t *srvc_id = &param->search_res.srvc_id;
            if (srvc_id->uuid.len == ESP_UUID_LEN_128 &&
                memcmp(srvc_id->uuid.uuid.uuid128, vcard_service_uuid, 16) == 0) {
                g_remote_service_start = param->search_res.start_handle;
                g_remote_service_end = param->search_res.end_handle;
            }
            break;
        }
        case ESP_GATTC_SEARCH_CMPL_EVT: {
            if (g_remote_service_start == 0 || g_remote_service_end == 0) {
                exchange_finish(false);
                break;
            }
            esp_bt_uuid_t uuid = {};
            uint16_t count = 1;
            esp_gattc_char_elem_t char_elem = {};

            uuid.len = ESP_UUID_LEN_128;
            memcpy(uuid.uuid.uuid128, vcard_data_uuid, 16);
            if (esp_ble_gattc_get_char_by_uuid(gattc_if, g_exchange_conn_id,
                                               g_remote_service_start, g_remote_service_end,
                                               uuid, &char_elem, &count) == ESP_GATT_OK && count > 0) {
                g_remote_data_handle = char_elem.char_handle;
            }
            memcpy(uuid.uuid.uuid128, vcard_ctrl_uuid, 16);
            count = 1;
            if (esp_ble_gattc_get_char_by_uuid(gattc_if, g_exchange_conn_id,
                                               g_remote_service_start, g_remote_service_end,
                                               uuid, &char_elem, &count) == ESP_GATT_OK && count > 0) {
                g_remote_ctrl_handle = char_elem.char_handle;
            }
            memcpy(uuid.uuid.uuid128, vcard_rx_uuid, 16);
            count = 1;
            if (esp_ble_gattc_get_char_by_uuid(gattc_if, g_exchange_conn_id,
                                               g_remote_service_start, g_remote_service_end,
                                               uuid, &char_elem, &count) == ESP_GATT_OK && count > 0) {
                g_remote_rx_handle = char_elem.char_handle;
            }
            memcpy(uuid.uuid.uuid128, vcard_status_uuid, 16);
            count = 1;
            if (esp_ble_gattc_get_char_by_uuid(gattc_if, g_exchange_conn_id,
                                               g_remote_service_start, g_remote_service_end,
                                               uuid, &char_elem, &count) == ESP_GATT_OK && count > 0) {
                g_remote_status_handle = char_elem.char_handle;
            }

            if (g_remote_data_handle == 0 || g_remote_ctrl_handle == 0 ||
                g_remote_rx_handle == 0 || g_remote_status_handle == 0) {
                exchange_finish(false);
                break;
            }

            esp_bt_uuid_t ccc_uuid = {};
            ccc_uuid.len = ESP_UUID_LEN_16;
            ccc_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
            esp_gattc_descr_elem_t descr_elem = {};

            count = 1;
            if (esp_ble_gattc_get_descr_by_char_handle(gattc_if, g_exchange_conn_id,
                                                       g_remote_data_handle, ccc_uuid,
                                                       &descr_elem, &count) == ESP_GATT_OK && count > 0) {
                g_remote_data_ccc_handle = descr_elem.handle;
            }
            count = 1;
            if (esp_ble_gattc_get_descr_by_char_handle(gattc_if, g_exchange_conn_id,
                                                       g_remote_status_handle, ccc_uuid,
                                                       &descr_elem, &count) == ESP_GATT_OK && count > 0) {
                g_remote_status_ccc_handle = descr_elem.handle;
            }

            esp_ble_gattc_register_for_notify(gattc_if, g_exchange_addr, g_remote_data_handle);
            esp_ble_gattc_register_for_notify(gattc_if, g_exchange_addr, g_remote_status_handle);
            g_exchange_state = EXCHANGE_ENABLING_NOTIFY;
            break;
        }
        case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
            if (param->reg_for_notify.status != ESP_GATT_OK) break;
            uint16_t ccc_handle = 0;
            if (param->reg_for_notify.handle == g_remote_data_handle) {
                ccc_handle = g_remote_data_ccc_handle;
            } else if (param->reg_for_notify.handle == g_remote_status_handle) {
                ccc_handle = g_remote_status_ccc_handle;
            }
            if (ccc_handle != 0) {
                uint16_t ccc = 0x0002; // indications
                esp_ble_gattc_write_char_descr(gattc_if, g_exchange_conn_id, ccc_handle,
                                               sizeof(ccc), (uint8_t *)&ccc,
                                               ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_MITM);
            }
            break;
        }
        case ESP_GATTC_WRITE_DESCR_EVT:
            if (param->write.status == ESP_GATT_OK) {
                if (param->write.handle == g_remote_data_ccc_handle) {
                    g_remote_data_ccc_set = true;
                } else if (param->write.handle == g_remote_status_ccc_handle) {
                    g_remote_status_ccc_set = true;
                }
            }
            if (g_remote_data_ccc_set && g_remote_status_ccc_set && g_exchange_state == EXCHANGE_ENABLING_NOTIFY) {
                uint8_t cmd = 0x01;
                g_exchange_state = EXCHANGE_REQUESTING_REMOTE;
                esp_ble_gattc_write_char(gattc_if, g_exchange_conn_id, g_remote_ctrl_handle,
                                         sizeof(cmd), &cmd, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_MITM);
            }
            break;
        case ESP_GATTC_WRITE_CHAR_EVT:
            if (param->write.status != ESP_GATT_OK) {
                exchange_finish(false);
                break;
            }
            if (param->write.handle == g_remote_ctrl_handle) {
                g_exchange_state = EXCHANGE_RECEIVING_REMOTE;
            } else if (param->write.handle == g_remote_rx_handle) {
                client_send_next_chunk();
            }
            break;
        case ESP_GATTC_NOTIFY_EVT:
            if (param->notify.handle == g_remote_data_handle) {
                client_handle_data_chunk(param->notify.value, param->notify.value_len);
            } else if (param->notify.handle == g_remote_status_handle) {
                bool ok = param->notify.value_len > 0 && param->notify.value[0] == 0x01;
                exchange_finish(ok);
            }
            break;
        case ESP_GATTC_DISCONNECT_EVT:
            if (g_exchange_state != EXCHANGE_IDLE) {
                exchange_finish(false);
            }
            break;
        default:
            break;
    }
}

// Wrapper for gap_event_handler to match ble_core callback signature
static void gap_event_listener(int event, void *param) {
    gap_event_handler((esp_gap_ble_cb_event_t)event, (esp_ble_gap_cb_param_t *)param);
}

bool ble_badge_init(void) {
    if (g_initialized) return true;

    // Initialize shared BLE core (handles BT stack, GAP, security)
    if (!ble_core_init()) {
        LOG_E(BLE_BADGE_TAG, "BLE core init failed");
        return false;
    }

    // Register GAP listener for scan results and pairing events
    ble_core_register_gap_listener(gap_event_listener, BLE_APP_BADGE);

    // Register GATTS callback for vCard service
    esp_err_t ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret != ESP_OK) {
        LOG_E(BLE_BADGE_TAG, "GATTS callback register failed: %s", esp_err_to_name(ret));
        return false;
    }

    // Register GATTC callback for exchange client
    ret = esp_ble_gattc_register_callback(gattc_event_handler);
    if (ret != ESP_OK) {
        LOG_E(BLE_BADGE_TAG, "GATTC callback register failed: %s", esp_err_to_name(ret));
        return false;
    }

    // NOTE: Security is now configured by ble_core

    ret = esp_ble_gatts_app_register(BLE_BADGE_APP_ID);
    if (ret != ESP_OK) {
        LOG_E(BLE_BADGE_TAG, "GATTS app register failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = esp_ble_gattc_app_register(BLE_BADGE_APP_ID);
    if (ret != ESP_OK) {
        LOG_E(BLE_BADGE_TAG, "GATTC app register failed: %s", esp_err_to_name(ret));
        return false;
    }

    // Get own BLE address to filter self from scan results
    const uint8_t *own_addr = esp_bt_dev_get_address();
    if (own_addr) {
        memcpy(g_own_bda, own_addr, sizeof(esp_bd_addr_t));
        LOG_I(BLE_BADGE_TAG, "Own BDA: %02X:%02X:%02X:%02X:%02X:%02X",
              g_own_bda[0], g_own_bda[1], g_own_bda[2],
              g_own_bda[3], g_own_bda[4], g_own_bda[5]);
    }

    esp_ble_gap_set_scan_params(&scan_params);
    g_initialized = true;
    update_adv_data();

    g_adv_timer = xTimerCreate("ble_adv", pdMS_TO_TICKS(g_adv_interval_ms), pdTRUE, NULL, adv_timer_cb);
    g_adv_stop_timer = xTimerCreate("ble_adv_stop", pdMS_TO_TICKS(2000), pdFALSE, NULL, adv_stop_timer_cb);
    g_scan_timer = xTimerCreate("ble_scan", pdMS_TO_TICKS(g_scan_window_ms + g_scan_pause_ms), pdTRUE, NULL, scan_timer_cb);
    g_scan_stop_timer = xTimerCreate("ble_scan_stop", pdMS_TO_TICKS(g_scan_window_ms), pdFALSE, NULL, scan_stop_timer_cb);
    g_exchange_timer = xTimerCreate("ble_exchange", pdMS_TO_TICKS(EXCHANGE_TIMEOUT_MS), pdFALSE, NULL, exchange_timeout_cb);

    vcard_store_init();
    update_adv_state();
    update_scan_state();
    return true;
}

void ble_badge_deinit(void) {
    if (!g_initialized) return;
    g_adv_requested = false;
    g_scan_requested = false;
    g_adv_active = false;
    g_scan_active = false;
    g_receive_enabled = false;
    g_exchange_enabled = false;
    g_exchange_state = EXCHANGE_IDLE;

    if (g_adv_timer) xTimerStop(g_adv_timer, 0);
    if (g_adv_stop_timer) xTimerStop(g_adv_stop_timer, 0);
    if (g_scan_timer) xTimerStop(g_scan_timer, 0);
    if (g_scan_stop_timer) xTimerStop(g_scan_stop_timer, 0);
    if (g_exchange_timer) xTimerStop(g_exchange_timer, 0);

    esp_ble_gap_stop_advertising();
    esp_ble_gap_stop_scanning();

    // Unregister GAP listener
    ble_core_register_gap_listener(NULL, BLE_APP_BADGE);

    // Unregister GATT apps (keep BT stack running for other services)
    if (g_gatts_if != ESP_GATT_IF_NONE) {
        esp_ble_gatts_app_unregister(g_gatts_if);
        g_gatts_if = ESP_GATT_IF_NONE;
    }
    if (g_gattc_if != ESP_GATT_IF_NONE) {
        esp_ble_gattc_app_unregister(g_gattc_if);
        g_gattc_if = ESP_GATT_IF_NONE;
    }

    // NOTE: Do NOT deinit BT stack here - other services may still use it
    // ble_core_deinit() should only be called when all services are done

    g_initialized = false;
}

void ble_badge_set_adv_enabled(bool enabled) {
    g_adv_requested = enabled;
    update_adv_data();
    update_adv_state();
}

bool ble_badge_is_adv_enabled(void) {
    return g_adv_requested;
}

void ble_badge_set_scan_enabled(bool enabled) {
    g_scan_requested = enabled;
    update_scan_state();
}

bool ble_badge_is_scan_enabled(void) {
    return g_scan_requested;
}

bool ble_badge_is_adv_active(void) {
    return g_adv_active;
}

bool ble_badge_is_scan_active(void) {
    return g_scan_active;
}

void ble_badge_set_receive_enabled(bool enabled) {
    g_receive_enabled = enabled;
}

bool ble_badge_is_receive_enabled(void) {
    return g_receive_enabled;
}

void ble_badge_set_exchange_enabled(bool enabled) {
    if (g_exchange_enabled == enabled) return;
    g_exchange_enabled = enabled;
    update_adv_data();
    update_adv_state();
    update_scan_state();
    if (!enabled && g_exchange_state != EXCHANGE_IDLE) {
        exchange_finish(false);
    }
}

bool ble_badge_is_exchange_enabled(void) {
    return g_exchange_enabled;
}

void ble_badge_set_scan_interval(uint32_t scan_ms, uint32_t pause_ms) {
    g_scan_window_ms = scan_ms;
    g_scan_pause_ms = pause_ms;
    if (g_scan_timer) {
        xTimerChangePeriod(g_scan_timer, pdMS_TO_TICKS(g_scan_window_ms + g_scan_pause_ms), 0);
    }
    if (g_scan_stop_timer) {
        xTimerChangePeriod(g_scan_stop_timer, pdMS_TO_TICKS(g_scan_window_ms), 0);
    }
}

void ble_badge_set_adv_interval(uint32_t interval_ms) {
    g_adv_interval_ms = interval_ms;
    if (g_adv_timer) {
        xTimerChangePeriod(g_adv_timer, pdMS_TO_TICKS(g_adv_interval_ms), 0);
    }
}

void ble_badge_set_rssi_threshold(int8_t rssi) {
    g_rssi_threshold = rssi;
}

uint32_t ble_badge_get_adv_interval(void) {
    return g_adv_interval_ms;
}

uint32_t ble_badge_get_scan_interval(void) {
    return g_scan_pause_ms;
}

uint16_t ble_badge_get_peers(ble_badge_peer_t *out, uint16_t max_peers) {
    if (!out || max_peers == 0) return 0;
    uint16_t count = 0;
    for (int i = 0; i < MAX_PEERS && count < max_peers; i++) {
        if (g_peers[i].name[0] == '\0') continue;
        out[count++] = g_peers[i];
    }
    return count;
}

bool ble_badge_exchange_with(const uint8_t addr[6]) {
    if (!addr || !g_initialized || g_gattc_if == ESP_GATT_IF_NONE) return false;
    if (g_exchange_state != EXCHANGE_IDLE) return false;
    if (!g_exchange_enabled) return false;
    if (vcard_store_get_own(g_local_tx_buf, sizeof(g_local_tx_buf)) == 0) {
        return false;
    }

    g_local_tx_len = strnlen(g_local_tx_buf, VCARD_MAX_LEN);
    g_local_tx_offset = 0;

    g_exchange_restore_adv = g_adv_requested;
    g_exchange_restore_scan = g_scan_requested;

    g_exchange_force_receive = true;
    g_exchange_result_pending = false;
    g_exchange_state = EXCHANGE_CONNECTING;
    memcpy(g_exchange_addr, addr, sizeof(esp_bd_addr_t));

    uint8_t addr_copy[6];
    memcpy(addr_copy, addr, sizeof(addr_copy));
    esp_err_t ret = esp_ble_gattc_open(g_gattc_if, addr_copy, BLE_ADDR_TYPE_PUBLIC, true);
    if (ret != ESP_OK) {
        exchange_finish(false);
        return false;
    }

    if (g_exchange_timer) {
        xTimerStop(g_exchange_timer, 0);
        xTimerStart(g_exchange_timer, 0);
    }
    return true;
}

bool ble_badge_poll_nearby(ble_badge_peer_t *out) {
    if (!out) return false;
    if (!g_pending_nearby_set) return false;
    *out = g_pending_nearby;
    g_pending_nearby_set = false;
    return true;
}

bool ble_badge_exchange_cancel(void) {
    if (g_exchange_state == EXCHANGE_IDLE) return false;
    exchange_finish(false);
    return true;
}

bool ble_badge_poll_exchange_result(bool *success) {
    if (!g_exchange_result_pending) return false;
    if (success) {
        *success = g_exchange_result_success;
    }
    g_exchange_result_pending = false;
    return true;
}

bool ble_badge_exchange_in_progress(void) {
    return g_exchange_state != EXCHANGE_IDLE;
}

bool ble_badge_poll_pairing_event(ble_badge_pair_event_t *type, uint32_t *passkey) {
    if (!type) return false;
    if (g_pairing_passkey_req) {
        g_pairing_passkey_req = false;
        *type = BLE_BADGE_PAIR_PASSKEY_INPUT;
    } else if (g_pairing_pending) {
        g_pairing_pending = false;
        *type = BLE_BADGE_PAIR_NUMERIC;
    } else if (g_pairing_display_only) {
        g_pairing_display_only = false;
        *type = BLE_BADGE_PAIR_PASSKEY_DISPLAY;
    } else {
        return false;
    }
    if (passkey) *passkey = g_pairing_passkey;
    return true;
}

void ble_badge_confirm_pairing(bool accept) {
    esp_ble_confirm_reply(g_pairing_addr, accept);
}

void ble_badge_reply_passkey(bool accept, uint32_t passkey) {
    esp_ble_passkey_reply(g_pairing_addr, accept, passkey);
}

// ============================================================================
// GPG Key Exchange over BLE
// ============================================================================
#if FEATURE_GPG

// Serialize GPG key for BLE transfer
static size_t gpg_serialize_key(uint8_t *buf, size_t buf_size) {
    uint8_t pubkey[64];
    size_t pubkey_len;
    uint8_t curve;
    char user_id[GPG_USER_ID_MAX];
    uint8_t fingerprint[GPG_FINGERPRINT_LEN];

    if (!gpg_export_for_broadcast(pubkey, &pubkey_len, &curve, user_id, fingerprint)) {
        return 0;
    }

    size_t user_id_len = strnlen(user_id, GPG_USER_ID_MAX - 1);
    size_t total = 1 + 1 + pubkey_len + GPG_FINGERPRINT_LEN + 1 + user_id_len;

    if (total > buf_size) {
        return 0;
    }

    size_t idx = 0;
    buf[idx++] = curve;
    buf[idx++] = (uint8_t)pubkey_len;
    memcpy(&buf[idx], pubkey, pubkey_len);
    idx += pubkey_len;
    memcpy(&buf[idx], fingerprint, GPG_FINGERPRINT_LEN);
    idx += GPG_FINGERPRINT_LEN;
    buf[idx++] = (uint8_t)user_id_len;
    memcpy(&buf[idx], user_id, user_id_len);
    idx += user_id_len;

    return idx;
}

// Deserialize and store received GPG key
static bool gpg_deserialize_and_store(const uint8_t *buf, size_t len) {
    if (len < 1 + 1 + 32 + GPG_FINGERPRINT_LEN + 1) {
        LOG_E(BLE_BADGE_TAG, "GPG payload too short: %zu", len);
        return false;
    }

    size_t idx = 0;
    uint8_t curve = buf[idx++];
    uint8_t pubkey_len = buf[idx++];

    if (pubkey_len != 32 && pubkey_len != 64) {
        LOG_E(BLE_BADGE_TAG, "Invalid pubkey length: %u", pubkey_len);
        return false;
    }

    if (idx + pubkey_len + GPG_FINGERPRINT_LEN + 1 > len) {
        LOG_E(BLE_BADGE_TAG, "GPG payload truncated");
        return false;
    }

    const uint8_t *pubkey = &buf[idx];
    idx += pubkey_len;
    const uint8_t *fingerprint = &buf[idx];
    idx += GPG_FINGERPRINT_LEN;
    uint8_t user_id_len = buf[idx++];

    if (idx + user_id_len > len) {
        LOG_E(BLE_BADGE_TAG, "User ID truncated");
        return false;
    }

    char user_id[GPG_USER_ID_MAX];
    memset(user_id, 0, sizeof(user_id));
    size_t copy_len = (user_id_len < GPG_USER_ID_MAX - 1) ? user_id_len : (GPG_USER_ID_MAX - 1);
    memcpy(user_id, &buf[idx], copy_len);

    // Store the received key
    if (!gpg_receive_pubkey(pubkey, pubkey_len, curve, user_id, fingerprint)) {
        LOG_E(BLE_BADGE_TAG, "Failed to store received GPG key");
        return false;
    }

    LOG_I(BLE_BADGE_TAG, "Received GPG key: %s", user_id);
    return true;
}

// Handle incoming GPG data chunk (server side)
// Note: Not static because it's called from server_handle_rx_chunk
void server_handle_gpg_rx_chunk(const uint8_t *data, size_t len) {
    if (!data || len == 0) return;
    uint8_t opcode = data[0];
    size_t idx = 1;

    if (opcode == GPG_OP_WRITE_START) {
        if (len < 3) return;
        g_gpg_rx_expected = (size_t)data[1] | ((size_t)data[2] << 8);
        g_gpg_rx_len = 0;
        idx = 3;
        if (g_gpg_rx_expected > GPG_PAYLOAD_MAX_LEN) {
            g_gpg_rx_expected = 0;
            server_send_status(0x00);
            return;
        }
    }

    if (idx < len && g_gpg_rx_len < GPG_PAYLOAD_MAX_LEN) {
        size_t copy_len = len - idx;
        if (copy_len > (GPG_PAYLOAD_MAX_LEN - g_gpg_rx_len)) {
            copy_len = GPG_PAYLOAD_MAX_LEN - g_gpg_rx_len;
        }
        memcpy(&g_gpg_rx_buf[g_gpg_rx_len], &data[idx], copy_len);
        g_gpg_rx_len += copy_len;
    }

    bool done = (g_gpg_rx_expected > 0 && g_gpg_rx_len >= g_gpg_rx_expected) ||
                (opcode == GPG_OP_WRITE_END);
    if (done) {
        bool ok = gpg_deserialize_and_store(g_gpg_rx_buf, g_gpg_rx_len);
        server_send_status(ok ? 0x01 : 0x00);
        g_gpg_rx_len = 0;
        g_gpg_rx_expected = 0;
    }
}

bool ble_badge_gpg_exchange_with(const uint8_t addr[6]) {
    if (!addr || !g_initialized || g_gattc_if == ESP_GATT_IF_NONE) return false;
    if (g_exchange_state != EXCHANGE_IDLE) return false;
    if (!g_exchange_enabled) return false;

    // Check if we have a GPG key to exchange
    if (!gpg_is_initialized()) {
        LOG_E(BLE_BADGE_TAG, "No GPG key configured");
        return false;
    }

    // Serialize our GPG key
    g_gpg_tx_len = gpg_serialize_key(g_gpg_tx_buf, sizeof(g_gpg_tx_buf));
    if (g_gpg_tx_len == 0) {
        LOG_E(BLE_BADGE_TAG, "Failed to serialize GPG key");
        return false;
    }
    g_gpg_tx_offset = 0;

    g_gpg_exchange_mode = true;
    g_gpg_exchange_result_pending = false;
    g_exchange_restore_adv = g_adv_requested;
    g_exchange_restore_scan = g_scan_requested;
    g_exchange_force_receive = true;
    g_exchange_result_pending = false;
    g_exchange_state = EXCHANGE_CONNECTING;
    memcpy(g_exchange_addr, addr, sizeof(esp_bd_addr_t));

    uint8_t addr_copy[6];
    memcpy(addr_copy, addr, sizeof(addr_copy));
    esp_err_t ret = esp_ble_gattc_open(g_gattc_if, addr_copy, BLE_ADDR_TYPE_PUBLIC, true);
    if (ret != ESP_OK) {
        g_gpg_exchange_mode = false;
        exchange_finish(false);
        return false;
    }

    if (g_exchange_timer) {
        xTimerStop(g_exchange_timer, 0);
        xTimerStart(g_exchange_timer, 0);
    }

    LOG_I(BLE_BADGE_TAG, "Starting GPG key exchange");
    return true;
}

bool ble_badge_poll_gpg_exchange_result(bool *success) {
    if (!g_gpg_exchange_result_pending) return false;
    if (success) {
        *success = g_gpg_exchange_result_success;
    }
    g_gpg_exchange_result_pending = false;
    return true;
}

bool ble_badge_gpg_exchange_in_progress(void) {
    return g_gpg_exchange_mode && g_exchange_state != EXCHANGE_IDLE;
}

#endif // FEATURE_GPG

#endif // FEATURE_BLE_BADGE
