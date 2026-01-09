// GUI Module for CDC Badge
// Display abstraction with async refresh support

#include "gui.h"
#include "hw_config.h"
#include "cdc_log.h"
#include "driver/ledc.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>

// App info for splash screen
#ifndef APP_NAME
#define APP_NAME "CDC Badge"
#endif
#ifndef APP_VERSION
#define APP_VERSION "v0.3"
#endif

// SPI and Display instances
static EpdSpi epd_io;
static Gdey029T94 display(epd_io);
static bool gui_initialized = false;

// Backlight state
static uint16_t backlight_level = GUI_BACKLIGHT_DEFAULT;
static bool backlight_is_on = true;

// Async render state
static SemaphoreHandle_t g_render_mutex = NULL;
static volatile bool g_render_pending = false;
static volatile bool g_render_full = false;
static TaskHandle_t g_render_task = NULL;

// NVS namespace and key
#define NVS_NAMESPACE "display"
#define NVS_KEY_BACKLIGHT "backlight"

// LEDC configuration for backlight
#define LEDC_TIMER          LEDC_TIMER_0
#define LEDC_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL        LEDC_CHANNEL_0
#define LEDC_DUTY_RES       LEDC_TIMER_10_BIT
#define LEDC_FREQUENCY      10000

// Render task stack size
#define RENDER_TASK_STACK_SIZE 4096
#define RENDER_TASK_PRIORITY 5

// Apply backlight level to hardware
static void apply_backlight(uint16_t level) {
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, level);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

// Render task - runs display updates in background
static void render_task(void *arg) {
    while (true) {
        // Wait for render request
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Get render parameters under mutex
        xSemaphoreTake(g_render_mutex, portMAX_DELAY);
        bool do_full = g_render_full;
        g_render_pending = false;
        xSemaphoreGive(g_render_mutex);

        // Perform the actual display update (blocking within this task)
        if (do_full) {
            display.update();
        } else {
            // Full-screen partial refresh using native dimensions
            display.updateWindow(0, 0, 128, 296, false);
        }
    }
}

void gui_load_backlight(void) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) == ESP_OK) {
        uint16_t saved = GUI_BACKLIGHT_DEFAULT;
        if (nvs_get_u16(nvs, NVS_KEY_BACKLIGHT, &saved) == ESP_OK) {
            backlight_level = saved;
            if (backlight_level > GUI_BACKLIGHT_MAX) {
                backlight_level = GUI_BACKLIGHT_MAX;
            }
            LOG_I("GUI", "Loaded backlight: %d", backlight_level);
        }
        nvs_close(nvs);
    }
}

void gui_save_backlight(void) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_u16(nvs, NVS_KEY_BACKLIGHT, backlight_level);
        nvs_commit(nvs);
        nvs_close(nvs);
        LOG_I("GUI", "Saved backlight: %d", backlight_level);
    }
}

void gui_init(void) {
    if (gui_initialized) return;

    LOG_I("GUI", "Initializing display...");

    // Load backlight from NVS first
    gui_load_backlight();

    // Configure backlight PWM
    // Use XTAL clock (40MHz) for light sleep compatibility
    // LEDC_SLEEP_MODE_KEEP_ALIVE keeps PWM running during light sleep
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_DUTY_RES,
        .timer_num = LEDC_TIMER,
        .freq_hz = LEDC_FREQUENCY,
        .clk_cfg = LEDC_USE_XTAL_CLK,  // Light sleep compatible
        .deconfigure = false
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .gpio_num = EPD_LED_PIN,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = backlight_level,
        .hpoint = 0,
        .sleep_mode = LEDC_SLEEP_MODE_KEEP_ALIVE,  // Keep running during light sleep
        .flags = {.output_invert = 0}
    };
    ledc_channel_config(&ledc_channel);

    // Initialize display (CalEPD uses Kconfig for pins)
    display.init(false);  // debug=false
    display.setRotation(1);  // Landscape
    display.setMonoMode(true);  // Mono only (partial refresh needs this)
    display.fillScreen(EPD_WHITE);

    // Create render mutex and task
    g_render_mutex = xSemaphoreCreateMutex();
    xTaskCreate(render_task, "gui_render", RENDER_TASK_STACK_SIZE,
                NULL, RENDER_TASK_PRIORITY, &g_render_task);

    backlight_is_on = true;
    gui_initialized = true;
    LOG_I("GUI", "Display initialized (296x128), backlight=%d", backlight_level);
}

