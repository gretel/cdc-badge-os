/**
 * E-Paper Display HAL Implementation
 * Wraps CalEPD library for Gdey029T94 (296x128 B/W)
 *
 * WICHTIG: Alle Objekte werden LAZY initialisiert um Crashes
 * durch globale Konstruktoren zu vermeiden!
 */

#include "cdc_hal/IDisplay.h"
#include "cdc_hal/hw_config.h"
#include "cdc_log.h"
#include "driver/ledc.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <goodisplay/gdey029T94.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <cstring>
#include <cstdarg>

static const char* TAG = "EpaperDisplay";

// Splash screen text
static constexpr const char* SPLASH_TITLE = "CDC Badge";
static constexpr const char* SPLASH_VERSION = "v0.5";

namespace cdc::hal {

// LEDC configuration for backlight
static constexpr ledc_timer_t LEDC_TIMER = LEDC_TIMER_0;
static constexpr ledc_mode_t LEDC_MODE = LEDC_LOW_SPEED_MODE;
static constexpr ledc_channel_t LEDC_CHANNEL = LEDC_CHANNEL_0;
static constexpr ledc_timer_bit_t LEDC_DUTY_RES = LEDC_TIMER_10_BIT;
static constexpr uint32_t LEDC_FREQUENCY = 10000;

// NVS keys
static constexpr const char* NVS_NAMESPACE = "display";
static constexpr const char* NVS_KEY_BACKLIGHT = "backlight";

// Display constants
static constexpr uint16_t WIDTH = 296;
static constexpr uint16_t HEIGHT = 128;
static constexpr uint16_t BACKLIGHT_DEFAULT = 512;
static constexpr uint16_t BACKLIGHT_MAX = 1023;

// LAZY initialized display objects - NO global constructors!
static EpdSpi* s_epd_spi = nullptr;
static Gdey029T94* s_epd_display = nullptr;

// Display state
static bool s_initialized = false;
static uint16_t s_backlightLevel = BACKLIGHT_DEFAULT;
static bool s_backlightOn = true;

// Render task
static SemaphoreHandle_t s_renderMutex = nullptr;
static TaskHandle_t s_renderTask = nullptr;
static volatile bool s_renderPending = false;
static volatile bool s_renderFull = false;

static void applyBacklight(uint16_t level) {
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, level);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

static void loadBacklight() {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) == ESP_OK) {
        uint16_t saved = BACKLIGHT_DEFAULT;
        if (nvs_get_u16(nvs, NVS_KEY_BACKLIGHT, &saved) == ESP_OK) {
            s_backlightLevel = (saved > BACKLIGHT_MAX) ? BACKLIGHT_MAX : saved;
        }
        nvs_close(nvs);
    }
}

static void persistBacklight(uint16_t level) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_u16(nvs, NVS_KEY_BACKLIGHT, level);
        nvs_commit(nvs);
        nvs_close(nvs);
        LOG_D(TAG, "Backlight saved to NVS: %u", level);
    }
}

static void renderTask(void* arg) {
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        xSemaphoreTake(s_renderMutex, portMAX_DELAY);
        bool doFull = s_renderFull;
        s_renderPending = false;
        xSemaphoreGive(s_renderMutex);

        if (s_epd_display) {
            if (doFull) {
                s_epd_display->update();
            } else {
                s_epd_display->updateWindow(0, 0, HEIGHT, WIDTH, false);  // Physical dimensions: 128x296
            }
        }
    }
}

/**
 * Simple display wrapper using static functions
 */
class EpaperDisplay : public IDisplay {
public:
    bool init() override;
    bool start() override;
    void stop() override;
    core::ServiceState getState() const override { return state_; }
    const char* getName() const override { return "display"; }

    void clear() override;
    void flush(RefreshMode mode) override;
    void flushSync(RefreshMode mode) override;
    bool isBusy() const override { return s_renderPending; }
    uint16_t getWidth() const override { return WIDTH; }
    uint16_t getHeight() const override { return HEIGHT; }
    void setBacklight(uint16_t level) override;
    void saveBacklight() override;
    uint16_t getBacklight() const override { return s_backlightLevel; }
    bool isBacklightOn() const override { return s_backlightOn && s_backlightLevel > 0; }
    void backlightOn() override;
    void backlightOff() override;
    void* getNativeHandle() override { return s_epd_display; }
    void showSplash() override;

