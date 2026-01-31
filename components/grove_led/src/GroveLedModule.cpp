#include "grove_led/GroveLedModule.h"
#include "grove_led/RgbInputView.h"
#include "cdc_core/ModuleRegistry.h"
#include "cdc_ui/I18n.h"
#include "cdc_ui/ViewStack.h"
#include "cdc_views/SliderView.h"
#include "cdc_views/ListView.h"
#include "cdc_log.h"
#include <nvs_flash.h>
#include <nvs.h>
#include <cstring>

static const char* TAG = "GroveLED";

namespace cdc::grove_led {

// NVS storage - all settings persisted
// Uses mod_ prefix for automatic cleanup when module is removed
static constexpr const char* NVS_NAMESPACE = "mod_grove_led";
static constexpr const char* NVS_KEY_ENABLED = "enabled";
static constexpr const char* NVS_KEY_LED_COUNT = "led_count";
static constexpr const char* NVS_KEY_BRIGHTNESS = "bright";
static constexpr const char* NVS_KEY_COLOR_R = "color_r";
static constexpr const char* NVS_KEY_COLOR_G = "color_g";
static constexpr const char* NVS_KEY_COLOR_B = "color_b";
static constexpr const char* NVS_KEY_EFFECT = "effect";

// Module-specific string IDs (registered dynamically)
static uint16_t s_strIdBase = 0;
static constexpr uint16_t STR_LEDS = 0;
static constexpr uint16_t STR_GROVE_LED = 1;
static constexpr uint16_t STR_LED_COUNT = 2;
static constexpr uint16_t STR_BRIGHTNESS = 3;
static constexpr uint16_t STR_COLOR = 4;
static constexpr uint16_t STR_EFFECT = 5;
static constexpr uint16_t STR_RAINBOW = 6;
static constexpr uint16_t STR_RED = 7;
static constexpr uint16_t STR_GREEN = 8;
static constexpr uint16_t STR_BLUE = 9;
static constexpr uint16_t STR_ON = 10;
static constexpr uint16_t STR_OFF = 11;
static constexpr uint16_t STR_COUNT = 12;

// Helper to get module string
static const char* mstr(uint16_t offset) {
    return ui::tr(s_strIdBase + offset);
}

static void registerStrings() {
    auto& i18n = ui::I18n::instance();
    s_strIdBase = i18n.registerModule("grove_led", STR_COUNT);

    if (s_strIdBase == 0) {
        LOG_E(TAG, "Failed to register i18n strings");
        return;
    }

    // English
    i18n.registerTranslation(s_strIdBase + STR_LEDS, ui::Language::EN, "LEDs");
    i18n.registerTranslation(s_strIdBase + STR_GROVE_LED, ui::Language::EN, "Grove-LED");
    i18n.registerTranslation(s_strIdBase + STR_LED_COUNT, ui::Language::EN, "LED Count");
    i18n.registerTranslation(s_strIdBase + STR_BRIGHTNESS, ui::Language::EN, "Brightness");
    i18n.registerTranslation(s_strIdBase + STR_COLOR, ui::Language::EN, "Color");
    i18n.registerTranslation(s_strIdBase + STR_EFFECT, ui::Language::EN, "Effect");
    i18n.registerTranslation(s_strIdBase + STR_RAINBOW, ui::Language::EN, "Rainbow");
    i18n.registerTranslation(s_strIdBase + STR_RED, ui::Language::EN, "Red");
    i18n.registerTranslation(s_strIdBase + STR_GREEN, ui::Language::EN, "Green");
    i18n.registerTranslation(s_strIdBase + STR_BLUE, ui::Language::EN, "Blue");
    i18n.registerTranslation(s_strIdBase + STR_ON, ui::Language::EN, "On");
    i18n.registerTranslation(s_strIdBase + STR_OFF, ui::Language::EN, "Off");

    // German
    i18n.registerTranslation(s_strIdBase + STR_LEDS, ui::Language::DE, "LEDs");
    i18n.registerTranslation(s_strIdBase + STR_GROVE_LED, ui::Language::DE, "Grove-LED");
    i18n.registerTranslation(s_strIdBase + STR_LED_COUNT, ui::Language::DE, "LED Anzahl");
    i18n.registerTranslation(s_strIdBase + STR_BRIGHTNESS, ui::Language::DE, "Helligkeit");
    i18n.registerTranslation(s_strIdBase + STR_COLOR, ui::Language::DE, "Farbe");
    i18n.registerTranslation(s_strIdBase + STR_EFFECT, ui::Language::DE, "Effekt");
    i18n.registerTranslation(s_strIdBase + STR_RAINBOW, ui::Language::DE, "Regenbogen");
    i18n.registerTranslation(s_strIdBase + STR_RED, ui::Language::DE, "Rot");
    i18n.registerTranslation(s_strIdBase + STR_GREEN, ui::Language::DE, "Gruen");
    i18n.registerTranslation(s_strIdBase + STR_BLUE, ui::Language::DE, "Blau");
    i18n.registerTranslation(s_strIdBase + STR_ON, ui::Language::DE, "An");
    i18n.registerTranslation(s_strIdBase + STR_OFF, ui::Language::DE, "Aus");

    LOG_I(TAG, "Registered i18n strings (base=%d)", s_strIdBase);
}

GroveLedModule& GroveLedModule::instance() {
    static GroveLedModule inst;
    return inst;
}

bool GroveLedModule::init() {
    LOG_I(TAG, "Initializing Grove LED module");

    // Register i18n strings first
    registerStrings();

    loadSettings();

    // Configure LED strip
    led_strip_config_t strip_config = {
        .strip_gpio_num = GROVE_DATA_PIN,
        .max_leds = MAX_LEDS,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,  // WS2812/WS2813 use GRB
        .led_model = LED_MODEL_WS2812,  // WS2813 is compatible with WS2812
        .flags = {
            .invert_out = false,
        }
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,  // 10MHz
        .mem_block_symbols = 64,
        .flags = {
            .with_dma = false,
        }
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &strip_);
    if (err != ESP_OK) {
        LOG_E(TAG, "Failed to create LED strip: %s", esp_err_to_name(err));
        return false;
    }

    clearLeds();
    state_ = core::ServiceState::INITIALIZED;
    return true;
}

bool GroveLedModule::start() {
    if (!strip_) {
        LOG_E(TAG, "Cannot start: LED strip not initialized");
        return false;
    }

    LOG_I(TAG, "Starting Grove LED module (count=%d)", ledCount_);
    state_ = core::ServiceState::STARTED;
    return true;
}

void GroveLedModule::stop() {
    LOG_I(TAG, "Stopping Grove LED module");
    setEnabled(false);
    state_ = core::ServiceState::STOPPED;
}

// =============================================================================
// Main Menu (Grove-LED submenu)
// =============================================================================

static ui::ListView* s_mainMenu = nullptr;
static ui::ListItem s_mainMenuItems[5];
static char s_enableLabel[24];  // Dynamic label for "LEDs: On/Off"

// Sub-views
static ui::SliderView* s_ledCountSlider = nullptr;
static ui::SliderView* s_brightnessSlider = nullptr;
static RgbInputView* s_rgbInput = nullptr;
static ui::ListView* s_effectMenu = nullptr;
static ui::ListItem s_effectMenuItems[1];

// Forward declarations
static void showLedCountView();
static void showBrightnessView();
static void showColorInput();
static void showEffectMenu();

static void onMainMenuSelect(uint16_t index, void* userData) {
    (void)userData;
    switch (index) {
        case 0:
            // Toggle LEDs on/off
            GroveLedModule::instance().setEnabled(!GroveLedModule::instance().isEnabled());
            // Update menu label
            snprintf(s_enableLabel, sizeof(s_enableLabel), "%s: %s",
                     mstr(STR_LEDS),
                     GroveLedModule::instance().isEnabled() ? mstr(STR_ON) : mstr(STR_OFF));
            s_mainMenuItems[0].label = s_enableLabel;
            s_mainMenu->markDirty();
            break;
        case 1: showLedCountView(); break;
        case 2: showBrightnessView(); break;
        case 3: showColorInput(); break;
        case 4: showEffectMenu(); break;
    }
}

static ui::IView* getGroveLedMenu() {
    if (!s_mainMenu) {
        s_mainMenu = new ui::ListView();
    }

    // Dynamic label for enable/disable
    snprintf(s_enableLabel, sizeof(s_enableLabel), "%s: %s",
             mstr(STR_LEDS),
             GroveLedModule::instance().isEnabled() ? mstr(STR_ON) : mstr(STR_OFF));

    s_mainMenuItems[0] = {s_enableLabel, 0, false, nullptr};
    s_mainMenuItems[1] = {mstr(STR_LED_COUNT), 0, false, nullptr};
    s_mainMenuItems[2] = {mstr(STR_BRIGHTNESS), 0, false, nullptr};
    s_mainMenuItems[3] = {mstr(STR_COLOR), 0, false, nullptr};
    s_mainMenuItems[4] = {mstr(STR_EFFECT), 0, false, nullptr};

    s_mainMenu->init(mstr(STR_GROVE_LED), s_mainMenuItems, 5);
    s_mainMenu->setOnSelect(onMainMenuSelect);
    return s_mainMenu;
}

// =============================================================================
// LED Count Slider
// =============================================================================

static void onLedCountSave(uint16_t value) {
    GroveLedModule::instance().setLedCount(static_cast<uint8_t>(value));
}

static void showLedCountView() {
    if (!s_ledCountSlider) {
        s_ledCountSlider = new ui::SliderView();
    }
    s_ledCountSlider->init(
        mstr(STR_LED_COUNT),
        1,
        GroveLedModule::MAX_LEDS,
        GroveLedModule::instance().getLedCount(),
        1
    );
    s_ledCountSlider->setOnSave(onLedCountSave);
    ui::ViewStack::instance().push(s_ledCountSlider);
}

// =============================================================================
// Brightness Slider
// =============================================================================

static void onBrightnessSave(uint16_t value) {
    GroveLedModule::instance().setBrightness(static_cast<uint8_t>(value));
}

static void onBrightnessChange(uint16_t value) {
    // Live preview
    GroveLedModule::instance().setBrightness(static_cast<uint8_t>(value));
}

static void showBrightnessView() {
    if (!s_brightnessSlider) {
        s_brightnessSlider = new ui::SliderView();
    }
    s_brightnessSlider->init(
        mstr(STR_BRIGHTNESS),
        0,
        255,
        GroveLedModule::instance().getBrightness(),
        5
    );
    s_brightnessSlider->setOnSave(onBrightnessSave);
    s_brightnessSlider->setOnChange(onBrightnessChange);
    ui::ViewStack::instance().push(s_brightnessSlider);
}

// =============================================================================
// Color Input (RGB)
// =============================================================================

static void onColorConfirm(uint8_t r, uint8_t g, uint8_t b) {
    GroveLedModule::instance().setStaticColor(r, g, b);
}

static void showColorInput() {
    if (!s_rgbInput) {
        s_rgbInput = new RgbInputView();
    }

    auto& module = GroveLedModule::instance();
    s_rgbInput->init(mstr(STR_COLOR), module.getStaticR(), module.getStaticG(), module.getStaticB());
    s_rgbInput->setOnConfirm(onColorConfirm);
    ui::ViewStack::instance().push(s_rgbInput);
}

// =============================================================================
// Effect Menu
// =============================================================================

static void onEffectMenuSelect(uint16_t index, void* userData) {
    (void)userData;
    auto& module = GroveLedModule::instance();

    switch (index) {
        case 0:
            module.setEffect(LedEffect::RAINBOW);
            break;
    }
    // Pop back to previous menu
    ui::ViewStack::instance().pop();
}

static void showEffectMenu() {
    if (!s_effectMenu) {
        s_effectMenu = new ui::ListView();
    }

    s_effectMenuItems[0] = {mstr(STR_RAINBOW), 0, false, nullptr};

    s_effectMenu->init(mstr(STR_EFFECT), s_effectMenuItems, 1);
    s_effectMenu->setOnSelect(onEffectMenuSelect);
    ui::ViewStack::instance().push(s_effectMenu);
}

// =============================================================================
// Module Interface
// =============================================================================

uint8_t GroveLedModule::getMenuItems(core::ModuleMenuItem* items, uint8_t maxItems) {
    if (!items || maxItems == 0) return 0;

    items[0] = {
        .label = mstr(STR_GROVE_LED),
        .priority = 50,
        .getView = getGroveLedMenu,
        .isVisible = nullptr,
        .moduleName = nullptr,
        .location = core::MenuLocation::TOOLS_MENU
    };

    return 1;
}

// === Lock Screen Context Menu ===

static const char* getLedToggleLabel() {
    return mstr(STR_LEDS);
}

static void onLedToggle() {
    auto& module = GroveLedModule::instance();
    module.setEnabled(!module.isEnabled());
}

uint8_t GroveLedModule::getLockScreenContextItems(core::LockScreenContextItem* items, uint8_t maxItems) {
    if (!items || maxItems == 0) return 0;

    items[0] = {
        .getLabel = getLedToggleLabel,
        .callback = onLedToggle,
        .priority = 50,
        .moduleName = nullptr
    };

    return 1;
}

// =============================================================================
// LED Control
// =============================================================================

void GroveLedModule::setEnabled(bool enabled) {
    if (enabled_ != enabled) {
        enabled_ = enabled;
        saveSettings();
        LOG_I(TAG, "LEDs %s", enabled ? "enabled" : "disabled");

        if (!enabled) {
            clearLeds();
        }
    }
}

void GroveLedModule::setLedCount(uint8_t count) {
    if (count < 1) count = 1;
    if (count > MAX_LEDS) count = MAX_LEDS;
    ledCount_ = count;
    saveSettings();
    LOG_I(TAG, "LED count set to %d", count);
}

void GroveLedModule::setBrightness(uint8_t brightness) {
    brightness_ = brightness;
    saveSettings();
}

void GroveLedModule::setEffect(LedEffect effect) {
    effect_ = effect;
    saveSettings();
    LOG_I(TAG, "Effect set to %d", (int)effect);
}

void GroveLedModule::setStaticColor(uint8_t r, uint8_t g, uint8_t b) {
    staticR_ = r;
    staticG_ = g;
    staticB_ = b;
    effect_ = LedEffect::STATIC;
    saveSettings();
    LOG_I(TAG, "Static color set to R=%d G=%d B=%d", r, g, b);
}

void GroveLedModule::onTick(uint32_t nowMs) {
    if (!enabled_ || !strip_) return;

    if (nowMs - lastUpdateMs_ >= RAINBOW_UPDATE_INTERVAL_MS) {
        lastUpdateMs_ = nowMs;

        switch (effect_) {
            case LedEffect::RAINBOW:
                updateRainbow(nowMs);
                break;
            case LedEffect::STATIC:
                updateStaticColor();
                break;
        }
    }
}

void GroveLedModule::updateRainbow(uint32_t nowMs) {
    (void)nowMs;

    // Rainbow effect: cycle through hues
    for (uint8_t i = 0; i < ledCount_; i++) {
        uint16_t hue = (rainbowOffset_ + (i * 360 / ledCount_)) % 360;

        // HSV to RGB conversion
        uint8_t r, g, b;
        uint8_t region = hue / 60;
        uint8_t remainder = (hue - (region * 60)) * 255 / 60;

        switch (region) {
            case 0:  r = 255; g = remainder; b = 0; break;
            case 1:  r = 255 - remainder; g = 255; b = 0; break;
            case 2:  r = 0; g = 255; b = remainder; break;
            case 3:  r = 0; g = 255 - remainder; b = 255; break;
            case 4:  r = remainder; g = 0; b = 255; break;
            default: r = 255; g = 0; b = 255 - remainder; break;
        }

        // Apply brightness
        r = (r * brightness_) / 255;
        g = (g * brightness_) / 255;
        b = (b * brightness_) / 255;

        led_strip_set_pixel(strip_, i, r, g, b);
    }

    // Clear remaining LEDs
    for (uint8_t i = ledCount_; i < MAX_LEDS; i++) {
        led_strip_set_pixel(strip_, i, 0, 0, 0);
    }

    led_strip_refresh(strip_);
    rainbowOffset_ = (rainbowOffset_ + 5) % 360;
}

void GroveLedModule::updateStaticColor() {
    // Apply brightness to static color
    uint8_t r = (staticR_ * brightness_) / 255;
    uint8_t g = (staticG_ * brightness_) / 255;
    uint8_t b = (staticB_ * brightness_) / 255;

    for (uint8_t i = 0; i < ledCount_; i++) {
        led_strip_set_pixel(strip_, i, r, g, b);
    }

    // Clear remaining LEDs
    for (uint8_t i = ledCount_; i < MAX_LEDS; i++) {
        led_strip_set_pixel(strip_, i, 0, 0, 0);
    }

    led_strip_refresh(strip_);
}

void GroveLedModule::clearLeds() {
    if (!strip_) return;
    led_strip_clear(strip_);
}

void GroveLedModule::loadSettings() {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        uint8_t val = 0;

        // Enabled state
        if (nvs_get_u8(handle, NVS_KEY_ENABLED, &val) == ESP_OK) {
            enabled_ = (val != 0);
        }

        // LED count
        if (nvs_get_u8(handle, NVS_KEY_LED_COUNT, &val) == ESP_OK) {
            ledCount_ = (val >= 1 && val <= MAX_LEDS) ? val : DEFAULT_LED_COUNT;
        }

        // Brightness
        if (nvs_get_u8(handle, NVS_KEY_BRIGHTNESS, &val) == ESP_OK) {
            brightness_ = val;
        }

        // Color RGB
        if (nvs_get_u8(handle, NVS_KEY_COLOR_R, &val) == ESP_OK) {
            staticR_ = val;
        }
        if (nvs_get_u8(handle, NVS_KEY_COLOR_G, &val) == ESP_OK) {
            staticG_ = val;
        }
        if (nvs_get_u8(handle, NVS_KEY_COLOR_B, &val) == ESP_OK) {
            staticB_ = val;
        }

        // Effect
        if (nvs_get_u8(handle, NVS_KEY_EFFECT, &val) == ESP_OK) {
            effect_ = static_cast<LedEffect>(val);
        }

        nvs_close(handle);
        LOG_I(TAG, "Loaded: en=%d cnt=%d bri=%d R=%d G=%d B=%d eff=%d",
              enabled_, ledCount_, brightness_, staticR_, staticG_, staticB_, (int)effect_);
    }
}

void GroveLedModule::saveSettings() {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_u8(handle, NVS_KEY_ENABLED, enabled_ ? 1 : 0);
        nvs_set_u8(handle, NVS_KEY_LED_COUNT, ledCount_);
        nvs_set_u8(handle, NVS_KEY_BRIGHTNESS, brightness_);
        nvs_set_u8(handle, NVS_KEY_COLOR_R, staticR_);
        nvs_set_u8(handle, NVS_KEY_COLOR_G, staticG_);
        nvs_set_u8(handle, NVS_KEY_COLOR_B, staticB_);
        nvs_set_u8(handle, NVS_KEY_EFFECT, static_cast<uint8_t>(effect_));
        nvs_commit(handle);
        nvs_close(handle);
    }
}

} // namespace cdc::grove_led

// === Module Registration ===

extern "C" void grove_led_register() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        auto& moduleReg = cdc::core::ModuleRegistry::instance();
        auto& module = cdc::grove_led::GroveLedModule::instance();

        // Register first so module appears in list (even if init fails)
        moduleReg.registerModule(&module);

        if (!module.init()) {
            // Init failed - report error
            moduleReg.reportModuleError(module.getName(), "LED strip init failed");
            return;
        }

        module.start();
    });
}
