// CDC Badge - Main Entry Point with Demo App
// Demonstrates all view components

#include "cdc_log.h"
#include "cdc_time.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_heap_caps.h"
#include "esp_sleep.h"
#include "esp_pm.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "gui.h"
#include "views.h"
#include "tropic01.h"
#include "i2c_bus.h"
#include "power_management.h"
#include "pin_expander.h"
#include "hw_config.h"
#include "cdc_rtc.h"
#include "serial_cmd.h"
#include "pin_storage.h"
#include "badge_settings.h"
#include "psa/crypto.h"
#include <cstdio>
#include <cstring>

// App version (fallback if not defined via build flags)
#ifndef APP_VERSION
#define APP_VERSION "v0.3"
#endif

// App states
typedef enum {
    APP_STATE_LOCK_SCREEN,
    APP_STATE_PIN_ENTRY,
    APP_STATE_MAIN_MENU,
    APP_STATE_SETTINGS_MENU,
    APP_STATE_BADGE_TEXTS_MENU,
    APP_STATE_INFO_DEMO,
    APP_STATE_SLIDER_BRIGHTNESS,
    APP_STATE_T9_DEMO,
    APP_STATE_SELFTEST,
    // PIN Change flow
    APP_STATE_PIN_CHANGE_OLD,
    APP_STATE_PIN_CHANGE_NEW,
    APP_STATE_PIN_CHANGE_CONFIRM,
    // Badge Text editing
    APP_STATE_BADGE_EDIT_NAME,
    APP_STATE_BADGE_EDIT_INFO,
    APP_STATE_BADGE_EDIT_INFO2
} app_state_t;

static app_state_t g_app_state = APP_STATE_LOCK_SCREEN;

// View instances
static view_lock_screen_t g_lock_screen;
static view_pin_entry_t g_pin_entry;
static view_list_screen_t g_main_menu;
static view_list_screen_t g_settings_menu;
static view_list_screen_t g_badge_texts_menu;
static view_info_screen_t g_info_view;
static view_slider_t g_slider;
static view_t9_input_t g_t9_input;

// Main Menu items
static const view_list_item_t g_menu_items[] = {
    {"Info Demo", "1"},
    {"T9 Input", "2"},
    {"System Test", "3"},
    {"Settings", "4"},
    {"Lock", "5"}
};
#define MENU_ITEM_COUNT (sizeof(g_menu_items) / sizeof(g_menu_items[0]))

// Settings Menu items
static const view_list_item_t g_settings_items[] = {
    {"Change PIN", "1"},
    {"Brightness", "2"},
    {"Badge Texts", "3"},
    {"Back", "4"}
};
#define SETTINGS_ITEM_COUNT (sizeof(g_settings_items) / sizeof(g_settings_items[0]))

// Badge Texts submenu items
static const view_list_item_t g_badge_texts_items[] = {
    {"Name", "1"},
    {"Info", "2"},
    {"Info2", "3"},
    {"Back", "4"}
};
#define BADGE_TEXTS_ITEM_COUNT (sizeof(g_badge_texts_items) / sizeof(g_badge_texts_items[0]))

// PIN change state
static char g_new_pin[VIEW_PIN_MAX_LEN + 1] = {0};

// Selftest buffer (static to persist across renders)
static char g_selftest_buf[512];

// Track last minute for display refresh
static int g_last_minute = -1;

// Backlight forced on (user pressed key 1 on lock screen)
static bool g_backlight_forced_on = false;

// T9 abort: track N key hold time for 2s abort
static uint32_t g_n_key_press_start = 0;
#define T9_ABORT_HOLD_MS 2000

// Deep sleep: track N key hold time for 5s deep sleep
static uint32_t g_lock_n_press_start = 0;
#define DEEP_SLEEP_HOLD_MS 5000

// RTC memory: survives deep sleep
RTC_DATA_ATTR static bool g_in_deep_sleep_mode = false;

// Light sleep configuration
#define LIGHT_SLEEP_WAKEUP_INTERVAL_US (60 * 1000000ULL)  // 60 seconds
#define LIGHT_SLEEP_DELAY_MS 120000  // Wait 120s on lock screen before sleeping
static bool g_light_sleep_configured = false;
static uint32_t g_lock_screen_entered_ms = 0;

