// BLE UART Service Implementation (Nordic UART Service compatible)
// Uses shared BLE Core for stack management

#include "ble_uart.h"

#if FEATURE_BLE_UART

#include "ble_core.h"
#include "cdc_log.h"
#include "cdc_time.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/ringbuf.h"
#include "freertos/timers.h"

#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

#include <string.h>
#include <stdarg.h>

// ============================================================================
// Constants
// ============================================================================

#define DEVICE_NAME             "CDC Badge"
#define BLE_TAG                 "BLE_UART"

// GATT handles
#define NUS_NUM_HANDLE          6       // Service + 2 chars * 2 + 1 CCCD

// GATT App ID (from ble_core.h - each service needs unique ID)
#define NUS_APP_ID              BLE_APP_UART

// Buffer sizes
#define RX_BUFFER_SIZE          2048    // Incoming data from client (increased for bulk transfers)
#define TX_CHUNK_SIZE           20      // BLE notification MTU (conservative)

// ============================================================================
// Nordic UART Service UUIDs (128-bit)
// Base: xxxxxxxx-B5A3-F393-E0A9-E50E24DCCA9E
// ============================================================================

// Service UUID: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
static const uint8_t nus_service_uuid[16] = {
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E
};

// RX Characteristic UUID: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E
// Client writes to this (data FROM phone TO badge)
static const uint8_t nus_rx_uuid[16] = {
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E
};

// TX Characteristic UUID: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E
// Badge notifies this (data FROM badge TO phone)
static const uint8_t nus_tx_uuid[16] = {
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x6E
};

// ============================================================================
// State
// ============================================================================

static bool g_initialized = false;
static bool g_connected = false;
static uint16_t g_conn_id = 0;
static uint16_t g_mtu = 23;  // Default BLE MTU

// GATT handles
static esp_gatt_if_t g_gatts_if = ESP_GATT_IF_NONE;
static uint16_t g_service_handle = 0;
static uint16_t g_rx_handle = 0;        // Client writes here
static uint16_t g_tx_handle = 0;        // Badge notifies here
static uint16_t g_tx_cccd_handle = 0;   // TX notifications enable/disable

// Notifications enabled
static bool g_tx_notify_enabled = false;

// RX buffer (ring buffer for incoming data)
static RingbufHandle_t g_rx_ringbuf = NULL;

// RX callback
static ble_uart_rx_callback_t g_rx_callback = NULL;

// Power management
static ble_power_mode_t g_power_mode = BLE_POWER_OFF;
static uint32_t g_last_activity_ms = 0;

// Reconnect timer (to restart advertising after disconnect)
static TimerHandle_t g_reconnect_timer = NULL;

// Security/Pairing state
static bool g_bonded = false;
static uint32_t g_passkey = 0;

// Callback for displaying passkey on screen
static ble_passkey_display_cb_t g_passkey_display_cb = NULL;

// Callback for auth complete (to dismiss passkey toast)
typedef void (*ble_auth_complete_cb_t)(bool success);
static ble_auth_complete_cb_t g_auth_complete_cb = NULL;

// TX flow control - semaphore to wait for confirmation
static SemaphoreHandle_t g_tx_sem = NULL;
static volatile bool g_tx_congested = false;

// Advertising parameters (increased intervals for WiFi coexistence)
static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0xA0,     // 100ms (was 40ms)
    .adv_int_max        = 0x140,    // 200ms (was 80ms)
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .peer_addr          = {0},
    .peer_addr_type     = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static esp_ble_adv_params_t adv_params_idle = {
    .adv_int_min        = 0x140,    // 200ms (was 100ms)
    .adv_int_max        = 0x280,    // 400ms (was 200ms)
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .peer_addr          = {0},
    .peer_addr_type     = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// Advertising data
// NOTE: Advertising data is now managed by ble_core

// ============================================================================
// Forward Declarations
// ============================================================================

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                 esp_ble_gatts_cb_param_t *param);

// ============================================================================
// Reconnect Timer Callback
// ============================================================================

static void reconnect_timer_callback(TimerHandle_t timer) {
    (void)timer;
    if (!g_connected && g_initialized) {
        LOG_D(BLE_TAG, "Restarting advertising after disconnect");
        ble_core_start_advertising();
    }
}

// NOTE: GAP events are now handled by ble_core
// Security callbacks are routed through ble_core_set_passkey_callback() etc.

