/**
 * ESP Hardware Implementation
 *
 * Access to ESP-specific hardware functions.
 */

#include "cdc_hal/IEspHardware.h"
#include "cdc_log.h"

#include "driver/temperature_sensor.h"

#include <limits>

static const char* TAG = "EspHardware";

namespace cdc::hal {

class EspHardware : public IEspHardware {
public:
    EspHardware() = default;

    // IService implementation
    bool init() override;
    bool start() override { state_ = core::ServiceState::STARTED; return true; }
    void stop() override { state_ = core::ServiceState::STOPPED; }
    core::ServiceState getState() const override { return state_; }
    const char* getName() const override { return "esp_hw"; }

    // IEspHardware implementation
    bool getTemperatureC(float* outC) override;

private:
    core::ServiceState state_ = core::ServiceState::UNINITIALIZED;
    temperature_sensor_handle_t sensor_ = nullptr;
    bool enabled_ = false;
};

bool EspHardware::init() {
    if (state_ != core::ServiceState::UNINITIALIZED) {
        return state_ == core::ServiceState::INITIALIZED ||
               state_ == core::ServiceState::STARTED;
    }

    temperature_sensor_config_t cfg = {
        .range_min = -10,
        .range_max = 80,
        .clk_src = TEMPERATURE_SENSOR_CLK_SRC_DEFAULT,
        .flags = {},
    };
    esp_err_t err = temperature_sensor_install(&cfg, &sensor_);
    if (err != ESP_OK) {
        LOG_W(TAG, "Temp sensor install failed: %d", err);
        state_ = core::ServiceState::ERROR;
        return false;
    }

    err = temperature_sensor_enable(sensor_);
    if (err != ESP_OK) {
        LOG_W(TAG, "Temp sensor enable failed: %d", err);
        temperature_sensor_uninstall(sensor_);
        sensor_ = nullptr;
        state_ = core::ServiceState::ERROR;
        return false;
    }

    enabled_ = true;
    state_ = core::ServiceState::INITIALIZED;
    return true;
}

bool EspHardware::getTemperatureC(float* outC) {
    if (!outC) return false;
    if (!sensor_ || !enabled_) {
        if (!init()) {
            return false;
        }
    }

    float tempC = std::numeric_limits<float>::quiet_NaN();
    esp_err_t err = temperature_sensor_get_celsius(sensor_, &tempC);
    if (err != ESP_OK) {
        LOG_W(TAG, "Temp sensor read failed: %d", err);
        return false;
    }

    *outC = tempC;
    return true;
}

static EspHardware g_espHardware;

IEspHardware* getEspHardwareInstance() {
    return &g_espHardware;
}

} // namespace cdc::hal
