// WiFi Manager Component
// Handles WiFi scanning, connection, and configuration storage

#include "wifi_manager.h"
#include "cdc_log.h"
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <lwip/dns.h>

// ============================================================================
// Configuration
// ============================================================================

#define WIFI_NVS_NAMESPACE      "wifi_cfg"
#define WIFI_NVS_KEY_CONFIG     "config"
#define WIFI_CONNECT_TIMEOUT_MS 15000
#define WIFI_SCAN_TIMEOUT_MS    10000

// Event bits
#define WIFI_CONNECTED_BIT      BIT0
#define WIFI_FAIL_BIT           BIT1
#define WIFI_SCAN_DONE_BIT      BIT2

// ============================================================================
// State
// ============================================================================

static struct {
    bool initialized;
    wifi_state_t state;
    EventGroupHandle_t event_group;
    esp_netif_t *netif;

    // Scan results (deduplicated)
    wifi_network_t networks[WIFI_MAX_NETWORKS];
    uint8_t network_count;

    // Connection info
    uint32_t ip_addr;
    uint32_t gateway;
    uint32_t subnet;
    uint32_t dns;
    uint32_t ntp_server;        // From DHCP option 42
    int8_t rssi;
    uint8_t channel;
    uint8_t bssid[6];
    char connected_ssid[WIFI_SSID_MAX_LEN];
    wifi_auth_mode_t connected_auth;
} g_wifi = {};

// ============================================================================
// Event Handler
// ============================================================================

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                LOG_I("WiFi", "STA started");
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                LOG_W("WiFi", "Disconnected");
                g_wifi.state = WIFI_STATE_FAILED;
                g_wifi.ip_addr = 0;
                xEventGroupSetBits(g_wifi.event_group, WIFI_FAIL_BIT);
                break;

            case WIFI_EVENT_SCAN_DONE:
                LOG_I("WiFi", "Scan complete");
                xEventGroupSetBits(g_wifi.event_group, WIFI_SCAN_DONE_BIT);
                break;

            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP:
                {
                    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
                    g_wifi.ip_addr = event->ip_info.ip.addr;
                    g_wifi.gateway = event->ip_info.gw.addr;
                    g_wifi.subnet = event->ip_info.netmask.addr;

                    // Get DNS
                    const ip_addr_t *dns_addr = dns_getserver(0);
                    if (dns_addr) {
                        g_wifi.dns = ip4_addr_get_u32(&dns_addr->u_addr.ip4);
                    }

                    // NTP server from DHCP is not directly accessible in ESP-IDF 5.x
                    // NTP sync will use pool.ntp.org as fallback
                    g_wifi.ntp_server = 0;

                    LOG_I("WiFi", "Got IP: %d.%d.%d.%d",
                          IP_BYTE(g_wifi.ip_addr, 0), IP_BYTE(g_wifi.ip_addr, 1),
                          IP_BYTE(g_wifi.ip_addr, 2), IP_BYTE(g_wifi.ip_addr, 3));

                    g_wifi.state = WIFI_STATE_CONNECTED;
                    xEventGroupSetBits(g_wifi.event_group, WIFI_CONNECTED_BIT);
                }
                break;

            default:
                break;
        }
    }
}

// ============================================================================
// Scan Deduplication
// ============================================================================

static void deduplicate_scan_results(wifi_ap_record_t *ap_records, uint16_t ap_count) {
    g_wifi.network_count = 0;

    for (uint16_t i = 0; i < ap_count && g_wifi.network_count < WIFI_MAX_NETWORKS; i++) {
        // Skip hidden networks
        if (ap_records[i].ssid[0] == '\0') continue;

        // Check if SSID already exists
        bool found = false;
        for (uint8_t j = 0; j < g_wifi.network_count; j++) {
            if (strcmp(g_wifi.networks[j].ssid, (char *)ap_records[i].ssid) == 0) {
                // Keep the one with stronger signal
                if (ap_records[i].rssi > g_wifi.networks[j].rssi) {
                    g_wifi.networks[j].rssi = ap_records[i].rssi;
                    g_wifi.networks[j].auth_mode = ap_records[i].authmode;
                }
                found = true;
                break;
            }
        }

        if (!found) {
            strncpy(g_wifi.networks[g_wifi.network_count].ssid,
                   (char *)ap_records[i].ssid, WIFI_SSID_MAX_LEN - 1);
            g_wifi.networks[g_wifi.network_count].ssid[WIFI_SSID_MAX_LEN - 1] = '\0';
            g_wifi.networks[g_wifi.network_count].rssi = ap_records[i].rssi;
            g_wifi.networks[g_wifi.network_count].auth_mode = ap_records[i].authmode;
            g_wifi.network_count++;
        }
    }

    // Sort by RSSI (strongest first)
    for (uint8_t i = 0; i < g_wifi.network_count - 1; i++) {
        for (uint8_t j = i + 1; j < g_wifi.network_count; j++) {
            if (g_wifi.networks[j].rssi > g_wifi.networks[i].rssi) {
                wifi_network_t temp = g_wifi.networks[i];
                g_wifi.networks[i] = g_wifi.networks[j];
                g_wifi.networks[j] = temp;
            }
        }
    }

    LOG_I("WiFi", "Found %d unique networks", g_wifi.network_count);
}

