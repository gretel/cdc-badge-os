#include "system_status.h"

#include "cdc_time.h"
#include "power_management.h"
#include "temp_sensor.h"

#include "esp_heap_caps.h"

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

    pos += snprintf(buf + pos, buf_size - pos, "\n--- Runtime ---\n\n");

    // Free Heap
    pos += snprintf(buf + pos, buf_size - pos, "Heap: %lu KB\n",
                    (unsigned long)(esp_get_free_heap_size() / 1024));

    // PSRAM
    size_t psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    pos += snprintf(buf + pos, buf_size - pos, "PSRAM: %lu KB\n",
                    (unsigned long)(psram / 1024));

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