    // GFX Drawing Methods
    void drawPixel(int16_t x, int16_t y, uint16_t color) override;
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) override;
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
    void setCursor(int16_t x, int16_t y) override;
    void setTextColor(uint16_t color) override;
    void setTextSize(uint8_t size) override;
    void setFont(const void* font) override;
    void print(const char* text) override;
    void printf(const char* fmt, ...) override;

private:
    core::ServiceState state_ = core::ServiceState::UNINITIALIZED;
};

bool EpaperDisplay::init() {
    if (s_initialized) {
        return true;
    }

    LOG_I(TAG, "Initializing E-Paper display...");

    // Load backlight from NVS
    loadBacklight();

    // Configure backlight PWM
    ledc_timer_config_t ledcTimer = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_DUTY_RES,
        .timer_num = LEDC_TIMER,
        .freq_hz = LEDC_FREQUENCY,
        .clk_cfg = LEDC_USE_XTAL_CLK,
        .deconfigure = false
    };
    ledc_timer_config(&ledcTimer);

    ledc_channel_config_t ledcChannel = {
        .gpio_num = EPD_LED_PIN,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = s_backlightLevel,
        .hpoint = 0,
        .sleep_mode = LEDC_SLEEP_MODE_KEEP_ALIVE,
        .flags = {.output_invert = 0}
    };
    ledc_channel_config(&ledcChannel);

    LOG_I(TAG, "Backlight configured");

    // LAZY create display objects
    if (!s_epd_spi) {
        s_epd_spi = new EpdSpi();
    }
    if (!s_epd_display) {
        s_epd_display = new Gdey029T94(*s_epd_spi);
    }

    // Initialize display
    s_epd_display->init(false);
    s_epd_display->setRotation(1);
    s_epd_display->setMonoMode(true);
    s_epd_display->fillScreen(EPD_WHITE);

    LOG_I(TAG, "Display hardware initialized");

    // Create render task
    s_renderMutex = xSemaphoreCreateMutex();
    if (!s_renderMutex) {
        LOG_E(TAG, "Failed to create render mutex");
        state_ = core::ServiceState::ERROR;
        return false;
    }

    BaseType_t ret = xTaskCreate(renderTask, "epd_render", 8192, nullptr, 5, &s_renderTask);
    if (ret != pdPASS) {
        LOG_E(TAG, "Failed to create render task");
        state_ = core::ServiceState::ERROR;
        return false;
    }

    s_initialized = true;
    state_ = core::ServiceState::INITIALIZED;
    LOG_I(TAG, "Display initialized (%ux%u), backlight=%u", WIDTH, HEIGHT, s_backlightLevel);
    return true;
}

bool EpaperDisplay::start() {
    if (state_ == core::ServiceState::INITIALIZED) {
        state_ = core::ServiceState::STARTED;
        s_backlightOn = true;
        applyBacklight(s_backlightLevel);
        return true;
    }
    return state_ == core::ServiceState::STARTED;
}

void EpaperDisplay::stop() {
    if (state_ == core::ServiceState::STARTED) {
        s_backlightOn = false;
        applyBacklight(0);
        state_ = core::ServiceState::STOPPED;
    }
}

void EpaperDisplay::clear() {
    if (s_epd_display) {
        s_epd_display->fillScreen(EPD_WHITE);
    }
}

void EpaperDisplay::flush(RefreshMode mode) {
    // If no render task, fall back to sync
    if (!s_renderTask) {
        flushSync(mode);
        return;
    }

    xSemaphoreTake(s_renderMutex, portMAX_DELAY);
    if (s_renderPending) {
        // Merge request: if FULL is requested, upgrade to FULL
        if (mode == RefreshMode::FULL) s_renderFull = true;
    } else {
        s_renderPending = true;
        s_renderFull = (mode == RefreshMode::FULL);
    }
    xSemaphoreGive(s_renderMutex);

    xTaskNotifyGive(s_renderTask);
}

void EpaperDisplay::flushSync(RefreshMode mode) {
    if (!s_epd_display) return;
    if (mode == RefreshMode::FULL) {
        s_epd_display->update();
    } else {
        s_epd_display->updateWindow(0, 0, HEIGHT, WIDTH, false);  // Physical dimensions: 128x296
    }
}

