#include "system_status.h"

#include "cdc_time.h"
#include "power_management.h"
#include "temp_sensor.h"

#include "esp_heap_caps.h"
#include "nvs_flash.h"

#include <cstdio>

void system_status_build_selftest_text(const hw_status_t *status, char *buf, size_t buf_size) {
    int pos = 0;

    pos += snprintf(buf + pos, buf_size - pos, "=== Hardware Test ===\n\n");

    // I2C Bus
    pos += snprintf(buf + pos, buf_size - pos, "I2C Bus: %s\n",
                    status->i2c_ok ? "OK" : "FAIL");

    // Power Management (BQ25895)
    pos += snprintf(buf + pos, buf_size - pos, "BQ25895: %s\n",
                    status->power_ok ? "OK" : "FAIL");

    // Keypad (TCA9535)
    pos += snprintf(buf + pos, buf_size - pos, "TCA9535: %s\n",
                    status->keypad_ok ? "OK" : "FAIL");

    // TROPIC01
    pos += snprintf(buf + pos, buf_size - pos, "TROPIC01: %s\n",
                    status->tropic01_ok ? "OK" : "FAIL");

    // TROPIC01 Session
    pos += snprintf(buf + pos, buf_size - pos, "TR01 Session: %s\n",
                    status->tropic01_session_ok ? "OK" : "---");

    pos += snprintf(buf + pos, buf_size - pos, "\n--- Memory ---\n\n");

    // Internal Heap
    size_t free_heap = esp_get_free_heap_size();
    size_t min_heap = esp_get_minimum_free_heap_size();
    pos += snprintf(buf + pos, buf_size - pos, "Heap: %lu/%lu KB\n",
                    (unsigned long)(free_heap / 1024),
                    (unsigned long)(min_heap / 1024));

    // PSRAM
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    pos += snprintf(buf + pos, buf_size - pos, "PSRAM: %lu/%lu KB\n",
                    (unsigned long)(psram_free / 1024),
                    (unsigned long)(psram_total / 1024));

    // NVS
    nvs_stats_t nvs_stats;
    if (nvs_get_stats(NULL, &nvs_stats) == ESP_OK) {
        pos += snprintf(buf + pos, buf_size - pos, "NVS: %lu/%lu entries\n",
                        (unsigned long)nvs_stats.used_entries,
                        (unsigned long)nvs_stats.total_entries);
    }

    pos += snprintf(buf + pos, buf_size - pos, "\n--- Runtime ---\n\n");

    // Battery
    pos += snprintf(buf + pos, buf_size - pos, "Battery: %d%% %s\n",
                    power_get_battery_percent(),
                    power_is_charging() ? "(chg)" : "");

    // Temperature (internal sensor)
    float temp_c = 0.0f;
    if (temp_sensor_get_celsius(&temp_c)) {
        pos += snprintf(buf + pos, buf_size - pos, "Temp: %.1f C\n", (double)temp_c);
    }

    // Uptime
    pos += snprintf(buf + pos, buf_size - pos, "Uptime: %lu s\n",
                    (unsigned long)(millis() / 1000));
}
