#include "cdc_os_ui/WifiHandlers.h"
#include "cdc_hal/IWifiController.h"
#include "cdc_hal/IRtc.h"
#include "nvs.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <cstdio>

namespace cdc::ui {

void WifiWizard::reset() {
    memset(this, 0, sizeof(*this));
    useDhcp = true;
    strncpy(netmask, "255.255.255.0", sizeof(netmask));
}

WifiHandlers& WifiHandlers::instance() {
    static WifiHandlers s_instance;
    return s_instance;
}

bool WifiHandlers::isValidIpOctet(int val) {
    return val >= 0 && val <= 255;
}

bool WifiHandlers::isValidIpAddress(const char* ip) {
    if (!ip || !ip[0]) return false;
    int a, b, c, d;
    if (sscanf(ip, "%d.%d.%d.%d", &a, &b, &c, &d) != 4) return false;
    return isValidIpOctet(a) && isValidIpOctet(b) && isValidIpOctet(c) && isValidIpOctet(d);
}

uint32_t WifiHandlers::parseIpAddress(const char* ip) const {
    int a, b, c, d;
    if (sscanf(ip, "%d.%d.%d.%d", &a, &b, &c, &d) != 4) return 0;
    if (!isValidIpOctet(a) || !isValidIpOctet(b) || !isValidIpOctet(c) || !isValidIpOctet(d)) return 0;
    return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) |
           (static_cast<uint32_t>(c) << 8) | static_cast<uint32_t>(d);
}

void WifiHandlers::loadConfig() {
    nvs_handle_t nvs;
    if (nvs_open("wifi", NVS_READONLY, &nvs) != ESP_OK) {
        config_.valid = false;
        return;
    }

    size_t len = sizeof(config_.ssid);
    if (nvs_get_str(nvs, "ssid", config_.ssid, &len) != ESP_OK || len <= 1) {
        nvs_close(nvs);
        config_.valid = false;
        return;
    }

    len = sizeof(config_.password);
    nvs_get_str(nvs, "pass", config_.password, &len);
    nvs_get_u8(nvs, "sec", &config_.security);

    uint8_t dhcp = 1;
    nvs_get_u8(nvs, "dhcp", &dhcp);
    config_.useDhcp = (dhcp != 0);

    nvs_get_u32(nvs, "ip", &config_.staticIp);
    nvs_get_u32(nvs, "gw", &config_.gateway);
    nvs_get_u32(nvs, "nm", &config_.netmask);

    nvs_close(nvs);
    config_.valid = true;
}

void WifiHandlers::saveConfig() {
    nvs_handle_t nvs;
    if (nvs_open("wifi", NVS_READWRITE, &nvs) != ESP_OK) return;

    nvs_set_str(nvs, "ssid", wizard_.ssid);
    nvs_set_str(nvs, "pass", wizard_.password);
    nvs_set_u8(nvs, "sec", static_cast<uint8_t>(wizard_.security));
    nvs_set_u8(nvs, "dhcp", wizard_.useDhcp ? 1 : 0);

    // Parse and save static IP config
    if (!wizard_.useDhcp) {
        nvs_set_u32(nvs, "ip", parseIpAddress(wizard_.staticIp));
        nvs_set_u32(nvs, "gw", parseIpAddress(wizard_.gateway));
        nvs_set_u32(nvs, "nm", parseIpAddress(wizard_.netmask));
    }

    nvs_commit(nvs);
    nvs_close(nvs);

    // Reload config
    loadConfig();
}

bool WifiHandlers::isConnected() const {
    auto* wifi = hal::getWifiControllerInstance();
    return wifi && wifi->isConnected();
}

bool WifiHandlers::connect() {
    if (!config_.valid) {
        lastError_ = "No config";
        return false;
    }

    auto* wifi = hal::getWifiControllerInstance();
    if (!wifi) {
        lastError_ = "No WiFi HW";
        return false;
    }

    // Enable WiFi if needed
    if (!wifi->isEnabled()) {
        if (!wifi->enable(hal::WifiMode::STA)) {
            lastError_ = "WiFi init failed";
            return false;
        }
    }

    bool connected = wifi->connect(config_.ssid, config_.password, WIFI_CONNECT_TIMEOUT_MS);

    if (!connected || !wifi->isConnected()) {
        hal::WifiState state = wifi->getWifiState();
        switch (state) {
            case hal::WifiState::DISCONNECTED: lastError_ = "Disconnected"; break;
            case hal::WifiState::CONNECTION_FAILED: lastError_ = "Auth failed"; break;
            default: lastError_ = "Timeout"; break;
        }
        return false;
    }

    lastError_ = nullptr;
    return true;
}

void WifiHandlers::disconnect() {
    auto* wifi = hal::getWifiControllerInstance();
    if (!wifi) return;

    if (wifi->isConnected()) {
        wifi->disconnect();
    }
    if (wifi->isEnabled()) {
        wifi->disable();
    }
}

bool WifiHandlers::syncNtp() {
    auto* wifi = hal::getWifiControllerInstance();
    if (!wifi) {
        lastError_ = "No WiFi HW";
        return false;
    }

    // Track if we established the connection ourselves
    bool weConnected = false;

    // If not connected, try to connect using saved config
    if (!wifi->isConnected()) {
        if (!config_.valid) {
            lastError_ = "No config";
            return false;
        }

        // Enable WiFi and connect
        if (!wifi->isEnabled()) {
            wifi->enable(hal::WifiMode::STA);
        }

        bool connected = wifi->connect(config_.ssid, config_.password, WIFI_CONNECT_TIMEOUT_MS);

        if (!connected || !wifi->isConnected()) {
            lastError_ = "Connect failed";
            return false;
        }

        weConnected = true;
    }

    // Initialize SNTP if not done
    static bool sntpInited = false;
    if (!sntpInited) {
        esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_setservername(1, "time.google.com");
        esp_sntp_init();
        sntpInited = true;
    } else {
        esp_sntp_restart();
    }

    // Wait for sync
    uint32_t startMs = esp_timer_get_time() / 1000;
    bool synced = false;

    while ((esp_timer_get_time() / 1000 - startMs) < NTP_SYNC_TIMEOUT_MS) {
        if (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
            synced = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Disconnect if we established the connection
    if (weConnected) {
        wifi->disconnect();
        wifi->disable();
    }

    if (synced) {
        // Save time to RTC/NVS for persistence
        auto* rtc = hal::getRtcInstance();
        if (rtc) {
            rtc->markTimeSet();
        }
        lastError_ = nullptr;
        return true;
    }

    lastError_ = "NTP timeout";
    return false;
}

} // namespace cdc::ui
