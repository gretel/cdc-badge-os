#include "app_power.h"

#include "app_globals.h"
#include "app_render.h"

#include "cdc_log.h"
#include "gui.h"
#include "hw_config.h"
#include "power_management.h"

#include "driver/gpio.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Light sleep configuration
#define LIGHT_SLEEP_WAKEUP_INTERVAL_US (60 * 1000000ULL)  // 60 seconds

// Configure and enter light sleep (lock screen only)
void enter_light_sleep(void) {
    if (!g_light_sleep_configured) {
        // Timer wakeup
        esp_sleep_enable_timer_wakeup(LIGHT_SLEEP_WAKEUP_INTERVAL_US);

        // GPIO wakeup (keypad interrupt) - level triggered
        gpio_wakeup_enable(EXP_IRQ_PIN, GPIO_INTR_LOW_LEVEL);
        esp_sleep_enable_gpio_wakeup();

        g_light_sleep_configured = true;
        LOG_I("SLEEP", "Light sleep configured (GPIO%d + 10s timer)", EXP_IRQ_PIN);
    }

    // Prepare GPIO before sleep (disable interrupt to avoid conflicts)
    power_prepare_gpio_for_sleep();

    // Enter light sleep
    esp_light_sleep_start();

    // Log wakeup cause
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_GPIO) {
        LOG_D("SLEEP", "GPIO wakeup");
    }

    // Stabilize GPIO after wakeup (wait for key release, restore edge-trigger)
    power_stabilize_gpio_after_sleep();
}

// Enter deep sleep mode (only GPIO wakeup, no timer)
void enter_deep_sleep(void) {
    LOG_I("SLEEP", "Entering deep sleep mode...");

    // Mark that we're in deep sleep mode (survives reset)
    g_in_deep_sleep_mode = true;

    // Turn off backlight
    gui_backlight_off();

    // Show lock screen without clock/date, with sleep icon (time won't update in deep sleep)
    update_lock_screen_data();
    g_lock_screen.clock[0] = '\0';  // Hide clock
    g_lock_screen.date[0] = '\0';   // Hide date
    g_lock_screen.show_lock_icon = false;
    g_lock_screen.show_sleep_icon = true;
    g_lock_screen.backlight_on = false;  // Hide backlight icon (backlight is off)
    view_lock_screen_render(&g_lock_screen, false);  // Calls gui_flush() async

    // Wait for display update to complete before entering deep sleep
    vTaskDelay(pdMS_TO_TICKS(3000));  // E-paper full refresh takes ~2-3s

    // Configure GPIO wakeup only (no timer)
    // Use EXT1 instead of EXT0 - less RTC GPIO issues on ESP32-S3
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_sleep_enable_ext1_wakeup_io(1ULL << EXP_IRQ_PIN, ESP_EXT1_WAKEUP_ANY_LOW);

    // Enter deep sleep (causes reset on wake)
    esp_deep_sleep_start();
}

// Helper: Dynamic brightness step size
// 0-10: step 1, 10-200: step 50, 200+: step 100
uint16_t brightness_step(uint16_t current, bool up) {
    if (up) {
        if (current < 10) return 1;
        if (current < 200) return 50;
        return 100;
    }
    if (current <= 10) return 1;
    if (current <= 200) return 50;
    return 100;
}