// ============================================================================
// GATTS Event Handler
// ============================================================================

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                 esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT:
            if (param->reg.status == ESP_GATT_OK && param->reg.app_id == NUS_APP_ID) {
                g_gatts_if = gatts_if;
                LOG_I(BLE_TAG, "GATT server registered (app_id=%d)", NUS_APP_ID);

                // Set device name via ble_core
                ble_core_set_device_name(DEVICE_NAME);

                // Create NUS service
                esp_gatt_srvc_id_t service_id;
                memset(&service_id, 0, sizeof(service_id));
                service_id.is_primary = true;
                service_id.id.inst_id = 0;
                service_id.id.uuid.len = ESP_UUID_LEN_128;
                memcpy(service_id.id.uuid.uuid.uuid128, nus_service_uuid, 16);
                esp_ble_gatts_create_service(gatts_if, &service_id, NUS_NUM_HANDLE);
            }
            break;

        case ESP_GATTS_CREATE_EVT:
            if (param->create.status == ESP_GATT_OK) {
                g_service_handle = param->create.service_handle;
                LOG_D(BLE_TAG, "Service created, handle=%d", g_service_handle);

                // Start service
                esp_ble_gatts_start_service(g_service_handle);

                // Add RX characteristic (write, write without response)
                // ENCRYPTED permission requires pairing before writing
                esp_bt_uuid_t rx_uuid;
                rx_uuid.len = ESP_UUID_LEN_128;
                memcpy(rx_uuid.uuid.uuid128, nus_rx_uuid, 16);
                esp_ble_gatts_add_char(g_service_handle, &rx_uuid,
                                        ESP_GATT_PERM_WRITE_ENCRYPTED,
                                        ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_WRITE_NR,
                                        NULL, NULL);
            }
            break;

        case ESP_GATTS_ADD_CHAR_EVT:
            if (param->add_char.status == ESP_GATT_OK) {
                if (g_rx_handle == 0) {
                    g_rx_handle = param->add_char.attr_handle;
                    LOG_D(BLE_TAG, "RX char added, handle=%d", g_rx_handle);

                    // Add TX characteristic (notify)
                    // ENCRYPTED permission requires pairing before receiving notifications
                    esp_bt_uuid_t tx_uuid;
                    tx_uuid.len = ESP_UUID_LEN_128;
                    memcpy(tx_uuid.uuid.uuid128, nus_tx_uuid, 16);
                    esp_ble_gatts_add_char(g_service_handle, &tx_uuid,
                                            ESP_GATT_PERM_READ_ENCRYPTED,
                                            ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                                            NULL, NULL);
                }
                else if (g_tx_handle == 0) {
                    g_tx_handle = param->add_char.attr_handle;
                    LOG_D(BLE_TAG, "TX char added, handle=%d", g_tx_handle);

                    // Add CCCD descriptor for TX notifications
                    // ENCRYPTED permissions require pairing to enable notifications
                    esp_bt_uuid_t cccd_uuid;
                    cccd_uuid.len = ESP_UUID_LEN_16;
                    cccd_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
                    esp_ble_gatts_add_char_descr(g_service_handle, &cccd_uuid,
                                                  ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED,
                                                  NULL, NULL);
                }
            }
            break;

        case ESP_GATTS_ADD_CHAR_DESCR_EVT:
            if (param->add_char_descr.status == ESP_GATT_OK) {
                g_tx_cccd_handle = param->add_char_descr.attr_handle;
                LOG_I(BLE_TAG, "NUS service ready (CCCD=%d)", g_tx_cccd_handle);
            }
            break;

        case ESP_GATTS_START_EVT:
            LOG_D(BLE_TAG, "Service started");
            break;

        case ESP_GATTS_CONNECT_EVT:
            LOG_I(BLE_TAG, "Client connected, conn_id=%d", param->connect.conn_id);
            g_connected = true;
            g_conn_id = param->connect.conn_id;
            g_bonded = false;  // Reset bonded state, will be set in AUTH_CMPL
            g_last_activity_ms = millis();
            // Initiate security - request encryption/pairing from device side
            esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);
            break;

        case ESP_GATTS_DISCONNECT_EVT:
            LOG_I(BLE_TAG, "Client disconnected, reason=0x%02x", param->disconnect.reason);
            g_connected = false;
            g_tx_notify_enabled = false;
            g_conn_id = 0;
            // Restart advertising after short delay via timer (avoids blocking BLE callback)
            if (g_reconnect_timer) {
                xTimerStart(g_reconnect_timer, 0);
            }
            break;

        case ESP_GATTS_MTU_EVT:
            g_mtu = param->mtu.mtu - 3;  // ATT MTU - 3 bytes overhead
            LOG_D(BLE_TAG, "MTU=%d (usable=%d)", param->mtu.mtu, g_mtu);
            break;

        case ESP_GATTS_WRITE_EVT:
            g_last_activity_ms = millis();

            if (param->write.handle == g_rx_handle) {
                // Data received from client
                LOG_D(BLE_TAG, "RX %d bytes", param->write.len);

                // Store in ring buffer
                if (g_rx_ringbuf && param->write.len > 0) {
                    xRingbufferSend(g_rx_ringbuf, param->write.value, param->write.len, 0);
                }

                // Call callback if set
                if (g_rx_callback && param->write.len > 0) {
                    g_rx_callback(param->write.value, param->write.len);
                }
            }
            else if (param->write.handle == g_tx_cccd_handle) {
                // CCCD write - enable/disable notifications
                if (param->write.len == 2) {
                    uint16_t cccd_value = param->write.value[0] | (param->write.value[1] << 8);
                    g_tx_notify_enabled = (cccd_value == 0x0001);
                    LOG_I(BLE_TAG, "TX notifications %s", g_tx_notify_enabled ? "ENABLED" : "disabled");
                }
            }

            // Send response if needed
            if (param->write.need_rsp) {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
                                            param->write.trans_id, ESP_GATT_OK, NULL);
            }
            break;

        case ESP_GATTS_CONF_EVT:
            // Notification confirmed - release TX semaphore
            if (g_tx_sem) {
                xSemaphoreGive(g_tx_sem);
            }
            g_tx_congested = false;
            break;

        case ESP_GATTS_CONGEST_EVT:
            // Congestion control
            g_tx_congested = param->congest.congested;
            LOG_D(BLE_TAG, "Congestion: %s", g_tx_congested ? "ON" : "OFF");
            if (!g_tx_congested && g_tx_sem) {
                xSemaphoreGive(g_tx_sem);
            }
            break;

        default:
            break;
    }
}

