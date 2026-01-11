#include "temp_sensor.h"

#include "cdc_log.h"

#include "driver/temperature_sensor.h"
#include "esp_err.h"

static temperature_sensor_handle_t g_temp_handle = nullptr;
static bool g_temp_ready = false;

bool temp_sensor_init(void) {
    if (g_temp_ready) return true;

    temperature_sensor_config_t config = {
        .range_min = -10,
        .range_max = 80,
        .clk_src = TEMPERATURE_SENSOR_CLK_SRC_DEFAULT,
        .flags = {
            .allow_pd = 0,
        },
    };

    esp_err_t err = temperature_sensor_install(&config, &g_temp_handle);
    if (err != ESP_OK) {
        LOG_W("TEMP", "install failed: %s", esp_err_to_name(err));
        return false;
    }

    err = temperature_sensor_enable(g_temp_handle);
    if (err != ESP_OK) {
        LOG_W("TEMP", "enable failed: %s", esp_err_to_name(err));
        return false;
    }

    g_temp_ready = true;
    return true;
}

bool temp_sensor_get_celsius(float *out_celsius) {
    if (!out_celsius) return false;
    if (!temp_sensor_init()) return false;

    float temp = 0.0f;
    esp_err_t err = temperature_sensor_get_celsius(g_temp_handle, &temp);
    if (err != ESP_OK) {
        LOG_W("TEMP", "read failed: %s", esp_err_to_name(err));
        return false;
    }

    *out_celsius = temp;
    return true;
}
