#pragma once

#include "cdc_core/IModule.h"
#include <led_strip.h>

namespace cdc::grove_led {

/**
 * LED Effect types
 */
enum class LedEffect : uint8_t {
    RAINBOW,
    STATIC
};

/**
 * Grove LED Module - Controls WS2813 LED strip via Grove port
 *
 * Features:
 * - Tools menu: Grove-LED submenu with LED Count, Brightness, Color, Effect
 * - Lock screen context: LED toggle (on/off)
 * - Rainbow and static color effects
 */
class GroveLedModule : public core::IModule {
public:
    static constexpr gpio_num_t GROVE_DATA_PIN = GPIO_NUM_2;  // Grove port data pin
    static constexpr uint8_t MAX_LEDS = 100;
    static constexpr uint8_t DEFAULT_LED_COUNT = 8;
    static constexpr uint8_t DEFAULT_BRIGHTNESS = 64;  // 25%

    // IService interface
    const char* getName() const override { return "grove_led"; }
    core::ServiceState getState() const override { return state_; }
    bool init() override;
    bool start() override;
    void stop() override;

    // IModule interface
    const char* getVersion() const override { return "1.0"; }
    bool isDefaultEnabled() const override { return false; }
    uint8_t getMenuItems(core::ModuleMenuItem* items, uint8_t maxItems) override;
    uint8_t getLockScreenContextItems(core::LockScreenContextItem* items, uint8_t maxItems) override;
    void onTick(uint32_t nowMs) override;

    // LED control
    void setEnabled(bool enabled);
    bool isEnabled() const { return enabled_; }

    // LED count (persisted to NVS)
    void setLedCount(uint8_t count);
    uint8_t getLedCount() const { return ledCount_; }

    // Brightness 0-255 (persisted to NVS)
    void setBrightness(uint8_t brightness);
    uint8_t getBrightness() const { return brightness_; }

    // Static color RGB (persisted to NVS)
    void setStaticColor(uint8_t r, uint8_t g, uint8_t b);
    uint8_t getStaticR() const { return staticR_; }
    uint8_t getStaticG() const { return staticG_; }
    uint8_t getStaticB() const { return staticB_; }

    // Effect (persisted to NVS)
    void setEffect(LedEffect effect);
    LedEffect getEffect() const { return effect_; }

    // Singleton access
    static GroveLedModule& instance();

private:
    GroveLedModule() = default;

    void updateRainbow(uint32_t nowMs);
    void updateStaticColor();
    void clearLeds();
    void loadSettings();
    void saveSettings();

    core::ServiceState state_ = core::ServiceState::UNINITIALIZED;
    led_strip_handle_t strip_ = nullptr;

    // Settings (persisted to NVS)
    uint8_t ledCount_ = DEFAULT_LED_COUNT;
    uint8_t brightness_ = DEFAULT_BRIGHTNESS;
    uint8_t staticR_ = 255;
    uint8_t staticG_ = 255;
    uint8_t staticB_ = 255;
    LedEffect effect_ = LedEffect::RAINBOW;

    // Runtime state
    bool enabled_ = false;
    uint32_t lastUpdateMs_ = 0;
    uint16_t rainbowOffset_ = 0;

    static constexpr uint32_t RAINBOW_UPDATE_INTERVAL_MS = 50;
};

} // namespace cdc::grove_led

// Module registration (called from main before runAllInitializers)
extern "C" void grove_led_register();