// ============================================================================
// Public API
// ============================================================================

bool ble_uart_init(void) {
    if (g_initialized) return true;

    LOG_I(BLE_TAG, "Initializing BLE UART...");

    // Create RX ring buffer
    g_rx_ringbuf = xRingbufferCreate(RX_BUFFER_SIZE, RINGBUF_TYPE_BYTEBUF);
    if (!g_rx_ringbuf) {
        LOG_E(BLE_TAG, "Failed to create RX buffer");
        return false;
    }

    // Create reconnect timer (100ms delay to restart advertising after disconnect)
    g_reconnect_timer = xTimerCreate("ble_reconn", pdMS_TO_TICKS(100), pdFALSE, NULL, reconnect_timer_callback);
    if (!g_reconnect_timer) {
        LOG_E(BLE_TAG, "Failed to create reconnect timer");
        return false;
    }

    // Create TX flow control semaphore (binary semaphore for send confirmation)
    g_tx_sem = xSemaphoreCreateBinary();
    if (!g_tx_sem) {
        LOG_E(BLE_TAG, "Failed to create TX semaphore");
        return false;
    }
    xSemaphoreGive(g_tx_sem);  // Start with semaphore available

    // Initialize shared BLE core (handles BT stack, GAP, security)
    if (!ble_core_init()) {
        LOG_E(BLE_TAG, "BLE core init failed");
        return false;
    }

    // Register passkey callback to route through ble_core
    ble_core_set_passkey_callback([](uint32_t passkey) {
        g_passkey = passkey;
        if (g_passkey_display_cb) {
            g_passkey_display_cb(passkey);
        }
    });

    ble_core_set_auth_callback([](bool success) {
        g_bonded = success;
        if (g_auth_complete_cb) {
            g_auth_complete_cb(success);
        }
    });

    // Register GATTS callback for NUS service events
    esp_err_t ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret != ESP_OK) {
        LOG_E(BLE_TAG, "GATTS callback register failed: %s", esp_err_to_name(ret));
        return false;
    }

    // Register GATT app (this triggers service creation)
    ret = esp_ble_gatts_app_register(NUS_APP_ID);
    if (ret != ESP_OK) {
        LOG_E(BLE_TAG, "GATTS app register failed: %s", esp_err_to_name(ret));
        return false;
    }

    g_initialized = true;
    g_power_mode = BLE_POWER_ACTIVE;

    // Register with console for parallel log output
    console_register_ble_uart(ble_uart_send, ble_uart_tx_ready, ble_uart_getchar);

    LOG_I(BLE_TAG, "BLE UART initialized");
    return true;
}

bool ble_uart_is_initialized(void) {
    return g_initialized;
}

bool ble_uart_is_connected(void) {
    return g_connected;
}

bool ble_uart_tx_ready(void) {
    return g_connected && g_tx_notify_enabled;
}