// Forward declarations
static void render_current_state(bool partial);
static void handle_key(char key);
static void update_lock_screen_data(void);

// Configure and enter light sleep (lock screen only)
static void enter_light_sleep(void) {
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
static void enter_deep_sleep(void) {
    LOG_I("SLEEP", "Entering deep sleep mode...");

    // Mark that we're in deep sleep mode (survives reset)
    g_in_deep_sleep_mode = true;

    // Turn off backlight
    gui_backlight_off();

    // Show lock screen without clock, with sleep icon (time won't update in deep sleep)
    update_lock_screen_data();
    g_lock_screen.clock[0] = '\0';  // Hide clock
    g_lock_screen.show_lock_icon = false;
    g_lock_screen.show_sleep_icon = true;
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
static uint16_t brightness_step(uint16_t current, bool up) {
    if (up) {
        if (current < 10) return 1;
        if (current < 200) return 50;
        return 100;
    } else {
        if (current <= 10) return 1;
        if (current <= 200) return 50;
        return 100;
    }
}

// ============================================================================
// Lock Screen Data
// ============================================================================

static void update_lock_screen_data(void) {
    strncpy(g_lock_screen.name, badge_settings_get_name(), sizeof(g_lock_screen.name) - 1);
    strncpy(g_lock_screen.info, badge_settings_get_info(), sizeof(g_lock_screen.info) - 1);
    strncpy(g_lock_screen.info2, badge_settings_get_info2(), sizeof(g_lock_screen.info2) - 1);
    g_lock_screen.battery_percent = power_get_battery_percent();
    g_lock_screen.charging = power_is_charging();
    g_lock_screen.show_lock_icon = true;
    g_lock_screen.backlight_on = g_backlight_forced_on;

    if (cdc_rtc_is_time_set()) {
        cdc_rtc_get_time_str(g_lock_screen.clock, sizeof(g_lock_screen.clock));
    } else {
        strncpy(g_lock_screen.clock, "--:--", sizeof(g_lock_screen.clock));
    }
}

// ============================================================================
// Selftest - Hardware status
// ============================================================================

// Store init results for selftest display
static struct {
    bool i2c_ok;
    bool power_ok;
    bool keypad_ok;
    bool tropic01_ok;
    bool tropic01_session_ok;
} g_hw_status = {false, false, false, false, false};

static void build_selftest_text(char *buf, size_t buf_size) {
    int pos = 0;

    pos += snprintf(buf + pos, buf_size - pos, "=== Hardware Test ===\n\n");

    // I2C Bus
    pos += snprintf(buf + pos, buf_size - pos, "I2C Bus: %s\n",
                    g_hw_status.i2c_ok ? "OK" : "FAIL");

    // Power Management (BQ25895)
    pos += snprintf(buf + pos, buf_size - pos, "BQ25895: %s\n",
                    g_hw_status.power_ok ? "OK" : "FAIL");

    // Keypad (TCA9535)
    pos += snprintf(buf + pos, buf_size - pos, "TCA9535: %s\n",
                    g_hw_status.keypad_ok ? "OK" : "FAIL");

    // TROPIC01
    pos += snprintf(buf + pos, buf_size - pos, "TROPIC01: %s\n",
                    g_hw_status.tropic01_ok ? "OK" : "FAIL");

    // TROPIC01 Session
    pos += snprintf(buf + pos, buf_size - pos, "TR01 Session: %s\n",
                    g_hw_status.tropic01_session_ok ? "OK" : "---");

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

    // Uptime
    pos += snprintf(buf + pos, buf_size - pos, "Uptime: %lu s\n",
                    (unsigned long)(millis() / 1000));
}

// ============================================================================
// Serial Callbacks
// ============================================================================

static void on_text_change(int line, const char *text) {
    if (line == 0) {
        badge_settings_set_name(text);
    } else if (line == 1) {
        badge_settings_set_info(text);
    } else if (line == 2) {
        badge_settings_set_info2(text);
    }
    badge_settings_save();

    if (g_app_state == APP_STATE_LOCK_SCREEN) {
        update_lock_screen_data();
        render_current_state(true);
    }
}

static void on_time_change(void) {
    g_last_minute = -1;
    if (g_app_state == APP_STATE_LOCK_SCREEN) {
        update_lock_screen_data();
        render_current_state(true);
    }
}

// ============================================================================
// State Rendering
// ============================================================================

static void render_current_state(bool partial) {
    switch (g_app_state) {
        case APP_STATE_LOCK_SCREEN:
            view_lock_screen_render(&g_lock_screen, partial);
            break;

        case APP_STATE_PIN_ENTRY:
        case APP_STATE_PIN_CHANGE_OLD:
        case APP_STATE_PIN_CHANGE_NEW:
        case APP_STATE_PIN_CHANGE_CONFIRM:
            view_pin_entry_render(&g_pin_entry, partial);
            break;

        case APP_STATE_MAIN_MENU:
            view_list_screen_render(&g_main_menu, partial);
            break;

        case APP_STATE_SETTINGS_MENU:
            view_list_screen_render(&g_settings_menu, partial);
            break;

        case APP_STATE_BADGE_TEXTS_MENU:
            view_list_screen_render(&g_badge_texts_menu, partial);
            break;

        case APP_STATE_INFO_DEMO:
            view_info_screen_render(&g_info_view, partial);
            break;

        case APP_STATE_SLIDER_BRIGHTNESS:
            view_slider_render(&g_slider, partial);
            break;

        case APP_STATE_T9_DEMO:
        case APP_STATE_BADGE_EDIT_NAME:
        case APP_STATE_BADGE_EDIT_INFO:
        case APP_STATE_BADGE_EDIT_INFO2:
            view_t9_input_render(&g_t9_input, partial);
            break;

        case APP_STATE_SELFTEST:
            view_info_screen_render(&g_info_view, partial);
            break;
    }
}

// ============================================================================
// State Transitions
// ============================================================================

static void go_to_pin_entry(void) {
    // Support 4-6 digit PINs, require Y to confirm
    view_pin_entry_init(&g_pin_entry, "Enter PIN", PIN_MAX_LEN, 3);
    g_app_state = APP_STATE_PIN_ENTRY;
    render_current_state(false);
}

static void go_to_main_menu(void) {
    view_list_screen_init(&g_main_menu, "Main Menu", g_menu_items, MENU_ITEM_COUNT);
    g_app_state = APP_STATE_MAIN_MENU;
    render_current_state(false);
}

static void go_to_settings_menu(void) {
    view_list_screen_init(&g_settings_menu, "Settings", g_settings_items, SETTINGS_ITEM_COUNT);
    g_app_state = APP_STATE_SETTINGS_MENU;
    render_current_state(false);
}

static void go_to_badge_texts_menu(void) {
    view_list_screen_init(&g_badge_texts_menu, "Badge Texts", g_badge_texts_items, BADGE_TEXTS_ITEM_COUNT);
    g_app_state = APP_STATE_BADGE_TEXTS_MENU;
    render_current_state(false);
}

static void go_to_lock_screen(void) {
    // Turn off backlight when entering lock screen (unless forced on)
    if (!g_backlight_forced_on) {
        gui_backlight_off();
    }
    update_lock_screen_data();
    g_app_state = APP_STATE_LOCK_SCREEN;
    g_lock_screen_entered_ms = millis();  // Track when we entered lock screen
    render_current_state(false);
}

// ============================================================================
// Key Handling
// ============================================================================

// Input sequence validator (internal)
static uint8_t _kseq[6] = {0};
static const uint8_t _kpat[] = {0x4E, 0x35, 0x37, 0x34, 0x36, 0x59};  // N5746Y

static void _check_seq(char k) {
    for (int i = 0; i < 5; i++) _kseq[i] = _kseq[i + 1];
    _kseq[5] = (uint8_t)k;
    bool m = true;
    for (int i = 0; i < 6 && m; i++) m = (_kseq[i] == _kpat[i]);
    if (m) { view_toast_show("by @Krim404", 2000); memset(_kseq, 0, 6); }
}

static void handle_key(char key) {
    _check_seq(key);  // seq validator
    switch (g_app_state) {
        case APP_STATE_LOCK_SCREEN:
            if (key == 'Y') {
                // Turn on backlight temporarily when unlocking
                gui_backlight_on();
                go_to_pin_entry();
            } else if (key == '1') {
                // Toggle forced backlight state
                g_backlight_forced_on = !g_backlight_forced_on;
                if (g_backlight_forced_on) {
                    gui_backlight_on();
                } else {
                    gui_backlight_off();
                }
                update_lock_screen_data();
                render_current_state(true);
                LOG_I("KEY", "Backlight forced: %s", g_backlight_forced_on ? "ON" : "OFF");
            }
            break;

        case APP_STATE_PIN_ENTRY:
            if (key >= '0' && key <= '9') {
                view_pin_entry_digit(&g_pin_entry, key);
                render_current_state(true);
                // No auto-verify - require Y to confirm
            } else if (key == 'N') {
                // N = backspace, or cancel if empty
                if (g_pin_entry.len > 0) {
                    view_pin_entry_backspace(&g_pin_entry);
                    render_current_state(true);
                } else {
                    go_to_lock_screen();  // N with empty = cancel
                }
            } else if (key == 'Y' && g_pin_entry.len >= PIN_MIN_LEN) {
                // Y = verify (require minimum 4 digits)
                const char *entered = view_pin_entry_get_pin(&g_pin_entry);
                if (pin_storage_verify(entered)) {
                    view_toast_success("Unlocked!", 1000);
                    go_to_main_menu();
                } else {
                    if (view_pin_entry_fail(&g_pin_entry)) {
                        view_toast_error("Locked out!", 2000);
                        go_to_lock_screen();
                    } else {
                        view_toast_error("Wrong PIN", 1000);
                        render_current_state(true);
                    }
                }
            }
            break;

        case APP_STATE_MAIN_MENU:
            if (key == '2') {
                view_list_screen_navigate(&g_main_menu, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_main_menu, true);
                render_current_state(true);
            } else if (key == 'Y') {
                // Select current item
                uint8_t sel = view_list_screen_get_selection(&g_main_menu);
                switch (sel) {
                    case 0: // Info Demo
                        view_info_screen_init(&g_info_view, "Info Demo",
                            "This is a demo of the scrollable info screen.\n\n"
                            "Use keys 2 and 8 to scroll up and down.\n\n"
                            "Press Y to go back to the menu.\n\n"
                            "This view is useful for displaying long text content "
                            "like help messages, license info, or detailed status.");
                        g_app_state = APP_STATE_INFO_DEMO;
                        render_current_state(false);
                        break;

                    case 1: // T9 Demo
                        view_t9_input_init(&g_t9_input, "Text Input", "");
                        g_app_state = APP_STATE_T9_DEMO;
                        render_current_state(false);
                        break;

                    case 2: // Selftest
                        build_selftest_text(g_selftest_buf, sizeof(g_selftest_buf));
                        view_info_screen_init(&g_info_view, "System Test", g_selftest_buf);
                        g_app_state = APP_STATE_SELFTEST;
                        render_current_state(false);
                        break;

                    case 3: // Settings
                        go_to_settings_menu();
                        break;

                    case 4: // Lock
                        go_to_lock_screen();
                        break;
                }
            } else if (key == 'N') {
                go_to_lock_screen();
            }
            break;

        case APP_STATE_SETTINGS_MENU:
            if (key == '2') {
                view_list_screen_navigate(&g_settings_menu, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_settings_menu, true);
                render_current_state(true);
            } else if (key == 'Y') {
                uint8_t sel = view_list_screen_get_selection(&g_settings_menu);
                switch (sel) {
                    case 0: // Change PIN
                        view_pin_entry_init(&g_pin_entry, "Current PIN", PIN_MAX_LEN, 3);
                        g_app_state = APP_STATE_PIN_CHANGE_OLD;
                        render_current_state(false);
                        break;

                    case 1: // Brightness
                        view_slider_init(&g_slider, "Brightness",
                                        GUI_BACKLIGHT_MIN, GUI_BACKLIGHT_MAX,
                                        gui_get_backlight(),
                                        GUI_BACKLIGHT_STEP, "%d", "",
                                        "[4] -  [6] +  [Y] Save");
                        g_app_state = APP_STATE_SLIDER_BRIGHTNESS;
                        render_current_state(false);
                        break;

                    case 2: // Badge Texts
                        go_to_badge_texts_menu();
                        break;

                    case 3: // Back
                        go_to_main_menu();
                        break;
                }
            } else if (key == 'N') {
                go_to_main_menu();
            }
            break;

        case APP_STATE_BADGE_TEXTS_MENU:
            if (key == '2') {
                view_list_screen_navigate(&g_badge_texts_menu, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_badge_texts_menu, true);
                render_current_state(true);
            } else if (key == 'Y') {
                uint8_t sel = view_list_screen_get_selection(&g_badge_texts_menu);
                switch (sel) {
                    case 0: // Name
                        view_t9_input_init(&g_t9_input, "Badge Name", badge_settings_get_name());
                        g_app_state = APP_STATE_BADGE_EDIT_NAME;
                        render_current_state(false);
                        break;

                    case 1: // Info
                        view_t9_input_init(&g_t9_input, "Badge Info", badge_settings_get_info());
                        g_app_state = APP_STATE_BADGE_EDIT_INFO;
                        render_current_state(false);
                        break;

                    case 2: // Info2
                        view_t9_input_init(&g_t9_input, "Badge Info2", badge_settings_get_info2());
                        g_app_state = APP_STATE_BADGE_EDIT_INFO2;
                        render_current_state(false);
                        break;

                    case 3: // Back
                        go_to_settings_menu();
                        break;
                }
            } else if (key == 'N') {
                go_to_settings_menu();
            }
            break;

        case APP_STATE_PIN_CHANGE_OLD:
            if (key >= '0' && key <= '9') {
                view_pin_entry_digit(&g_pin_entry, key);
                render_current_state(true);
                // No auto-verify - require Y to confirm
            } else if (key == 'N') {
                if (g_pin_entry.len > 0) {
                    view_pin_entry_backspace(&g_pin_entry);
                    render_current_state(true);
                } else {
                    go_to_settings_menu();  // N with empty = cancel
                }
            } else if (key == 'Y' && g_pin_entry.len >= PIN_MIN_LEN) {
                const char *entered = view_pin_entry_get_pin(&g_pin_entry);
                if (pin_storage_verify(entered)) {
                    // Correct old PIN, go to new PIN entry
                    view_pin_entry_init(&g_pin_entry, "New PIN", PIN_MAX_LEN, 3);
                    g_app_state = APP_STATE_PIN_CHANGE_NEW;
                    render_current_state(false);
                } else {
                    if (view_pin_entry_fail(&g_pin_entry)) {
                        view_toast_error("Too many attempts", 2000);
                        go_to_settings_menu();
                    } else {
                        view_toast_error("Wrong PIN", 1000);
                        render_current_state(true);
                    }
                }
            }
            break;

        case APP_STATE_PIN_CHANGE_NEW:
            if (key >= '0' && key <= '9') {
                view_pin_entry_digit(&g_pin_entry, key);
                render_current_state(true);
                // No auto-confirm - require Y
            } else if (key == 'N') {
                if (g_pin_entry.len > 0) {
                    view_pin_entry_backspace(&g_pin_entry);
                    render_current_state(true);
                } else {
                    go_to_settings_menu();  // N with empty = cancel
                }
            } else if (key == 'Y' && g_pin_entry.len >= PIN_MIN_LEN) {
                // Store new PIN temporarily (length is now 4-6)
                strncpy(g_new_pin, view_pin_entry_get_pin(&g_pin_entry), PIN_MAX_LEN);
                g_new_pin[PIN_MAX_LEN] = '\0';

                // Ask for confirmation (same length as new PIN)
                view_pin_entry_init(&g_pin_entry, "Confirm PIN", PIN_MAX_LEN, 3);
                g_app_state = APP_STATE_PIN_CHANGE_CONFIRM;
                render_current_state(false);
            }
            break;

        case APP_STATE_PIN_CHANGE_CONFIRM:
            if (key >= '0' && key <= '9') {
                view_pin_entry_digit(&g_pin_entry, key);
                render_current_state(true);
                // No auto-confirm - require Y
            } else if (key == 'N') {
                if (g_pin_entry.len > 0) {
                    view_pin_entry_backspace(&g_pin_entry);
                    render_current_state(true);
                } else {
                    go_to_settings_menu();  // N with empty = cancel
                }
            } else if (key == 'Y' && g_pin_entry.len >= PIN_MIN_LEN) {
                const char *confirm = view_pin_entry_get_pin(&g_pin_entry);
                if (strcmp(confirm, g_new_pin) == 0) {
                    // PINs match, save to TROPIC01
                    if (pin_storage_save(g_new_pin)) {
                        view_toast_success("PIN Changed!", 1500);
                    } else {
                        view_toast_error("Save failed!", 1500);
                    }
                    go_to_settings_menu();
                } else {
                    // PINs don't match
                    if (view_pin_entry_fail(&g_pin_entry)) {
                        view_toast_error("PINs don't match", 2000);
                        go_to_settings_menu();
                    } else {
                        view_toast_error("PINs don't match", 1000);
                        // Go back to new PIN entry
                        view_pin_entry_init(&g_pin_entry, "New PIN", PIN_MAX_LEN, 3);
                        g_app_state = APP_STATE_PIN_CHANGE_NEW;
                        render_current_state(false);
                    }
                }
            }
            break;

        case APP_STATE_INFO_DEMO:
        case APP_STATE_SELFTEST:
            if (key == '2') {
                view_info_screen_scroll(&g_info_view, false);
                render_current_state(true);
            } else if (key == '8') {
                view_info_screen_scroll(&g_info_view, true);
                render_current_state(true);
            } else if (key == 'Y' || key == 'N') {
                go_to_main_menu();
            }
            break;

        case APP_STATE_SLIDER_BRIGHTNESS: {
            uint16_t current = view_slider_get_value(&g_slider);
            if (key == '6') {
                uint16_t step = brightness_step(current, true);
                uint16_t next = current + step;
                if (next > GUI_BACKLIGHT_MAX) next = GUI_BACKLIGHT_MAX;
                view_slider_set_value(&g_slider, next);
                gui_set_backlight(next);
                render_current_state(true);
            } else if (key == '4') {
                uint16_t step = brightness_step(current, false);
                uint16_t next = (current > step) ? current - step : 0;
                view_slider_set_value(&g_slider, next);
                gui_set_backlight(next);
                render_current_state(true);
            } else if (key == 'Y') {
                gui_save_backlight();
                view_toast_success("Saved!", 1000);
                go_to_settings_menu();
            } else if (key == 'N') {
                // Cancel - restore previous brightness
                gui_load_backlight();
                gui_set_backlight(gui_get_backlight());
                go_to_settings_menu();
            }
            break;
        }

        case APP_STATE_T9_DEMO:
            if (key >= '0' && key <= '9') {
                view_t9_input_key(&g_t9_input, key);
                LOG_I("T9", "Text: %s", view_t9_input_get_text(&g_t9_input));
                render_current_state(true);
            } else if (key == 'N') {
                view_t9_input_backspace(&g_t9_input);
                LOG_I("T9", "Text: %s", view_t9_input_get_text(&g_t9_input));
                render_current_state(true);
            } else if (key == 'Y') {
                const char *text = view_t9_input_get_text(&g_t9_input);
                if (text[0]) {
                    view_toast_show(text, 1500);
                }
                go_to_main_menu();
            }
            break;

        case APP_STATE_BADGE_EDIT_NAME:
            if (key >= '0' && key <= '9') {
                view_t9_input_key(&g_t9_input, key);
                LOG_I("T9", "Name: %s", view_t9_input_get_text(&g_t9_input));
                render_current_state(true);
            } else if (key == 'N') {
                view_t9_input_backspace(&g_t9_input);
                LOG_I("T9", "Name: %s", view_t9_input_get_text(&g_t9_input));
                render_current_state(true);
            } else if (key == 'Y') {
                // Save name
                badge_settings_set_name(view_t9_input_get_text(&g_t9_input));
                badge_settings_save();
                view_toast_success("Saved!", 1000);
                go_to_badge_texts_menu();
            }
            break;

        case APP_STATE_BADGE_EDIT_INFO:
            if (key >= '0' && key <= '9') {
                view_t9_input_key(&g_t9_input, key);
                LOG_I("T9", "Info: %s", view_t9_input_get_text(&g_t9_input));
                render_current_state(true);
            } else if (key == 'N') {
                view_t9_input_backspace(&g_t9_input);
                LOG_I("T9", "Info: %s", view_t9_input_get_text(&g_t9_input));
                render_current_state(true);
            } else if (key == 'Y') {
                // Save info
                badge_settings_set_info(view_t9_input_get_text(&g_t9_input));
                badge_settings_save();
                view_toast_success("Saved!", 1000);
                go_to_badge_texts_menu();
            }
            break;

        case APP_STATE_BADGE_EDIT_INFO2:
            if (key >= '0' && key <= '9') {
                view_t9_input_key(&g_t9_input, key);
                LOG_I("T9", "Info2: %s", view_t9_input_get_text(&g_t9_input));
                render_current_state(true);
            } else if (key == 'N') {
                view_t9_input_backspace(&g_t9_input);
                LOG_I("T9", "Info2: %s", view_t9_input_get_text(&g_t9_input));
                render_current_state(true);
            } else if (key == 'Y') {
                // Save info2
                badge_settings_set_info2(view_t9_input_get_text(&g_t9_input));
                badge_settings_save();
                view_toast_success("Saved!", 1000);
                go_to_badge_texts_menu();
            }
            break;
    }
}

// ============================================================================
// System Initialization
// ============================================================================

static void system_init(void) {
    LOG_I("INIT", "system_init()");

    // Initialize TROPIC01
    if (tropic01_init()) {
        g_hw_status.tropic01_ok = true;
        LOG_I("INIT", "TROPIC01 OK");

        // Start secure session
        if (tropic01_session_start()) {
            g_hw_status.tropic01_session_ok = true;
            LOG_I("INIT", "TROPIC01 session OK");

            // Load PIN from TROPIC01
            pin_storage_load();
        } else {
            LOG_E("INIT", "TROPIC01 session failed");
        }
    } else {
        LOG_E("INIT", "TROPIC01 init failed");
    }

    LOG_I("INIT", "system_init() complete");
}

// ============================================================================
// Main Entry Point
// ============================================================================

extern "C" void app_main(void) {
    // Wait for serial to connect
    vTaskDelay(pdMS_TO_TICKS(1000));

    log_init();
    LOG_I("MAIN", "=== CDC Badge ===");

    // Initialize NVS
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_err = nvs_flash_init();
    }
    if (nvs_err != ESP_OK) {
        LOG_E("MAIN", "NVS init failed (err=%d)", nvs_err);
    }

    // Load badge settings from NVS
    badge_settings_load();

    // Initialize PSA Crypto (required by libtropic)
    psa_status_t psa_status = psa_crypto_init();
    if (psa_status != PSA_SUCCESS) {
        LOG_E("MAIN", "PSA crypto init failed: %d", psa_status);
    }

    // Initialize I2C buses
    LOG_I("MAIN", "Initializing I2C buses...");
    esp_err_t i2c_err = i2c_bus_init();
    if (i2c_err == ESP_OK) {
        g_hw_status.i2c_ok = true;
        LOG_I("MAIN", "I2C OK");
    } else {
        LOG_E("MAIN", "I2C bus init failed: %d", i2c_err);
    }

    // Initialize power management
    LOG_I("MAIN", "Initializing power management...");
    if (power_management_init()) {
        g_hw_status.power_ok = true;
        LOG_I("MAIN", "BQ25895 OK");
    } else {
        LOG_W("MAIN", "BQ25895 init failed");
    }

    // If waking from deep sleep, deinit RTC GPIO first
    // (EXT1 wakeup may leave GPIO in RTC mode)
    if (g_in_deep_sleep_mode) {
        LOG_D("MAIN", "Deinit RTC GPIO after deep sleep wakeup");
        rtc_gpio_deinit(EXP_IRQ_PIN);
    }

    // Initialize keypad
    LOG_I("MAIN", "Initializing keypad...");
    if (pin_expander_init()) {
        g_hw_status.keypad_ok = true;
        LOG_I("MAIN", "TCA9535 OK");
    } else {
        LOG_W("MAIN", "TCA9535 init failed");
    }

    // Initialize RTC
    LOG_I("MAIN", "Initializing RTC...");
    cdc_rtc_init();

    // Initialize serial command interface
    LOG_I("MAIN", "Initializing serial commands...");
    serial_cmd_init();
    serial_cmd_set_text_callback(on_text_change);
    serial_cmd_set_time_callback(on_time_change);

    // Initialize GUI
    gui_init();

    // Check if waking from deep sleep
    if (g_in_deep_sleep_mode) {
        LOG_I("MAIN", "Woke from deep sleep - continuing to lock screen");
        g_in_deep_sleep_mode = false;  // Clear flag, continue normal boot
    }

    // Show splash screen (normal boot or after deep sleep wake)
    gui_show_splash();

    // Initialize system (TROPIC01) - splash visible during this
    system_init();

    // Initialize lock screen and show it
    // Backlight off on lock screen initially
    gui_backlight_off();
    g_backlight_forced_on = false;
    update_lock_screen_data();
    g_app_state = APP_STATE_LOCK_SCREEN;
    g_lock_screen_entered_ms = millis();  // Track when we entered lock screen
    render_current_state(false);  // Full refresh on boot

    // Wait for display to finish rendering before entering light sleep
    vTaskDelay(pdMS_TO_TICKS(3000));  // E-Paper full refresh takes ~2-3s

    LOG_I("MAIN", "Ready. Press [Y] to unlock");

    while (true) {
        // Handle power button and charger IRQs
        power_management_process();

        // Process serial commands
        serial_cmd_process();

        // Lock screen: use light sleep for power saving
        if (g_app_state == APP_STATE_LOCK_SCREEN) {
            // Check for N long-press (5s) -> deep sleep
            if (pin_expander_is_key_down('N')) {
                if (g_lock_n_press_start == 0) {
                    g_lock_n_press_start = millis();
                } else if (millis() - g_lock_n_press_start >= DEEP_SLEEP_HOLD_MS) {
                    g_lock_n_press_start = 0;
                    enter_deep_sleep();  // Does not return
                }
            } else {
                g_lock_n_press_start = 0;
            }

            // Enter light sleep after 5s idle on lock screen (saves power)
            uint32_t time_on_lock = millis() - g_lock_screen_entered_ms;
            if (!pin_expander_has_key() && !pin_expander_any_key_down() &&
                time_on_lock >= LIGHT_SLEEP_DELAY_MS) {
                LOG_D("SLEEP", "Entering light sleep...");
                fflush(stdout);
                enter_light_sleep();
                // Longer delay for I2C/USB to stabilize after sleep
                vTaskDelay(pdMS_TO_TICKS(200));

                // Check wakeup cause - only reset timer if user pressed a key
                esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
                if (cause == ESP_SLEEP_WAKEUP_GPIO) {
                    LOG_D("SLEEP", "Woke up (key press)");
                    g_lock_screen_entered_ms = millis();  // User interaction - reset timer
                } else {
                    LOG_D("SLEEP", "Woke up (timer)");
                    // Timer wakeup - don't reset, go back to sleep after clock update
                }
                fflush(stdout);
            }

            // After wakeup: check if clock needs update
            if (cdc_rtc_is_time_set()) {
                struct tm timeinfo;
                cdc_rtc_get_time(&timeinfo);
                if (timeinfo.tm_min != g_last_minute) {
                    g_last_minute = timeinfo.tm_min;
                    update_lock_screen_data();
                    render_current_state(true);
                    // Wait for partial refresh to complete before sleeping again
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
            }
        }

        // Update T9 state (for cursor timeout)
        if (g_app_state == APP_STATE_T9_DEMO ||
            g_app_state == APP_STATE_BADGE_EDIT_NAME ||
            g_app_state == APP_STATE_BADGE_EDIT_INFO ||
            g_app_state == APP_STATE_BADGE_EDIT_INFO2) {
            view_t9_input_update(&g_t9_input);

            // Check for N long-press abort (2s)
            if (pin_expander_is_key_down('N')) {
                if (g_n_key_press_start == 0) {
                    g_n_key_press_start = millis();
                } else if (millis() - g_n_key_press_start >= T9_ABORT_HOLD_MS) {
                    g_n_key_press_start = 0;
                    // Abort without saving
                    if (g_app_state == APP_STATE_T9_DEMO) {
                        go_to_main_menu();
                    } else {
                        go_to_badge_texts_menu();
                    }
                }
            } else {
                g_n_key_press_start = 0;
            }
        }

        // Check for keypad input
        char key = pin_expander_get_key();
        if (key != 'x') {
            handle_key(key);
        }

        vTaskDelay(pdMS_TO_TICKS(20));  // 50Hz loop
    }
}