void EpaperDisplay::setBacklight(uint16_t level) {
    if (level > BACKLIGHT_MAX) level = BACKLIGHT_MAX;
    s_backlightLevel = level;
    // Always apply immediately for live preview (e.g., brightness slider)
    // Also turn on backlight if level > 0
    if (level > 0) {
        s_backlightOn = true;
    }
    applyBacklight(level);
}

void EpaperDisplay::saveBacklight() {
    persistBacklight(s_backlightLevel);
}

void EpaperDisplay::backlightOn() {
    s_backlightOn = true;
    applyBacklight(s_backlightLevel);
    LOG_I(TAG, "Backlight ON (level=%u)", s_backlightLevel);
}

void EpaperDisplay::backlightOff() {
    s_backlightOn = false;
    applyBacklight(0);
    LOG_I(TAG, "Backlight OFF");
}

void EpaperDisplay::showSplash() {
    if (!s_epd_display) return;

    LOG_I(TAG, "Showing splash screen");

    s_epd_display->fillScreen(EPD_BLACK);
    s_epd_display->setTextColor(EPD_WHITE);

    // App name - large, centered
    s_epd_display->setFont(&FreeMonoBold12pt7b);
    int16_t x1, y1;
    uint16_t w, h;
    s_epd_display->getTextBounds(SPLASH_TITLE, 0, 0, &x1, &y1, &w, &h);
    int name_x = (s_epd_display->width() - w) / 2;
    s_epd_display->setCursor(name_x, 55);
    s_epd_display->print(SPLASH_TITLE);

    // Version - smaller, centered below name
    s_epd_display->setFont(&FreeMonoBold9pt7b);
    s_epd_display->getTextBounds(SPLASH_VERSION, 0, 0, &x1, &y1, &w, &h);
    int ver_x = (s_epd_display->width() - w) / 2;
    s_epd_display->setCursor(ver_x, 80);
    s_epd_display->print(SPLASH_VERSION);

    // Small text at bottom - built-in font (6x8)
    s_epd_display->setFont(nullptr);
    s_epd_display->setTextSize(1);

    // Build date
    char build_str[20];
    snprintf(build_str, sizeof(build_str), "%.3s%2.2s %.5s", __DATE__, __DATE__ + 4, __TIME__);
    s_epd_display->setCursor(2, 120);
    s_epd_display->print(build_str);

    // Right: "Modular Rewrite"
    const char* status_text = "Modular Rewrite";
    int status_x = s_epd_display->width() - (strlen(status_text) * 6) - 2;
    s_epd_display->setCursor(status_x, 120);
    s_epd_display->print(status_text);

    // Full refresh (blocking)
    s_epd_display->update();
    LOG_I(TAG, "Splash screen displayed");
}

// === GFX Drawing Method Implementations ===

void EpaperDisplay::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (s_epd_display) s_epd_display->drawPixel(x, y, color);
}

void EpaperDisplay::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    if (s_epd_display) s_epd_display->drawLine(x0, y0, x1, y1, color);
}

void EpaperDisplay::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (s_epd_display) s_epd_display->drawRect(x, y, w, h, color);
}

void EpaperDisplay::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (s_epd_display) s_epd_display->fillRect(x, y, w, h, color);
}

void EpaperDisplay::setCursor(int16_t x, int16_t y) {
    if (s_epd_display) s_epd_display->setCursor(x, y);
}

void EpaperDisplay::setTextColor(uint16_t color) {
    if (s_epd_display) s_epd_display->setTextColor(color);
}

void EpaperDisplay::setTextSize(uint8_t size) {
    if (s_epd_display) s_epd_display->setTextSize(size);
}

void EpaperDisplay::setFont(const void* font) {
    if (s_epd_display) s_epd_display->setFont(static_cast<const GFXfont*>(font));
}

void EpaperDisplay::print(const char* text) {
    if (s_epd_display && text) s_epd_display->print(text);
}

void EpaperDisplay::printf(const char* fmt, ...) {
    if (!s_epd_display || !fmt) return;
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    s_epd_display->print(buf);
}

// Singleton - LAZY created on first call
static EpaperDisplay* s_display = nullptr;

IDisplay* getDisplayInstance() {
    if (!s_display) {
        s_display = new EpaperDisplay();
    }
    return s_display;
}

} // namespace cdc::hal