void gui_show_splash(void) {
    LOG_I("GUI", "Showing splash screen");

    display.fillScreen(EPD_BLACK);
    display.setTextColor(EPD_WHITE);

    // App name - large, centered
    display.setFont(&FreeMonoBold12pt7b);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(APP_NAME, 0, 0, &x1, &y1, &w, &h);
    int name_x = (display.width() - w) / 2;
    display.setCursor(name_x, 55);
    display.print(APP_NAME);

    // Version - smaller, centered below name
    display.setFont(&FreeMonoBold9pt7b);
    display.getTextBounds(APP_VERSION, 0, 0, &x1, &y1, &w, &h);
    int ver_x = (display.width() - w) / 2;
    display.setCursor(ver_x, 80);
    display.print(APP_VERSION);

    // Small text at bottom - use built-in font (6x8 pixels)
    display.setFont(NULL);
    display.setTextSize(1);

    // Left bottom: Build date (MMDD HH:MM format)
    char build_str[16];
    // Extract from __DATE__ ("Jan  1 2025") and __TIME__ ("12:34:56")
    snprintf(build_str, sizeof(build_str), "%.3s%2.2s %.5s", __DATE__, __DATE__ + 4, __TIME__);
    display.setCursor(2, 120);
    display.print(build_str);

    // Right bottom: "updating cache"
    const char *cache_text = "updating cache";
    int cache_x = display.width() - (strlen(cache_text) * 6) - 2;  // 6 pixels per char
    display.setCursor(cache_x, 120);
    display.print(cache_text);

    // Full refresh (synchronous - we want to see it)
    display.update();
    LOG_I("GUI", "Splash screen displayed");
}

void gui_clear(void) {
    display.fillScreen(EPD_WHITE);
}

void gui_flush(bool full_refresh) {
    if (!g_render_task) {
        // Fallback: sync render if task not running
        if (full_refresh) {
            display.update();
        } else {
            display.updateWindow(0, 0, 128, 296, false);
        }
        return;
    }

    // Request async render
    xSemaphoreTake(g_render_mutex, portMAX_DELAY);
    // If already pending, upgrade to full if requested
    if (g_render_pending) {
        if (full_refresh) g_render_full = true;
    } else {
        g_render_pending = true;
        g_render_full = full_refresh;
    }
    xSemaphoreGive(g_render_mutex);

    // Notify render task
    xTaskNotifyGive(g_render_task);
}

void gui_flush_full(void) {
    gui_flush(true);
}

void gui_flush_partial(void) {
    gui_flush(false);
}

void gui_flush_sync(bool full_refresh) {
    // Synchronous flush - wait for completion
    if (full_refresh) {
        display.update();
    } else {
        display.updateWindow(0, 0, 128, 296, false);
    }
}

void gui_set_backlight(uint16_t level) {
    if (level > GUI_BACKLIGHT_MAX) level = GUI_BACKLIGHT_MAX;
    backlight_level = level;
    if (backlight_is_on) {
        apply_backlight(level);
    }
}

uint16_t gui_get_backlight(void) {
    return backlight_level;
}

void gui_backlight_on(void) {
    backlight_is_on = true;
    apply_backlight(backlight_level);
}

void gui_backlight_off(void) {
    backlight_is_on = false;
    apply_backlight(0);
}

void gui_backlight_toggle(void) {
    if (backlight_is_on) {
        gui_backlight_off();
    } else {
        gui_backlight_on();
    }
}

bool gui_is_backlight_on(void) {
    return backlight_is_on;
}

Gdey029T94& gui_get_display(void) {
    return display;
}
