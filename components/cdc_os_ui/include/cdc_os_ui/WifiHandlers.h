#pragma once

#include "cdc_hal/IWifiController.h"
#include <cstdint>

namespace cdc::ui {

// WiFi timeout constants (milliseconds)
static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
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

    // Connection management
    bool connect();
    void disconnect();
    bool isConnected() const;

    // NTP sync (connects if needed, disconnects if it connected)
    bool syncNtp();

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