// ============================================================================
// Public API - Initialization
// ============================================================================

bool wifi_manager_init(void) {
    if (g_wifi.initialized) {
        return true;  // Already initialized
    }

    LOG_I("WiFi", "Initializing...");

    // Create event group
    g_wifi.event_group = xEventGroupCreate();
    if (!g_wifi.event_group) {
        LOG_E("WiFi", "Failed to create event group");
        return false;
    }

    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create default event loop if not exists
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        LOG_E("WiFi", "Failed to create event loop: %d", err);
        return false;
    }

    // Create default WiFi STA
    g_wifi.netif = esp_netif_create_default_wifi_sta();
    if (!g_wifi.netif) {
        LOG_E("WiFi", "Failed to create netif");
        return false;
    }

    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    // Set mode to STA
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    g_wifi.initialized = true;
    g_wifi.state = WIFI_STATE_IDLE;

    LOG_I("WiFi", "Initialized");
    return true;
}

void wifi_manager_deinit(void) {
    if (!g_wifi.initialized) return;

    LOG_I("WiFi", "Deinitializing...");

    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();

    if (g_wifi.netif) {
        esp_netif_destroy_default_wifi(g_wifi.netif);
        g_wifi.netif = NULL;
    }

    if (g_wifi.event_group) {
        vEventGroupDelete(g_wifi.event_group);
        g_wifi.event_group = NULL;
    }

    g_wifi.initialized = false;
    g_wifi.state = WIFI_STATE_IDLE;
    g_wifi.ip_addr = 0;

    LOG_I("WiFi", "Deinitialized");
}

bool wifi_manager_is_init(void) {
    return g_wifi.initialized;
}

// ============================================================================
// Public API - Scanning
// ============================================================================

void wifi_manager_start_scan(void) {
    if (!g_wifi.initialized) return;

    LOG_I("WiFi", "Starting scan...");

    g_wifi.state = WIFI_STATE_SCANNING;
    g_wifi.network_count = 0;

    xEventGroupClearBits(g_wifi.event_group, WIFI_SCAN_DONE_BIT);

    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = {
            .active = { .min = 100, .max = 300 },
            .passive = 0
        },
        .home_chan_dwell_time = 0,
        .channel_bitmap = {0, 0},
        .coex_background_scan = false,
    };

    esp_wifi_scan_start(&scan_config, false);  // Non-blocking
}

bool wifi_manager_scan_complete(void) {
    if (!g_wifi.initialized || g_wifi.state != WIFI_STATE_SCANNING) {
        return g_wifi.state == WIFI_STATE_SCAN_DONE;
    }

    EventBits_t bits = xEventGroupWaitBits(g_wifi.event_group,
                                            WIFI_SCAN_DONE_BIT,
                                            pdFALSE, pdFALSE, 0);

    if (bits & WIFI_SCAN_DONE_BIT) {
        // Get scan results
        uint16_t ap_count = 0;
        esp_wifi_scan_get_ap_num(&ap_count);

        if (ap_count > 0) {
            wifi_ap_record_t *ap_records = (wifi_ap_record_t *)malloc(
                ap_count * sizeof(wifi_ap_record_t));

            if (ap_records) {
                esp_wifi_scan_get_ap_records(&ap_count, ap_records);
                deduplicate_scan_results(ap_records, ap_count);
                free(ap_records);
            }
        }

        g_wifi.state = WIFI_STATE_SCAN_DONE;
        return true;
    }

    return false;
}

uint8_t wifi_manager_get_network_count(void) {
    return g_wifi.network_count;
}

bool wifi_manager_get_network(uint8_t idx, wifi_network_t *out) {
    if (!out || idx >= g_wifi.network_count) return false;

    *out = g_wifi.networks[idx];
    return true;
}

// ============================================================================
// Public API - Connection
// ============================================================================

