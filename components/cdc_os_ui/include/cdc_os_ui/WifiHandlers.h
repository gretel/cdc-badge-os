#pragma once

#include "cdc_hal/IWifiController.h"
#include <cstdint>

namespace cdc::ui {

// WiFi timeout constants (milliseconds)
static constexpr uint32_t WIFI_CONNECT_TIMEOUT_DEFAULT_MS = 15000;
static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MIN_MS     = 3000;
static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MAX_MS     = 60000;
static constexpr uint32_t WIFI_SCAN_TIMEOUT_MS = 10000;
static constexpr uint32_t NTP_SYNC_TIMEOUT_MS = 10000;

// WiFi Setup Wizard State
struct WifiWizard {
    char ssid[33] = {};
    char password[65] = {};
    hal::WifiSecurity security = hal::WifiSecurity::WPA2_PSK;
    bool useDhcp = true;
    char staticIp[16] = {};
    char gateway[16] = {};
    char netmask[16] = "255.255.255.0";
    bool fromScan = false;

    void reset();
};

// WiFi Stored Configuration
struct WifiConfig {
    char ssid[33] = {};
    char password[65] = {};
    uint8_t security = 0;
    bool useDhcp = true;
    uint32_t staticIp = 0;
    uint32_t gateway = 0;
    uint32_t netmask = 0;
    bool valid = false;
};

// WiFi management handler class
class WifiHandlers {
public:
    static WifiHandlers& instance();

    // Config persistence
    void loadConfig();
    void saveConfig();
    WifiConfig& config() { return config_; }
    WifiWizard& wizard() { return wizard_; }

    /**
     * \brief Stores credentials directly (WPA2-PSK, DHCP) and persists them.
     *
     * Resets the wizard, fills SSID/password, marks DHCP+WPA2, then writes to NVS
     * via `saveConfig()` and reloads the cached config.
     *
     * \param ssid Null-terminated SSID (max 32 chars, truncated if longer).
     * \param password Null-terminated password (nullptr or "" for open networks).
     */
    void saveCredentials(const char* ssid, const char* password);

    /**
     * \brief Erases the entire "wifi" NVS namespace and invalidates the cached
     *        configuration.
     */
    void clearConfig();

    /**
     * \brief Reads the persisted connect timeout (NVS key "tout").
     * \return Timeout in ms, clamped to [WIFI_CONNECT_TIMEOUT_MIN_MS,
     *         WIFI_CONNECT_TIMEOUT_MAX_MS]; default
     *         WIFI_CONNECT_TIMEOUT_DEFAULT_MS if unset.
     */
    uint32_t getConnectTimeoutMs() const;

    /**
     * \brief Persists the connect timeout to NVS.
     * \param ms Timeout in ms; must lie within
     *           [WIFI_CONNECT_TIMEOUT_MIN_MS, WIFI_CONNECT_TIMEOUT_MAX_MS].
     * \return `true` on success, `false` if the value is out of range or NVS
     *         could not be opened.
     */
    bool setConnectTimeoutMs(uint32_t ms);

    // Connection management
    bool connect();
    void disconnect();
    bool isConnected() const;

    /**
     * \brief Ensures the device is connected to WiFi and optionally syncs
     *        time, leaving the connection up for the caller to use.
     *
     * Idempotent: if already connected, returns true after triggering an NTP
     * sync only when the system clock has not been set yet.
     *
     * The caller owns the lifetime of the connection and must call
     * \ref disconnect() when done. There is no auto-shutdown.
     *
     * \return `true` if WiFi is connected on return.
     */
    bool ensureConnected();

    /**
     * \brief Synchronizes system time via NTP.
     *
     * When `disconnectAfter` is `true` (default), the function reproduces the
     * legacy "connect, sync, disconnect" pattern used for one-shot time
     * fetches. When `false`, the caller is expected to manage the WiFi
     * lifetime; the function will reuse an active connection or open one
     * without tearing it down on return.
     *
     * If the RTC reports an already-valid time, the function returns `true`
     * without contacting any NTP server.
     */
    bool syncNtp(bool disconnectAfter = true);

    // Helper: IP validation
    static bool isValidIpAddress(const char* ip);

    // Get connection error message
    const char* getLastError() const { return lastError_; }

private:
    WifiHandlers() = default;

    WifiConfig config_;
    WifiWizard wizard_;
    const char* lastError_ = nullptr;

    static bool isValidIpOctet(int val);
    uint32_t parseIpAddress(const char* ip) const;
};

} // namespace cdc::ui
