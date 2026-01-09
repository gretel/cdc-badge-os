// WiFi Manager Component
// Handles WiFi scanning, connection, and configuration storage

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <esp_wifi_types.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Constants
// ============================================================================

#define WIFI_MAX_NETWORKS       20      // Max scan results
#define WIFI_SSID_MAX_LEN       33      // 32 + null
#define WIFI_PASSWORD_MAX_LEN   65      // 64 + null

// Helper macro for IP byte extraction
#define IP_BYTE(ip, n) (((ip) >> ((n) * 8)) & 0xFF)

// ============================================================================
// Types
// ============================================================================

// Scan result
typedef struct {
    char ssid[WIFI_SSID_MAX_LEN];
    int8_t rssi;
    wifi_auth_mode_t auth_mode;
} wifi_network_t;

// Stored config (NVS)
typedef struct {
    char ssid[WIFI_SSID_MAX_LEN];
    char password[WIFI_PASSWORD_MAX_LEN];
    wifi_auth_mode_t auth_mode;
    bool use_dhcp;
    uint32_t static_ip;
    uint32_t gateway;
    uint32_t subnet;
    uint32_t dns;
    uint32_t last_ip;           // Last successful IP
} wifi_config_stored_t;

// Connection info (for WiFi Details screen)
typedef struct {
    char ssid[WIFI_SSID_MAX_LEN];
    uint8_t mac[6];             // Station MAC
    uint8_t bssid[6];           // AP MAC
    uint32_t ip;
    uint32_t gateway;
    uint32_t subnet;
    uint32_t dns;
    uint32_t ntp_server;        // NTP server from DHCP (0 if not set)
    wifi_auth_mode_t auth_mode;
    int8_t rssi;
    uint8_t channel;
} wifi_info_t;

// WiFi state machine
typedef enum {
    WIFI_STATE_IDLE,
    WIFI_STATE_SCANNING,
    WIFI_STATE_SCAN_DONE,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_FAILED
} wifi_state_t;

// ============================================================================
// Initialization
// ============================================================================

/**
 * Initialize WiFi manager.
 * Initializes WiFi in STA mode.
 * Call before any other wifi_manager functions.
 *
 * @return true on success
 */
bool wifi_manager_init(void);

/**
 * Deinitialize WiFi manager.
 * Disconnects and turns off WiFi radio (power saving).
 */
void wifi_manager_deinit(void);

/**
 * Check if WiFi manager is initialized.
 */
bool wifi_manager_is_init(void);

// ============================================================================
// Scanning
// ============================================================================

/**
 * Start async WiFi scan.
 * Results available when wifi_manager_scan_complete() returns true.
 */
void wifi_manager_start_scan(void);

/**
 * Check if scan is complete.
 *
 * @return true when scan results are available
 */
bool wifi_manager_scan_complete(void);

/**
 * Get number of networks found.
 *
 * @return Number of unique SSIDs (deduplicated)
 */
uint8_t wifi_manager_get_network_count(void);

/**
 * Get network info by index.
 *
 * @param idx Network index (0 to count-1)
 * @param out Output structure
 * @return true if network exists
 */
bool wifi_manager_get_network(uint8_t idx, wifi_network_t *out);

// ============================================================================
// Connection
// ============================================================================

/**
 * Connect to WiFi using stored config.
 * Async - check wifi_manager_get_state() for result.
 *
 * @param config WiFi configuration
 */
void wifi_manager_connect(const wifi_config_stored_t *config);

/**
 * Disconnect from current network.
 */
void wifi_manager_disconnect(void);

/**
 * Get current WiFi state.
 *
 * @return Current state
 */
wifi_state_t wifi_manager_get_state(void);

/**
 * Get current IP address.
 *
 * @return IP address (0 if not connected)
 */
uint32_t wifi_manager_get_ip(void);

/**
 * Get detailed connection info.
 *
 * @param info Output structure
 * @return true if connected
 */
bool wifi_manager_get_info(wifi_info_t *info);

/**
 * Get NTP server from DHCP.
 *
 * @return NTP server IP (0 if not available from DHCP)
 */
uint32_t wifi_manager_get_ntp_server(void);

// ============================================================================
// NVS Storage
// ============================================================================

/**
 * Save WiFi configuration to NVS.
 *
 * @param config Configuration to save
 * @return true on success
 */
bool wifi_manager_save_config(const wifi_config_stored_t *config);

/**
 * Load WiFi configuration from NVS.
 *
 * @param config Output configuration
 * @return true if config exists
 */
bool wifi_manager_load_config(wifi_config_stored_t *config);

/**
 * Check if WiFi configuration exists in NVS.
 *
 * @return true if config exists
 */
bool wifi_manager_has_config(void);

/**
 * Clear WiFi configuration from NVS.
 */
void wifi_manager_clear_config(void);

// ============================================================================
// Session Management
// ============================================================================

/**
 * Acquire a WiFi session reference.
 * Call this before starting a WiFi operation.
 * WiFi will not be deinitialized while any session is active.
 *
 * @return Session ID (>0) or 0 on failure
 */
uint8_t wifi_manager_session_acquire(void);

/**
 * Release a WiFi session reference.
 * When all sessions are released, WiFi may be deinitialized.
 *
 * @param session_id Session ID from acquire
 * @param auto_deinit If true, deinit WiFi when last session released
 */
void wifi_manager_session_release(uint8_t session_id, bool auto_deinit);

/**
 * Get number of active sessions.
 *
 * @return Number of active session references
 */
uint8_t wifi_manager_session_count(void);

// ============================================================================
// Utility
// ============================================================================

/**
 * Convert auth mode to string.
 *
 * @param mode WiFi auth mode
 * @return Human-readable string
 */
const char *wifi_auth_mode_to_string(wifi_auth_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H