void wifi_manager_connect(const wifi_config_stored_t *config) {
    if (!g_wifi.initialized || !config) return;

    LOG_I("WiFi", "Connecting to %s...", config->ssid);

    g_wifi.state = WIFI_STATE_CONNECTING;
    xEventGroupClearBits(g_wifi.event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    // Configure WiFi
    wifi_config_t wifi_config = {};
    strncpy((char *)wifi_config.sta.ssid, config->ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, config->password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = config->auth_mode;

    // Store connected info
    strncpy(g_wifi.connected_ssid, config->ssid, WIFI_SSID_MAX_LEN - 1);
    g_wifi.connected_auth = config->auth_mode;

    // Configure static IP if not DHCP
    if (!config->use_dhcp && config->static_ip != 0) {
        esp_netif_dhcpc_stop(g_wifi.netif);

        esp_netif_ip_info_t ip_info = {};
        ip_info.ip.addr = config->static_ip;
        ip_info.netmask.addr = config->subnet;
        ip_info.gw.addr = config->gateway;
        esp_netif_set_ip_info(g_wifi.netif, &ip_info);

        // Set DNS
        if (config->dns != 0) {
            ip_addr_t dns_addr;
            ip4_addr_set_u32(&dns_addr.u_addr.ip4, config->dns);
            dns_addr.type = IPADDR_TYPE_V4;
            dns_setserver(0, &dns_addr);
        }
    } else {
        esp_netif_dhcpc_start(g_wifi.netif);
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    esp_wifi_connect();
}

void wifi_manager_disconnect(void) {
    if (!g_wifi.initialized) return;

    esp_wifi_disconnect();
    g_wifi.state = WIFI_STATE_IDLE;
    g_wifi.ip_addr = 0;
}

wifi_state_t wifi_manager_get_state(void) {
    if (!g_wifi.initialized) return WIFI_STATE_IDLE;

    // Check for state changes via event bits
    if (g_wifi.state == WIFI_STATE_CONNECTING) {
        EventBits_t bits = xEventGroupWaitBits(g_wifi.event_group,
                                                WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                                pdFALSE, pdFALSE, 0);
        if (bits & WIFI_CONNECTED_BIT) {
            g_wifi.state = WIFI_STATE_CONNECTED;
        } else if (bits & WIFI_FAIL_BIT) {
            g_wifi.state = WIFI_STATE_FAILED;
        }
    }

    return g_wifi.state;
}

uint32_t wifi_manager_get_ip(void) {
    return g_wifi.ip_addr;
}

bool wifi_manager_get_info(wifi_info_t *info) {
    if (!info || g_wifi.state != WIFI_STATE_CONNECTED) return false;

    memset(info, 0, sizeof(wifi_info_t));

    strncpy(info->ssid, g_wifi.connected_ssid, WIFI_SSID_MAX_LEN - 1);
    info->ip = g_wifi.ip_addr;
    info->gateway = g_wifi.gateway;
    info->subnet = g_wifi.subnet;
    info->dns = g_wifi.dns;
    info->ntp_server = g_wifi.ntp_server;
    info->auth_mode = g_wifi.connected_auth;

    // Get MAC address
    esp_wifi_get_mac(WIFI_IF_STA, info->mac);

    // Get AP info (BSSID, RSSI, channel)
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        memcpy(info->bssid, ap_info.bssid, 6);
        info->rssi = ap_info.rssi;
        info->channel = ap_info.primary;
    }

    return true;
}

uint32_t wifi_manager_get_ntp_server(void) {
    return g_wifi.ntp_server;
}

// ============================================================================
// Public API - NVS Storage
// ============================================================================

bool wifi_manager_save_config(const wifi_config_stored_t *config) {
    if (!config) return false;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        LOG_E("WiFi", "NVS open failed: %d", err);
        return false;
    }

    err = nvs_set_blob(handle, WIFI_NVS_KEY_CONFIG, config, sizeof(wifi_config_stored_t));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);

    if (err != ESP_OK) {
        LOG_E("WiFi", "NVS write failed: %d", err);
        return false;
    }

    LOG_I("WiFi", "Config saved for %s", config->ssid);
    return true;
}

bool wifi_manager_load_config(wifi_config_stored_t *config) {
    if (!config) return false;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return false;
    }

    size_t size = sizeof(wifi_config_stored_t);
    err = nvs_get_blob(handle, WIFI_NVS_KEY_CONFIG, config, &size);
    nvs_close(handle);

    if (err != ESP_OK || size != sizeof(wifi_config_stored_t)) {
        return false;
    }

    LOG_I("WiFi", "Config loaded for %s", config->ssid);
    return true;
}

bool wifi_manager_has_config(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return false;
    }

    size_t size = 0;
    err = nvs_get_blob(handle, WIFI_NVS_KEY_CONFIG, NULL, &size);
    nvs_close(handle);

    return (err == ESP_OK && size == sizeof(wifi_config_stored_t));
}

void wifi_manager_clear_config(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_erase_key(handle, WIFI_NVS_KEY_CONFIG);
        nvs_commit(handle);
        nvs_close(handle);
        LOG_I("WiFi", "Config cleared");
    }
}

// ============================================================================
// Utility
// ============================================================================

const char *wifi_auth_mode_to_string(wifi_auth_mode_t mode) {
    switch (mode) {
        case WIFI_AUTH_OPEN:            return "Open";
        case WIFI_AUTH_WEP:             return "WEP";
        case WIFI_AUTH_WPA_PSK:         return "WPA";
        case WIFI_AUTH_WPA2_PSK:        return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2";
        case WIFI_AUTH_WPA3_PSK:        return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3";
        case WIFI_AUTH_WAPI_PSK:        return "WAPI";
        default:                        return "Unknown";
    }
}
