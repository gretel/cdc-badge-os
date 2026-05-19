#include "cdc_os_ui/HardwareInfo.h"

#include "cdc_views/InfoView.h"
#include "cdc_ui/I18n.h"
#include "cdc_hal/II2cBus.h"
#include "cdc_hal/IPowerManager.h"
#include "cdc_hal/IKeypad.h"
#include "cdc_hal/IDisplay.h"
#include "cdc_hal/ISecureElement.h"
#include "cdc_hal/IEspHardware.h"
#include "cdc_hal/IWifiController.h"
#include "cdc_hal/IBluetoothController.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "nvs.h"

#include <cstdarg>
#include <cstdio>

namespace cdc::ui {

/**
 * \brief Builds localized hardware info text into a caller-provided buffer.
 * \param buf Destination character buffer.
 * \param bufSize Size of destination buffer.
 * \return void
 */
static void buildHardwareInfoText(char* buf, size_t bufSize) {
    if (!buf || bufSize == 0) return;
    size_t pos = 0;

    auto append = [&](const char* fmt, ...) {
        if (pos >= bufSize) return;
        va_list args;
        va_start(args, fmt);
        int written = vsnprintf(buf + pos, bufSize - pos, fmt, args);
        va_end(args);
        if (written > 0) {
            size_t w = static_cast<size_t>(written);
            pos += (w < (bufSize - pos)) ? w : (bufSize - pos - 1);
        }
    };

    const char* okText = tr(StringId::OK);
    const char* failText = tr(StringId::FAILED);
    const char* naText = tr(StringId::HW_NOT_AVAILABLE);

    append("=== %s ===\n", tr(StringId::HARDWARE_INFO));

    // I2C Bus (use bus 0 as primary)
    auto* i2c = hal::getI2cBus0();
    bool i2cOk = i2c && (i2c->getState() == core::ServiceState::INITIALIZED ||
                         i2c->getState() == core::ServiceState::STARTED);
    append("%s: %s\n", tr(StringId::HW_I2C_BUS), i2cOk ? okText : failText);

    // Power Management
    auto* power = hal::getPowerManagerInstance();
    bool powerOk = power && (power->getState() == core::ServiceState::INITIALIZED ||
                             power->getState() == core::ServiceState::STARTED);
    append("%s: %s\n", tr(StringId::HW_BQ25895), powerOk ? okText : failText);

    // Keypad
    auto* keypad = hal::getKeypadInstance();
    bool keypadOk = keypad && (keypad->getState() == core::ServiceState::INITIALIZED ||
                               keypad->getState() == core::ServiceState::STARTED);
    append("%s: %s\n", tr(StringId::HW_TCA9535), keypadOk ? okText : failText);

    // Display
    auto* display = hal::getDisplayInstance();
    bool displayOk = display && (display->getState() == core::ServiceState::INITIALIZED ||
                                  display->getState() == core::ServiceState::STARTED);
    append("%s: %s\n", tr(StringId::HW_DISPLAY), displayOk ? okText : failText);

    // TROPIC01
    auto* se = hal::getSecureElementInstance();
    bool seOk = se && (se->getState() == core::ServiceState::INITIALIZED ||
                       se->getState() == core::ServiceState::STARTED);
    append("%s: %s\n", tr(StringId::HW_TROPIC01), seOk ? okText : failText);
    append("%s: %s\n", tr(StringId::HW_TR01_SESSION),
           (se && se->isSessionActive()) ? okText : naText);

    // WiFi
    auto* wifi = hal::getWifiControllerInstance();
    bool wifiOk = wifi && (wifi->getState() == core::ServiceState::INITIALIZED ||
                           wifi->getState() == core::ServiceState::STARTED);
    append("%s: %s\n", tr(StringId::HW_WIFI), wifiOk ? okText : naText);

    // Bluetooth
    auto* ble = hal::getBluetoothControllerInstance();
    bool bleOk = ble && (ble->getState() == core::ServiceState::INITIALIZED ||
                         ble->getState() == core::ServiceState::STARTED);
    append("%s: %s\n", tr(StringId::HW_BLE), bleOk ? okText : naText);

    append("\n--- %s ---\n", tr(StringId::HW_SECTION_MEMORY));

    size_t freeHeap = esp_get_free_heap_size();
    size_t totalHeap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    size_t usedHeap = (totalHeap > freeHeap) ? (totalHeap - freeHeap) : 0;
    append("%s: %lu/%lu KB\n",
           tr(StringId::HW_HEAP),
           (unsigned long)(usedHeap / 1024),
           (unsigned long)(totalHeap / 1024));

    size_t intFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t intTotal = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    size_t intLargest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    append("DRAM: %lu/%lu KB free, largest %lu B\n",
           (unsigned long)(intFree / 1024),
           (unsigned long)(intTotal / 1024),
           (unsigned long)intLargest);

    size_t dmaFree = heap_caps_get_free_size(MALLOC_CAP_DMA);
    size_t dmaLargest = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
    append("DMA: %lu KB free, largest %lu B\n",
           (unsigned long)(dmaFree / 1024),
           (unsigned long)dmaLargest);

    size_t psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t psramTotal = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    size_t psramUsed = (psramTotal > psramFree) ? (psramTotal - psramFree) : 0;
    append("%s: %lu/%lu KB\n",
           tr(StringId::HW_PSRAM),
           (unsigned long)(psramUsed / 1024),
           (unsigned long)(psramTotal / 1024));

    nvs_stats_t nvsStats;
    if (nvs_get_stats(nullptr, &nvsStats) == ESP_OK) {
        append("%s: %lu/%lu %s\n",
               tr(StringId::HW_NVS),
               (unsigned long)nvsStats.used_entries,
               (unsigned long)nvsStats.total_entries,
               tr(StringId::HW_ENTRIES));
    }

    append("\n--- %s ---\n", tr(StringId::HW_SECTION_RUNTIME));

    if (power) {
        const char* chg = (power->getChargeStatus() == hal::ChargeStatus::FAST_CHARGE ||
                           power->getChargeStatus() == hal::ChargeStatus::PRE_CHARGE)
                              ? tr(StringId::HW_CHARGING_SUFFIX)
                              : "";
        append("%s: %u%%%s\n", tr(StringId::HW_BATTERY), power->getBatteryPercent(), chg);
    } else {
        append("%s: %s\n", tr(StringId::HW_BATTERY), naText);
    }

    float tempC = 0.0f;
    auto* espHw = hal::getEspHardwareInstance();
    if (espHw && espHw->getTemperatureC(&tempC)) {
        append("%s: %.1f C\n", tr(StringId::HW_TEMP), tempC);
    } else {
        append("%s: %s\n", tr(StringId::HW_TEMP), naText);
    }

    uint64_t uptimeS = esp_timer_get_time() / 1000000ULL;
    append("%s: %llu s\n", tr(StringId::HW_UPTIME), (unsigned long long)uptimeS);
}

/**
 * \brief Opens hardware info screen using shared info view.
 * \return void
 */
void showHardwareInfo() {
    static char hwInfo[512];
    buildHardwareInfoText(hwInfo, sizeof(hwInfo));
    showInfo(tr(StringId::HARDWARE_INFO), hwInfo);
}

} // namespace cdc::ui