// Prevent recursive logging during BLE TX
static volatile bool g_tx_in_progress = false;

size_t ble_uart_send(const uint8_t *data, size_t len) {
    if (!ble_uart_tx_ready() || !data || len == 0) return 0;
    if (g_tx_in_progress) return 0;  // Prevent recursion from LOG calls

    g_tx_in_progress = true;

    size_t sent = 0;
    size_t chunk_size = (g_mtu > 3) ? (g_mtu - 3) : TX_CHUNK_SIZE;
    if (chunk_size > 244) chunk_size = 244;  // Max ATT payload

    while (sent < len && g_connected) {
        // Wait for previous transmission to complete (with timeout)
        if (g_tx_sem) {
            if (xSemaphoreTake(g_tx_sem, pdMS_TO_TICKS(100)) != pdTRUE) {
                // Timeout - check if still connected
                if (!g_connected) break;
                // Give up semaphore and retry
                xSemaphoreGive(g_tx_sem);
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
        }

        // Wait if congested
        int congestion_wait = 0;
        while (g_tx_congested && g_connected && congestion_wait < 50) {
            vTaskDelay(pdMS_TO_TICKS(10));
            congestion_wait++;
        }
        if (!g_connected) {
            if (g_tx_sem) xSemaphoreGive(g_tx_sem);
            break;
        }

        size_t to_send = len - sent;
        if (to_send > chunk_size) to_send = chunk_size;

        esp_err_t ret = esp_ble_gatts_send_indicate(g_gatts_if, g_conn_id, g_tx_handle,
                                                     to_send, (uint8_t*)(data + sent), false);
        if (ret != ESP_OK) {
            // Don't LOG here to avoid recursion!
            if (g_tx_sem) xSemaphoreGive(g_tx_sem);
            break;
        }
        sent += to_send;
        // Semaphore will be given back by CONF_EVT callback
    }

    g_tx_in_progress = false;
    g_last_activity_ms = millis();
    return sent;
}

size_t ble_uart_print(const char *str) {
    if (!str) return 0;
    return ble_uart_send((const uint8_t*)str, strlen(str));
}

size_t ble_uart_printf(const char *fmt, ...) {
    if (!fmt) return 0;

    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len <= 0) return 0;
    if (len >= (int)sizeof(buf)) len = sizeof(buf) - 1;

    return ble_uart_send((const uint8_t*)buf, len);
}

size_t ble_uart_available(void) {
    if (!g_rx_ringbuf) return 0;

    size_t size;
    void *data = xRingbufferReceive(g_rx_ringbuf, &size, 0);
    if (data) {
        // Put it back (peek behavior)
        xRingbufferSend(g_rx_ringbuf, data, size, 0);
        vRingbufferReturnItem(g_rx_ringbuf, data);
        return size;
    }
    return 0;
}

// Single-byte read buffer for byte-by-byte getchar() access
static uint8_t g_rx_chunk[64];
static size_t g_rx_chunk_len = 0;
static size_t g_rx_chunk_pos = 0;

size_t ble_uart_read(uint8_t *buf, size_t max_len) {
    if (!g_rx_ringbuf || !buf || max_len == 0) return 0;

    size_t total_read = 0;

    // First, drain any remaining bytes from previous chunk
    while (g_rx_chunk_pos < g_rx_chunk_len && total_read < max_len) {
        buf[total_read++] = g_rx_chunk[g_rx_chunk_pos++];
    }

    // Then read new chunks from ringbuffer
    while (total_read < max_len) {
        size_t size;
        void *data = xRingbufferReceive(g_rx_ringbuf, &size, 0);
        if (!data) break;

        // Copy as much as we can directly to output buffer
        size_t to_copy = size;
        if (to_copy > max_len - total_read) to_copy = max_len - total_read;

        memcpy(buf + total_read, data, to_copy);
        total_read += to_copy;

        // If we couldn't fit the entire chunk, save remainder for next read
        if (to_copy < size) {
            size_t remainder = size - to_copy;
            if (remainder <= sizeof(g_rx_chunk)) {
                memcpy(g_rx_chunk, (uint8_t*)data + to_copy, remainder);
                g_rx_chunk_len = remainder;
                g_rx_chunk_pos = 0;
            }
            // else: data loss if chunk > 64 bytes (shouldn't happen with BLE MTU)
        }

        vRingbufferReturnItem(g_rx_ringbuf, data);
    }

    return total_read;
}

int ble_uart_getchar(void) {
    uint8_t c;
    if (ble_uart_read(&c, 1) == 1) {
        return c;
    }
    return -1;
}

void ble_uart_start_advertising(void) {
    if (g_initialized && !g_connected) {
        ble_core_start_advertising();
    }
}

void ble_uart_stop_advertising(void) {
    if (g_initialized) {
        ble_core_stop_advertising();
    }
}

void ble_uart_deinit(void) {
    if (!g_initialized) return;

    LOG_I(BLE_TAG, "Deinitializing BLE UART...");

    // Unregister from console first to stop any TX attempts
    console_register_ble_uart(NULL, NULL, NULL);

    // Stop reconnect timer
    if (g_reconnect_timer) {
        xTimerStop(g_reconnect_timer, portMAX_DELAY);
        xTimerDelete(g_reconnect_timer, portMAX_DELAY);
        g_reconnect_timer = NULL;
    }

    // Disconnect if connected
    if (g_connected && g_conn_id != 0) {
        esp_ble_gatts_close(g_gatts_if, g_conn_id);
        vTaskDelay(pdMS_TO_TICKS(100));  // Give time to disconnect
    }

    // Mark as not connected/initialized BEFORE deinit to stop any pending TX
    g_connected = false;
    g_tx_notify_enabled = false;
    g_initialized = false;

    // Stop advertising (shared with ble_core)
    ble_core_stop_advertising();
    vTaskDelay(pdMS_TO_TICKS(50));

    // Unregister GATT app (keep BT stack running for other services)
    if (g_gatts_if != ESP_GATT_IF_NONE) {
        esp_ble_gatts_app_unregister(g_gatts_if);
        g_gatts_if = ESP_GATT_IF_NONE;
    }

    // Remove callbacks from ble_core
    ble_core_set_passkey_callback(NULL);
    ble_core_set_auth_callback(NULL);

    // NOTE: Do NOT deinit BT stack here - other services may still use it
    // ble_core_deinit() should only be called when all services are done

    // Clean up TX semaphore
    if (g_tx_sem) {
        vSemaphoreDelete(g_tx_sem);
        g_tx_sem = NULL;
    }

    // Clean up RX ring buffer
    if (g_rx_ringbuf) {
        vRingbufferDelete(g_rx_ringbuf);
        g_rx_ringbuf = NULL;
    }

    // Reset all state
    g_service_handle = 0;
    g_rx_handle = 0;
    g_tx_handle = 0;
    g_tx_cccd_handle = 0;
    g_conn_id = 0;
    g_mtu = 23;
    g_bonded = false;
    g_tx_congested = false;
    g_tx_in_progress = false;
    g_power_mode = BLE_POWER_OFF;
    LOG_I(BLE_TAG, "BLE UART deinitialized");
}

void ble_uart_set_power_mode(ble_power_mode_t mode) {
    if (mode == g_power_mode) return;

    g_power_mode = mode;

    switch (mode) {
        case BLE_POWER_OFF:
            if (g_initialized) {
                ble_uart_stop_advertising();
            }
            LOG_I(BLE_TAG, "Power: OFF");
            break;

        case BLE_POWER_IDLE:
            if (g_initialized && !g_connected) {
                esp_ble_gap_stop_advertising();
                vTaskDelay(pdMS_TO_TICKS(50));
                esp_ble_gap_start_advertising(&adv_params_idle);
            }
            esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_N3);  // -3dBm
            LOG_I(BLE_TAG, "Power: IDLE");
            break;

        case BLE_POWER_ACTIVE:
            if (g_initialized && !g_connected) {
                esp_ble_gap_stop_advertising();
                vTaskDelay(pdMS_TO_TICKS(50));
                esp_ble_gap_start_advertising(&adv_params);
            }
            esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_N0);  // 0dBm (reduced for WiFi coexistence)
            LOG_I(BLE_TAG, "Power: ACTIVE");
            break;
    }

    g_last_activity_ms = millis();
}

ble_power_mode_t ble_uart_get_power_mode(void) {
    return g_power_mode;
}

void ble_uart_reset_activity(void) {
    g_last_activity_ms = millis();
}

uint32_t ble_uart_get_idle_time_ms(void) {
    if (g_last_activity_ms == 0) return 0;
    return millis() - g_last_activity_ms;
}

void ble_uart_set_rx_callback(ble_uart_rx_callback_t callback) {
    g_rx_callback = callback;
}

void ble_uart_set_passkey_display_callback(ble_passkey_display_cb_t callback) {
    g_passkey_display_cb = callback;
}

bool ble_uart_is_bonded(void) {
    return g_bonded;
}

void ble_uart_set_auth_complete_callback(ble_auth_complete_cb_t callback) {
    g_auth_complete_cb = callback;
}

#endif // FEATURE_BLE_UART
