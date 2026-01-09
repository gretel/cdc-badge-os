#include "app_input.h"

#include "app_fido.h"
#include "app_globals.h"
#include "app_menus.h"
#include "app_power.h"
#include "app_render.h"
#include "app_state.h"
#include "app_status.h"
#include "app_totp.h"

#include "badge_settings.h"
#include "cdc_log.h"
#include "cdc_rtc.h"
#include "cdc_time.h"
#include "gui.h"
#include "i18n.h"
#include "ntp_sync.h"
#include "pin_storage.h"
#include "power_management.h"

#if FEATURE_TOTP
#include "totp_store.h"
#endif
#if FEATURE_FIDO2
#include "fido2.h"
#endif

#if FEATURE_BLE_UART
#include "ble_uart.h"
#endif

#if FEATURE_CA
#include "ca.h"
#endif

#include <cstdio>
#include <cstring>

#if FEATURE_BLE_UART
static bool ble_is_active(void) {
    return g_ble_enabled || ble_uart_is_initialized();
}
#endif

static uint32_t ip_octets_to_u32(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return ((uint32_t)d << 24) | ((uint32_t)c << 16) | ((uint32_t)b << 8) | (uint32_t)a;
}

static void ip_input_to_string(const view_ip_input_t *view, char *out, size_t out_len) {
    if (!view || !out || out_len == 0) return;
    snprintf(out, out_len, "%u.%u.%u.%u",
             view->octet[0], view->octet[1], view->octet[2], view->octet[3]);
}

static bool ip_string_to_u32(const char *ip_str, uint32_t *out) {
    if (!ip_str || !out) return false;
    int a = 0;
    int b = 0;
    int c = 0;
    int d = 0;
    if (sscanf(ip_str, "%d.%d.%d.%d", &a, &b, &c, &d) != 4) {
        return false;
    }
    if (a < 0 || a > 255 || b < 0 || b > 255 || c < 0 || c > 255 || d < 0 || d > 255) {
        return false;
    }
    *out = ip_octets_to_u32((uint8_t)a, (uint8_t)b, (uint8_t)c, (uint8_t)d);
    return true;
}

static bool ip_string_empty(const char *ip_str) {
    return !ip_str || ip_str[0] == '\0';
}

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

void handle_key(char key) {
    _check_seq(key);  // seq validator

    // Reset autolock timer on any keypress (except on lock screen)
    if (g_app_state != APP_STATE_LOCK_SCREEN && g_app_state != APP_STATE_LOCKOUT) {
        g_last_activity_ms = millis();
    }

    switch (g_app_state) {
        case APP_STATE_LOCK_SCREEN:
            if (key == 'Y') {
                // Turn on backlight temporarily when unlocking
                gui_backlight_on();
                go_to_pin_entry();
            } else if (key == '3') {
                // Open quick menu (limited - WiFi/BLE moved to main menu)
                g_lock_quick_items[0] = {"Light", 1};
                g_lock_quick_items[1] = {"Sleep", 2};
                view_context_menu_init(&g_lock_quick_menu, "Quick Menu", g_lock_quick_items, 2);
                view_context_menu_show(&g_lock_quick_menu);
                g_app_state = APP_STATE_LOCK_QUICK_MENU;
            }
            break;

        case APP_STATE_LOCK_QUICK_MENU:
            if (key == '2') {
                view_context_menu_navigate(&g_lock_quick_menu, false);
                view_context_menu_render(&g_lock_quick_menu);
            } else if (key == '8') {
                view_context_menu_navigate(&g_lock_quick_menu, true);
                view_context_menu_render(&g_lock_quick_menu);
            } else if (key == 'Y') {
                uint8_t action = view_context_menu_get_action(&g_lock_quick_menu);
                view_context_menu_hide(&g_lock_quick_menu);
                switch (action) {
                    case 1:  // Light
                        g_backlight_forced_on = !g_backlight_forced_on;
                        if (g_backlight_forced_on) {
                            gui_backlight_on();
                        } else {
                            gui_backlight_off();
                        }
                        LOG_I("KEY", "Backlight forced: %s", g_backlight_forced_on ? "ON" : "OFF");
                        break;
                    case 2:  // Deep Sleep
                        if (power_is_usb_connected()) {
                            view_toast_error(i18n_str(STR_UNPLUG_USB), 1500);
                        } else {
                            enter_deep_sleep();  // Does not return
                        }
                        break;
                    default:  // Cancel (0)
                        break;
                }
                update_lock_screen_data();
                g_app_state = APP_STATE_LOCK_SCREEN;
                render_current_state(false);
            } else if (key == 'N') {
                view_context_menu_hide(&g_lock_quick_menu);
                g_app_state = APP_STATE_LOCK_SCREEN;
                render_current_state(false);
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
                    view_toast_success(i18n_str(STR_UNLOCKED), 1000);
                    go_to_main_menu();
                } else {
                    if (view_pin_entry_fail(&g_pin_entry)) {
                        // All attempts exhausted
#if DEBUG_MODE
                        // Debug mode: just show message and return to lock screen
                        view_toast_error(i18n_str(STR_LOCKED_OUT), 2000);
                        go_to_lock_screen();
#else
                        // Production mode: 1-minute lockout
                        go_to_lockout();
#endif
                    } else {
                        view_toast_error(i18n_str(STR_WRONG_PIN), 1000);
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
            } else if (key == 'Y' || key == '5') {
                // Select current item (using enum for dynamic indices)
                uint8_t sel = view_list_screen_get_selection(&g_main_menu);
#if FEATURE_TOTP
                if (sel == MENU_IDX_TOTP) {
                    go_to_totp_list();
                } else
#endif
#if FEATURE_FIDO2
                if (sel == MENU_IDX_FIDO2) {
                    go_to_fido_list();
                } else
#endif
#if FEATURE_CA
                if (sel == MENU_IDX_CA) {
                    build_ca_menu();
                    view_list_screen_init(&g_ca_menu, i18n_str(STR_CA_MENU), g_ca_items, CA_IDX_COUNT);
                    g_app_state = APP_STATE_CA_MENU;
                    render_current_state(false);
                } else
#endif
                if (sel == MENU_IDX_TOOLS) {
                    build_tools_menu();
                    view_list_screen_init(&g_tools_menu, i18n_str(STR_TOOLS), g_tools_items, 2);
                    g_app_state = APP_STATE_TOOLS_MENU;
                    render_current_state(false);
                } else if (sel == MENU_IDX_SELFTEST) {
                    build_selftest_text(g_selftest_buf, sizeof(g_selftest_buf));
                    view_info_screen_init(&g_info_view, "System Test", g_selftest_buf);
                    g_app_state = APP_STATE_SELFTEST;
                    render_current_state(false);
                } else if (sel == MENU_IDX_SETTINGS) {
                    go_to_settings_menu();
                }
                // Deep Sleep moved to lock screen quick menu (key 3)
            } else if (key == '3') {
                // Open quick menu for WiFi/BLE toggle
                g_main_quick_items[0] = {"Light", 1};
                g_main_quick_items[1] = {"Bluetooth", 2};
                g_main_quick_items[2] = {"WiFi", 3};
                g_main_quick_items[3] = {"Sleep", 4};
                view_context_menu_init(&g_main_quick_menu, "Quick Menu", g_main_quick_items, 4);
                view_context_menu_show(&g_main_quick_menu);
                g_app_state = APP_STATE_MAIN_QUICK_MENU;
            } else if (key == 'N') {
                go_to_lock_screen();
            }
            break;

        case APP_STATE_MAIN_QUICK_MENU:
            if (key == '2') {
                view_context_menu_navigate(&g_main_quick_menu, false);
                view_context_menu_render(&g_main_quick_menu);
            } else if (key == '8') {
                view_context_menu_navigate(&g_main_quick_menu, true);
                view_context_menu_render(&g_main_quick_menu);
            } else if (key == 'Y') {
                uint8_t action = view_context_menu_get_action(&g_main_quick_menu);
                view_context_menu_hide(&g_main_quick_menu);
                switch (action) {
                    case 1:  // Light
                        g_backlight_forced_on = !g_backlight_forced_on;
                        if (g_backlight_forced_on) {
                            gui_backlight_on();
                            view_toast_show("Light ON", 800);
                        } else {
                            gui_backlight_off();
                            view_toast_show("Light OFF", 800);
                        }
                        LOG_I("KEY", "Backlight forced: %s", g_backlight_forced_on ? "ON" : "OFF");
                        break;
                    case 2:  // Bluetooth
#if FEATURE_BLE_UART
                        if (!g_ble_enabled) {
                            if (wifi_manager_is_init()) {
                                view_toast_error(i18n_str(STR_BLUETOOTH_DISABLE_WIFI), 1500);
                                break;
                            }
                            if (!ble_uart_is_initialized()) {
                                ble_uart_init();
                            }
                            ble_uart_set_power_mode(BLE_POWER_ACTIVE);
                            g_ble_enabled = true;
                            view_toast_show(i18n_str(STR_BLUETOOTH_ON), 1000);
                        } else {
                            ble_uart_set_power_mode(BLE_POWER_OFF);
                            ble_uart_deinit();
                            g_ble_enabled = false;
                            view_toast_show(i18n_str(STR_BLUETOOTH_OFF), 1000);
                        }
                        LOG_I("KEY", "BLE: %s", g_ble_enabled ? "ON" : "OFF");
#else
                        view_toast_error("BLE disabled", 1000);
#endif
                        break;
                    case 3:  // WiFi
                        if (wifi_manager_get_state() == WIFI_STATE_CONNECTED) {
                            wifi_manager_disconnect();
                            wifi_manager_deinit();
                            view_toast_show(i18n_str(STR_WIFI_DISCONNECTED), 1000);
                        } else if (wifi_manager_has_config()) {
#if FEATURE_BLE_UART
                            if (ble_is_active()) {
                                view_toast_error(i18n_str(STR_WIFI_DISABLE_BLUETOOTH), 1500);
                                break;
                            }
#endif
                            if (!wifi_manager_is_init()) {
                                wifi_manager_init();
                            }
                            wifi_config_stored_t config;
                            wifi_manager_load_config(&config);
                            wifi_manager_connect(&config);
                            view_toast_show(i18n_str(STR_WIFI_CONNECTING), 1000);
                        } else {
                            view_toast_error(i18n_str(STR_WIFI_NO_CONFIG), 1000);
                        }
                        break;
                    case 4:  // Deep Sleep
                        if (power_is_usb_connected()) {
                            view_toast_error(i18n_str(STR_UNPLUG_USB), 1500);
                        } else {
                            enter_deep_sleep();  // Does not return
                        }
                        break;
                    default:  // Cancel (0)
                        break;
                }
                g_app_state = APP_STATE_MAIN_MENU;
                render_current_state(false);
            } else if (key == 'N') {
                view_context_menu_hide(&g_main_quick_menu);
                g_app_state = APP_STATE_MAIN_MENU;
                render_current_state(false);
            }
            break;

        case APP_STATE_SETTINGS_MENU:
            if (key == '2') {
                view_list_screen_navigate(&g_settings_menu, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_settings_menu, true);
                render_current_state(true);
            } else if (key == 'Y' || key == '5') {
                uint8_t sel = view_list_screen_get_selection(&g_settings_menu);
                switch (sel) {
                    case SETTINGS_IDX_CHANGE_PIN:
                        view_pin_entry_init(&g_pin_entry, i18n_str(STR_CURRENT_PIN), PIN_MAX_LEN, 3);
                        g_app_state = APP_STATE_PIN_CHANGE_OLD;
                        render_current_state(false);
                        break;

                    case SETTINGS_IDX_BRIGHTNESS:
                        view_slider_init(&g_slider, i18n_str(STR_BRIGHTNESS),
                                        GUI_BACKLIGHT_MIN, GUI_BACKLIGHT_MAX,
                                        gui_get_backlight(),
                                        GUI_BACKLIGHT_STEP, "%d", "",
                                        i18n_str(STR_HINT_BRIGHTNESS));
                        g_app_state = APP_STATE_SLIDER_BRIGHTNESS;
                        render_current_state(false);
                        break;

                    case SETTINGS_IDX_TIMEZONE: {
                        // Timezone: -12 to +14 stored as 0 to 26
                        int8_t tz = badge_settings_get_timezone();
                        uint16_t tz_value = (uint16_t)(tz + 12);  // -12 -> 0, +14 -> 26
                        view_slider_init(&g_slider, i18n_str(STR_TIMEZONE),
                                        0, 26, tz_value,
                                        1, "UTC%+d", "",
                                        i18n_str(STR_HINT_BRIGHTNESS));
                        g_slider.display_offset = -12;  // Display 0 as -12, 26 as +14
                        g_app_state = APP_STATE_SLIDER_TIMEZONE;
                        render_current_state(false);
                        break;
                    }

                    case SETTINGS_IDX_BADGE_TEXTS:
                        go_to_badge_texts_menu();
                        break;

                    case SETTINGS_IDX_SET_DATETIME: {
                        // Start Date/Time wizard: Date first, then Time
                        struct tm timeinfo;
                        if (cdc_rtc_is_time_set()) {
                            cdc_rtc_get_time(&timeinfo);
                        } else {
                            timeinfo.tm_mday = 1;
                            timeinfo.tm_mon = 0;
                            timeinfo.tm_year = 125;  // 2025
                            timeinfo.tm_hour = 12;
                            timeinfo.tm_min = 0;
                        }
                        // Initialize both views (time values preserved for wizard step 2)
                        view_date_input_init(&g_date_input,
                                             timeinfo.tm_mday,
                                             timeinfo.tm_mon + 1,
                                             timeinfo.tm_year + 1900);
                        view_time_input_init(&g_time_input,
                                             timeinfo.tm_hour,
                                             timeinfo.tm_min);
                        g_app_state = APP_STATE_SET_DATE;
                        render_current_state(false);
                        break;
                    }

                    case SETTINGS_IDX_LANGUAGE: {
                        // Build language list
                        for (int i = 0; i < LANG_COUNT; i++) {
                            g_language_items[i].label = i18n_get_language_name((language_t)i);
                        }
                        view_list_screen_init(&g_language_menu, i18n_str(STR_LANGUAGE), g_language_items, LANG_COUNT);
                        // Pre-select current language
                        g_language_menu.selection = i18n_get_language();
                        g_app_state = APP_STATE_LANGUAGE;
                        render_current_state(false);
                        break;
                    }
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
                    case BADGE_IDX_NAME:
                        view_t9_input_init(&g_t9_input, i18n_str(STR_BADGE_NAME), badge_settings_get_name());
                        g_app_state = APP_STATE_BADGE_EDIT_NAME;
                        render_current_state(false);
                        break;

                    case BADGE_IDX_INFO:
                        view_t9_input_init(&g_t9_input, i18n_str(STR_BADGE_INFO), badge_settings_get_info());
                        g_app_state = APP_STATE_BADGE_EDIT_INFO;
                        render_current_state(false);
                        break;

                    case BADGE_IDX_INFO2:
                        view_t9_input_init(&g_t9_input, i18n_str(STR_BADGE_INFO2), badge_settings_get_info2());
                        g_app_state = APP_STATE_BADGE_EDIT_INFO2;
                        render_current_state(false);
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
                    view_pin_entry_init(&g_pin_entry, i18n_str(STR_NEW_PIN), PIN_MAX_LEN, 3);
                    g_app_state = APP_STATE_PIN_CHANGE_NEW;
                    render_current_state(false);
                } else {
                    if (view_pin_entry_fail(&g_pin_entry)) {
                        view_toast_error(i18n_str(STR_TOO_MANY_ATTEMPTS), 2000);
                        go_to_settings_menu();
                    } else {
                        view_toast_error(i18n_str(STR_WRONG_PIN), 1000);
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
                view_pin_entry_init(&g_pin_entry, i18n_str(STR_CONFIRM_PIN), PIN_MAX_LEN, 3);
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
                        view_toast_success(i18n_str(STR_PIN_CHANGED), 1500);
                    } else {
                        view_toast_error(i18n_str(STR_SAVE_FAILED), 1500);
                    }
                    go_to_settings_menu();
                } else {
                    // PINs don't match
                    if (view_pin_entry_fail(&g_pin_entry)) {
                        view_toast_error(i18n_str(STR_PINS_DONT_MATCH), 2000);
                        go_to_settings_menu();
                    } else {
                        view_toast_error(i18n_str(STR_PINS_DONT_MATCH), 1000);
                        // Go back to new PIN entry
                        view_pin_entry_init(&g_pin_entry, i18n_str(STR_NEW_PIN), PIN_MAX_LEN, 3);
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

        case APP_STATE_SET_DATE:
            if (key >= '0' && key <= '9') {
                view_date_input_key(&g_date_input, key);
                render_current_state(true);
            } else if (key == '6') {
                view_date_input_next_field(&g_date_input);
                render_current_state(true);
            } else if (key == '4') {
                view_date_input_prev_field(&g_date_input);
                render_current_state(true);
            } else if (key == 'Y') {
                // Date confirmed - advance to Time input (wizard step 2)
                g_app_state = APP_STATE_SET_TIME;
                render_current_state(false);
            } else if (key == 'N') {
                // N = clear current field, or go back if at start
                if (!view_date_input_clear_field(&g_date_input)) {
                    // At start of first field - cancel wizard
                    go_to_settings_menu();
                } else {
                    render_current_state(true);
                }
            }
            break;

        case APP_STATE_SET_TIME:
            if (key >= '0' && key <= '9') {
                view_time_input_key(&g_time_input, key);
                render_current_state(true);
            } else if (key == '6') {
                view_time_input_next_field(&g_time_input);
                render_current_state(true);
            } else if (key == '4') {
                view_time_input_prev_field(&g_time_input);
                render_current_state(true);
            } else if (key == 'Y') {
                // Save both date and time to RTC
                cdc_rtc_set_date(g_date_input.year, g_date_input.month, g_date_input.day);
                cdc_rtc_set_time(g_time_input.hour, g_time_input.minute, 0);
                g_last_minute = -1;  // Force clock update
                view_toast_success(i18n_str(STR_SAVED), 1000);
                go_to_settings_menu();
            } else if (key == 'N') {
                // N = clear current field, or go back to date if at start
                if (!view_time_input_clear_field(&g_time_input)) {
                    // At start of first field - go back to date
                    g_app_state = APP_STATE_SET_DATE;
                    render_current_state(false);
                } else {
                    render_current_state(true);
                }
            }
            break;

        case APP_STATE_LANGUAGE:
            if (key == '2') {
                view_list_screen_navigate(&g_language_menu, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_language_menu, true);
                render_current_state(true);
            } else if (key == 'Y' || key == '5') {
                // Save selected language
                uint8_t sel = view_list_screen_get_selection(&g_language_menu);
                i18n_set_language((language_t)sel);
                view_toast_success(i18n_str(STR_SAVED), 1000);
                go_to_settings_menu();
            } else if (key == 'N') {
                go_to_settings_menu();
            }
            break;

        case APP_STATE_LOCKOUT:
            // Ignore all key presses during lockout - device is locked for 1 minute
            (void)key;  // Suppress unused warning
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
                view_toast_success(i18n_str(STR_SAVED), 1000);
                go_to_settings_menu();
            } else if (key == 'N') {
                // Cancel - restore previous brightness
                gui_load_backlight();
                gui_set_backlight(gui_get_backlight());
                go_to_settings_menu();
            }
            break;
        }

        case APP_STATE_SLIDER_TIMEZONE:
            if (key == '6' || key == '2') {
                // Increase (more positive / less negative)
                view_slider_adjust(&g_slider, true);
                render_current_state(true);
            } else if (key == '4' || key == '8') {
                // Decrease (more negative / less positive)
                view_slider_adjust(&g_slider, false);
                render_current_state(true);
            } else if (key == 'Y') {
                // Save timezone
                uint16_t val = view_slider_get_value(&g_slider);
                int8_t tz = (int8_t)val - 12;  // Convert 0-26 back to -12 to +14
                badge_settings_set_timezone(tz);
                badge_settings_save();
                view_toast_success(i18n_str(STR_SAVED), 1000);
                go_to_settings_menu();
            } else if (key == 'N') {
                // Cancel - no save
                go_to_settings_menu();
            }
            break;

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
                view_toast_success(i18n_str(STR_SAVED), 1000);
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
                view_toast_success(i18n_str(STR_SAVED), 1000);
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
                view_toast_success(i18n_str(STR_SAVED), 1000);
                go_to_badge_texts_menu();
            }
            break;

#if FEATURE_TOTP
        case APP_STATE_TOTP_LIST:
            // Handle context menu if visible
            if (view_context_menu_is_visible(&g_totp_context_menu)) {
                if (key == '2') {
                    view_context_menu_navigate(&g_totp_context_menu, false);
                    view_context_menu_render(&g_totp_context_menu);
                } else if (key == '8') {
                    view_context_menu_navigate(&g_totp_context_menu, true);
                    view_context_menu_render(&g_totp_context_menu);
                } else if (key == 'Y') {
                    uint8_t action = view_context_menu_get_action(&g_totp_context_menu);
                    view_context_menu_hide(&g_totp_context_menu);
                    if (action == 1) {
                        // Show Code
                        show_totp_code(g_totp_selected_index);
                    } else if (action == 2) {
                        // Edit
                        totp_wizard_edit(g_totp_selected_index);
                    } else if (action == 3) {
                        // Delete
                        if (totp_store_delete(g_totp_selected_index)) {
                            view_toast_success(i18n_str(STR_DELETED), 1000);
                            go_to_totp_list();
                        } else {
                            view_toast_error(i18n_str(STR_DELETE_FAILED), 1000);
                            render_current_state(false);
                        }
                    } else if (action == 4) {
                        // Add New
                        totp_wizard_start();
                    } else {
                        // Cancel - re-render list
                        render_current_state(false);
                    }
                } else if (key == 'N') {
                    view_context_menu_hide(&g_totp_context_menu);
                    render_current_state(false);
                }
                break;
            }

            // Normal list handling
            if (key == '2') {
                view_list_screen_navigate(&g_totp_list, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_totp_list, true);
                render_current_state(true);
            } else if (key == 'Y') {
                // Only show code if list is not empty
                if (totp_store_count() > 0) {
                    uint8_t sel = view_list_screen_get_selection(&g_totp_list);
                    show_totp_code(sel);
                }
            } else if (key == '3') {
                // Show context menu
                g_totp_selected_index = view_list_screen_get_selection(&g_totp_list);
                totp_account_info_t info;
                if (totp_store_count() > 0 && totp_store_get_info(g_totp_selected_index, &info)) {
                    // Full context menu with all options
                    view_context_menu_init(&g_totp_context_menu, info.name, g_totp_context_items, 5);
                } else {
                    // Empty list - only show "Add New" option
                    static const view_context_item_t add_only_items[] = {
                        { "Add New", 4 },
                    };
                    view_context_menu_init(&g_totp_context_menu, "TOTP", add_only_items, 1);
                }
                view_context_menu_show(&g_totp_context_menu);
            } else if (key == 'N') {
                go_to_main_menu();
            }
            break;

        case APP_STATE_TOTP_CODE:
            if (key == 'Y') {
                // Type code via USB keyboard
                if (totp_store_type_code(g_totp_selected_index, true)) {
                    view_toast_success(i18n_str(STR_CODE_TYPED), 1000);
                } else {
                    view_toast_error(i18n_str(STR_USB_NOT_READY), 1000);
                }
                go_to_totp_list();
            } else if (key == '3') {
                // Show context menu for current code
                totp_account_info_t info;
                if (totp_store_get_info(g_totp_selected_index, &info)) {
                    view_context_menu_init(&g_totp_context_menu, info.name, g_totp_context_items, 5);
                    view_context_menu_show(&g_totp_context_menu);
                    g_app_state = APP_STATE_TOTP_LIST;  // Context menu uses list state
                }
            } else if (key == 'N') {
                go_to_totp_list();
            }
            break;

        // TOTP Add Wizard - Name input
        case APP_STATE_TOTP_ADD_NAME:
            if (key >= '0' && key <= '9') {
                view_t9_input_key(&g_t9_input, key);
                render_current_state(true);
            } else if (key == 'N') {
                if (strlen(g_t9_input.buffer) > 0) {
                    view_t9_input_backspace(&g_t9_input);
                    render_current_state(true);
                } else {
                    // Cancel wizard
                    go_to_totp_list();
                }
            } else if (key == 'Y') {
                if (strlen(g_t9_input.buffer) > 0) {
                    totp_wizard_next_from_name();
                }
            }
            break;

        // TOTP Add Wizard - Secret input
        case APP_STATE_TOTP_ADD_SECRET:
            if (key >= '0' && key <= '9') {
                view_t9_input_key(&g_t9_input, key);
                render_current_state(true);
            } else if (key == 'N') {
                if (strlen(g_t9_input.buffer) > 0) {
                    view_t9_input_backspace(&g_t9_input);
                    render_current_state(true);
                } else {
                    // Go back to name
                    view_t9_input_init(&g_t9_input, "Account Name", g_totp_wizard.name);
                    g_app_state = APP_STATE_TOTP_ADD_NAME;
                    render_current_state(false);
                }
            } else if (key == 'Y') {
                if (strlen(g_t9_input.buffer) > 0) {
                    totp_wizard_next_from_secret();
                }
            }
            break;

        // TOTP Add Wizard - Issuer input (optional)
        case APP_STATE_TOTP_ADD_ISSUER:
            if (key >= '0' && key <= '9') {
                view_t9_input_key(&g_t9_input, key);
                render_current_state(true);
            } else if (key == 'N') {
                if (strlen(g_t9_input.buffer) > 0) {
                    view_t9_input_backspace(&g_t9_input);
                    render_current_state(true);
                } else {
                    // Go back to secret
                    view_t9_input_init(&g_t9_input, "Secret (Base32)", g_totp_wizard.secret);
                    g_app_state = APP_STATE_TOTP_ADD_SECRET;
                    render_current_state(false);
                }
            } else if (key == 'Y') {
                // Issuer is optional, continue even if empty
                totp_wizard_next_from_issuer();
            }
            break;

        // TOTP Add Wizard - Digits selection
        case APP_STATE_TOTP_ADD_DIGITS:
            if (key == '2') {
                view_list_screen_navigate(&g_totp_digits_menu, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_totp_digits_menu, true);
                render_current_state(true);
            } else if (key == 'Y') {
                totp_wizard_next_from_digits(view_list_screen_get_selection(&g_totp_digits_menu));
            } else if (key == 'N') {
                // Go back to issuer
                view_t9_input_init(&g_t9_input, "Issuer (optional)", g_totp_wizard.issuer);
                g_app_state = APP_STATE_TOTP_ADD_ISSUER;
                render_current_state(false);
            }
            break;

        // TOTP Add Wizard - Period selection
        case APP_STATE_TOTP_ADD_PERIOD:
            if (key == '2') {
                view_list_screen_navigate(&g_totp_period_menu, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_totp_period_menu, true);
                render_current_state(true);
            } else if (key == 'Y') {
                totp_wizard_finish(view_list_screen_get_selection(&g_totp_period_menu));
            } else if (key == 'N') {
                // Go back to digits
                view_list_screen_init(&g_totp_digits_menu, "Digits", g_totp_digits_items, 3);
                g_app_state = APP_STATE_TOTP_ADD_DIGITS;
                render_current_state(false);
            }
            break;

        // TOTP Add Wizard - Algorithm selection
        case APP_STATE_TOTP_ADD_ALGO:
            if (key == '2') {
                view_list_screen_navigate(&g_totp_algo_menu, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_totp_algo_menu, true);
                render_current_state(true);
            } else if (key == 'Y') {
                totp_wizard_next_from_algo(view_list_screen_get_selection(&g_totp_algo_menu));
            } else if (key == 'N') {
                // Go back to period
                view_list_screen_init(&g_totp_period_menu, "Period", g_totp_period_items, 2);
                g_app_state = APP_STATE_TOTP_ADD_PERIOD;
                render_current_state(false);
            }
            break;
#endif

#if FEATURE_FIDO2
        case APP_STATE_FIDO_LIST:
            // Handle context menu if visible
            if (view_context_menu_is_visible(&g_context_menu)) {
                if (key == '2') {
                    view_context_menu_navigate(&g_context_menu, false);
                    view_context_menu_render(&g_context_menu);
                } else if (key == '8') {
                    view_context_menu_navigate(&g_context_menu, true);
                    view_context_menu_render(&g_context_menu);
                } else if (key == 'Y') {
                    uint8_t action = view_context_menu_get_action(&g_context_menu);
                    view_context_menu_hide(&g_context_menu);
                    if (action == 1) {
                        // Details
                        show_fido_detail(g_fido_selected_index);
                    } else if (action == 2) {
                        // Delete
                        fido2_credential_info_t info;
                        if (fido2_get_credential_info(g_fido_selected_index, &info)) {
                            if (fido2_delete_credential(info.slot)) {
                                view_toast_success(i18n_str(STR_DELETED), 1000);
                                go_to_fido_list();
                            } else {
                                view_toast_error(i18n_str(STR_DELETE_FAILED), 1000);
                                render_current_state(false);
                            }
                        }
                    } else {
                        // Cancel or unknown - re-render list
                        render_current_state(false);
                    }
                } else if (key == 'N') {
                    view_context_menu_hide(&g_context_menu);
                    render_current_state(false);  // Re-render list
                }
                break;
            }

            // Normal list handling
            if (key == '2') {
                view_list_screen_navigate(&g_fido_list, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_fido_list, true);
                render_current_state(true);
            } else if (key == 'Y') {
                uint8_t sel = view_list_screen_get_selection(&g_fido_list);
                show_fido_detail(sel);
            } else if (key == '3') {
                // Show context menu for selected item
                g_fido_selected_index = view_list_screen_get_selection(&g_fido_list);
                fido2_credential_info_t info;
                if (fido2_get_credential_info(g_fido_selected_index, &info)) {
                    build_fido_context_menu();
                    view_context_menu_init(&g_context_menu, info.rp_id, g_fido_context_items_buf, g_fido_context_items_count);
                    view_context_menu_show(&g_context_menu);
                }
            } else if (key == 'N') {
                go_to_main_menu();
            }
            break;

        case APP_STATE_FIDO_DETAIL:
            // Note: No button 3 delete - credentials should be managed via menu
            if (key == 'N') {
                go_to_fido_list();
            } else if (key == '2' || key == '8') {
                view_info_screen_scroll(&g_info_view, key == '8');
                render_current_state(true);
            }
            break;

        case APP_STATE_FIDO_PROMPT:
            // Simple flow: Y to approve (→ PIN if locked), N to deny
            if (key == 'Y') {
                // Skip device PIN if already verified via ClientPIN protocol
                if (fido2_is_pin_verified()) {
                    LOG_I("FIDO2", "PIN already verified via ClientPIN - skipping device PIN");
                    fido2_set_pin_verified(false);  // Clear flag after use
                    fido2_prompt_complete(FIDO2_UP_APPROVED);
                } else if (g_fido_was_locked && pin_storage_is_set()) {
                    // Was on lock screen AND PIN is set → require PIN to unlock
                    LOG_I("FIDO2", "Locked - PIN required");
                    view_pin_entry_init(&g_pin_entry, i18n_str(STR_ENTER_PIN), PIN_MAX_LEN, 3);
                    g_app_state = APP_STATE_FIDO_PROMPT_PIN;
                    render_current_state(false);
                } else {
                    // Not locked OR no PIN set → approve directly
                    LOG_I("FIDO2", "Approving (locked=%d, pin_set=%d)",
                          g_fido_was_locked, pin_storage_is_set());
                    fido2_prompt_complete(FIDO2_UP_APPROVED);
                }
            } else if (key == 'N') {
                LOG_I("FIDO2", "Denied");
                fido2_set_pin_verified(false);  // Clear flag on deny
                fido2_prompt_complete(FIDO2_UP_DENIED);
            }
            break;

        case APP_STATE_FIDO_PROMPT_PIN:
            // PIN entry: digits, backspace, confirm
            if (key >= '0' && key <= '9') {
                view_pin_entry_digit(&g_pin_entry, key);
                render_current_state(true);
            } else if (key == 'N') {
                if (g_pin_entry.len > 0) {
                    view_pin_entry_backspace(&g_pin_entry);
                    render_current_state(true);
                } else {
                    // Empty + N = Cancel
                    LOG_I("FIDO2", "Cancelled");
                    fido2_prompt_complete(FIDO2_UP_DENIED);
                }
            } else if (key == 'Y' && g_pin_entry.len >= PIN_MIN_LEN) {
                const char *entered = view_pin_entry_get_pin(&g_pin_entry);
                if (pin_storage_verify(entered)) {
                    // PIN correct → APPROVE (user pressed Y + PIN correct = both conditions met)
                    LOG_I("FIDO2", "PIN OK - approving");
                    fido2_prompt_complete(FIDO2_UP_APPROVED);
                } else {
                    // Wrong PIN
                    if (view_pin_entry_fail(&g_pin_entry)) {
                        view_toast_error(i18n_str(STR_TOO_MANY_ATTEMPTS), 2000);
                        fido2_prompt_complete(FIDO2_UP_DENIED);
                    } else {
                        view_toast_error(i18n_str(STR_WRONG_PIN), 1000);
                        render_current_state(true);
                    }
                }
            }
            break;
#endif

        // WiFi states
        case APP_STATE_WIFI_SCAN:
            // Scanning in progress - N to cancel
            if (key == 'N') {
                wifi_manager_deinit();
                go_to_main_menu();
            }
            break;

        case APP_STATE_WIFI_LIST:
            // Handle context menu if visible
            if (view_context_menu_is_visible(&g_wifi_context_menu)) {
                if (key == '2') {
                    view_context_menu_navigate(&g_wifi_context_menu, false);
                    view_context_menu_render(&g_wifi_context_menu);
                } else if (key == '8') {
                    view_context_menu_navigate(&g_wifi_context_menu, true);
                    view_context_menu_render(&g_wifi_context_menu);
                } else if (key == 'Y') {
                    uint8_t action = view_context_menu_get_action(&g_wifi_context_menu);
                    view_context_menu_hide(&g_wifi_context_menu);
                    if (action == 1) {
                        // Connect - start wizard with selected network
                        wifi_network_t net;
                        if (wifi_manager_get_network(g_wifi_selected_index, &net)) {
                            strncpy(g_wifi_wizard.ssid, net.ssid, WIFI_SSID_MAX_LEN);
                            g_wifi_wizard.auth_mode = net.auth_mode;
                            g_wifi_wizard.from_scan = true;
                            if (net.auth_mode == WIFI_AUTH_OPEN) {
                                // Open network - skip password
                                g_wifi_wizard.password[0] = '\0';
                                build_wifi_ip_menu();
                                view_list_screen_init(&g_wifi_ip_menu, i18n_str(STR_WIFI_IP_MODE), g_wifi_ip_items, 2);
                                g_app_state = APP_STATE_WIFI_ADD_IP_MODE;
                            } else {
                                // Need password
                                view_t9_input_init(&g_t9_input, i18n_str(STR_WIFI_PASSWORD), "");
                                g_app_state = APP_STATE_WIFI_ADD_PASSWORD;
                            }
                            render_current_state(false);
                        }
                    } else if (action == 2) {
                        // Add Manual
                        memset(&g_wifi_wizard, 0, sizeof(g_wifi_wizard));
                        g_wifi_wizard.from_scan = false;
                        view_t9_input_init(&g_t9_input, i18n_str(STR_WIFI_SSID), "");
                        g_app_state = APP_STATE_WIFI_ADD_SSID;
                        render_current_state(false);
                    } else {
                        // Cancel
                        render_current_state(false);
                    }
                } else if (key == 'N') {
                    view_context_menu_hide(&g_wifi_context_menu);
                    render_current_state(false);
                }
                break;
            }

            // Normal list handling
            if (key == '2') {
                view_list_screen_navigate(&g_wifi_list, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_wifi_list, true);
                render_current_state(true);
            } else if (key == 'Y' || key == '5') {
                // Select network or show context menu
                g_wifi_selected_index = view_list_screen_get_selection(&g_wifi_list);
                wifi_network_t net;
                if (wifi_manager_get_network(g_wifi_selected_index, &net)) {
                    view_context_menu_init(&g_wifi_context_menu, net.ssid, g_wifi_context_items, 3);
                    view_context_menu_show(&g_wifi_context_menu);
                }
            } else if (key == '3') {
                // Context menu
                g_wifi_selected_index = view_list_screen_get_selection(&g_wifi_list);
                view_context_menu_init(&g_wifi_context_menu, "WiFi", g_wifi_context_items, 3);
                view_context_menu_show(&g_wifi_context_menu);
            } else if (key == 'N') {
                wifi_manager_deinit();
                go_to_main_menu();
            }
            break;

        case APP_STATE_WIFI_ADD_SSID:
            if (key >= '0' && key <= '9') {
                view_t9_input_key(&g_t9_input, key);
                render_current_state(true);
            } else if (key == 'N') {
                if (strlen(g_t9_input.buffer) > 0) {
                    view_t9_input_backspace(&g_t9_input);
                    render_current_state(true);
                } else {
                    // Cancel - back to list
                    g_app_state = APP_STATE_WIFI_LIST;
                    render_current_state(false);
                }
            } else if (key == 'Y') {
                if (strlen(g_t9_input.buffer) > 0) {
                    strncpy(g_wifi_wizard.ssid, g_t9_input.buffer, WIFI_SSID_MAX_LEN);
                    // Go to auth selection
                    view_list_screen_init(&g_wifi_auth_menu, i18n_str(STR_WIFI_ENCRYPTION), g_wifi_auth_items, 6);
                    g_app_state = APP_STATE_WIFI_ADD_AUTH;
                    render_current_state(false);
                }
            }
            break;

        case APP_STATE_WIFI_ADD_AUTH:
            if (key == '2') {
                view_list_screen_navigate(&g_wifi_auth_menu, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_wifi_auth_menu, true);
                render_current_state(true);
            } else if (key == 'Y') {
                uint8_t sel = view_list_screen_get_selection(&g_wifi_auth_menu);
                // Map selection to auth mode
                switch (sel) {
                    case 0: g_wifi_wizard.auth_mode = WIFI_AUTH_WPA2_PSK; break;
                    case 1: g_wifi_wizard.auth_mode = WIFI_AUTH_WPA_WPA2_PSK; break;
                    case 2: g_wifi_wizard.auth_mode = WIFI_AUTH_WPA3_PSK; break;
                    case 3: g_wifi_wizard.auth_mode = WIFI_AUTH_WPA_PSK; break;
                    case 4: g_wifi_wizard.auth_mode = WIFI_AUTH_OPEN; break;
                    case 5: g_wifi_wizard.auth_mode = WIFI_AUTH_WEP; break;
                }
                if (g_wifi_wizard.auth_mode == WIFI_AUTH_OPEN) {
                    // Open - skip password
                    g_wifi_wizard.password[0] = '\0';
                    build_wifi_ip_menu();
                    view_list_screen_init(&g_wifi_ip_menu, i18n_str(STR_WIFI_IP_MODE), g_wifi_ip_items, 2);
                    g_app_state = APP_STATE_WIFI_ADD_IP_MODE;
                } else {
                    view_t9_input_init(&g_t9_input, i18n_str(STR_WIFI_PASSWORD), "");
                    g_app_state = APP_STATE_WIFI_ADD_PASSWORD;
                }
                render_current_state(false);
            } else if (key == 'N') {
                // Back to SSID
                view_t9_input_init(&g_t9_input, i18n_str(STR_WIFI_SSID), g_wifi_wizard.ssid);
                g_app_state = APP_STATE_WIFI_ADD_SSID;
                render_current_state(false);
            }
            break;

        case APP_STATE_WIFI_ADD_PASSWORD:
            if (key >= '0' && key <= '9') {
                view_t9_input_key(&g_t9_input, key);
                render_current_state(true);
            } else if (key == 'N') {
                if (strlen(g_t9_input.buffer) > 0) {
                    view_t9_input_backspace(&g_t9_input);
                    render_current_state(true);
                } else {
                    // Back to auth selection (if from manual) or list (if from scan)
                    if (g_wifi_wizard.from_scan) {
                        g_app_state = APP_STATE_WIFI_LIST;
                    } else {
                        view_list_screen_init(&g_wifi_auth_menu, i18n_str(STR_WIFI_ENCRYPTION), g_wifi_auth_items, 6);
                        g_app_state = APP_STATE_WIFI_ADD_AUTH;
                    }
                    render_current_state(false);
                }
            } else if (key == 'Y') {
                strncpy(g_wifi_wizard.password, g_t9_input.buffer, WIFI_PASSWORD_MAX_LEN);
                build_wifi_ip_menu();
                view_list_screen_init(&g_wifi_ip_menu, i18n_str(STR_WIFI_IP_MODE), g_wifi_ip_items, 2);
                g_app_state = APP_STATE_WIFI_ADD_IP_MODE;
                render_current_state(false);
            }
            break;

        case APP_STATE_WIFI_ADD_IP_MODE:
            if (key == '2') {
                view_list_screen_navigate(&g_wifi_ip_menu, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_wifi_ip_menu, true);
                render_current_state(true);
            } else if (key == 'Y') {
                uint8_t sel = view_list_screen_get_selection(&g_wifi_ip_menu);
                g_wifi_wizard.use_dhcp = (sel == 0);
                if (g_wifi_wizard.use_dhcp) {
                    // DHCP - start connecting
#if FEATURE_BLE_UART
                    if (ble_is_active()) {
                        view_toast_error(i18n_str(STR_WIFI_DISABLE_BLUETOOTH), 1500);
                        render_current_state(false);
                        break;
                    }
#endif
                    wifi_config_stored_t config = {};
                    strncpy(config.ssid, g_wifi_wizard.ssid, WIFI_SSID_MAX_LEN);
                    strncpy(config.password, g_wifi_wizard.password, WIFI_PASSWORD_MAX_LEN);
                    config.auth_mode = g_wifi_wizard.auth_mode;
                    config.use_dhcp = true;
                    wifi_manager_connect(&config);
                    g_wifi_connect_start = millis();
                    view_info_screen_init(&g_info_view, g_wifi_wizard.ssid, i18n_str(STR_WIFI_CONNECTING));
                    g_app_state = APP_STATE_WIFI_CONNECTING;
                } else {
                    // Static IP - input IP address
                    view_ip_input_init(&g_ip_input, i18n_str(STR_WIFI_STATIC), g_wifi_wizard.static_ip);
                    g_app_state = APP_STATE_WIFI_ADD_STATIC_IP;
                }
                render_current_state(false);
            } else if (key == 'N') {
                // Back to password
                view_t9_input_init(&g_t9_input, i18n_str(STR_WIFI_PASSWORD), g_wifi_wizard.password);
                g_app_state = APP_STATE_WIFI_ADD_PASSWORD;
                render_current_state(false);
            }
            break;

        case APP_STATE_WIFI_ADD_STATIC_IP:
            if (key >= '0' && key <= '9') {
                if (view_ip_input_key(&g_ip_input, key)) {
                    render_current_state(true);
                }
            } else if (key == 'Y') {
                if (g_ip_input.field < 3) {
                    view_ip_input_next_field(&g_ip_input);
                    render_current_state(true);
                } else {
                    ip_input_to_string(&g_ip_input, g_wifi_wizard.static_ip, sizeof(g_wifi_wizard.static_ip));
                    if (ip_string_empty(g_wifi_wizard.gateway)) {
                        snprintf(g_wifi_wizard.gateway, sizeof(g_wifi_wizard.gateway), "%u.%u.%u.1",
                                 g_ip_input.octet[0], g_ip_input.octet[1], g_ip_input.octet[2]);
                    }
                    if (ip_string_empty(g_wifi_wizard.subnet)) {
                        strncpy(g_wifi_wizard.subnet, "255.255.255.0", sizeof(g_wifi_wizard.subnet));
                        g_wifi_wizard.subnet[sizeof(g_wifi_wizard.subnet) - 1] = '\0';
                    }
                    view_ip_input_init(&g_ip_input, i18n_str(STR_WIFI_GATEWAY), g_wifi_wizard.gateway);
                    g_app_state = APP_STATE_WIFI_ADD_GATEWAY;
                    render_current_state(false);
                }
            } else if (key == 'N') {
                if (view_ip_input_clear_field(&g_ip_input)) {
                    render_current_state(true);
                } else {
                    build_wifi_ip_menu();
                    view_list_screen_init(&g_wifi_ip_menu, i18n_str(STR_WIFI_IP_MODE), g_wifi_ip_items, 2);
                    g_app_state = APP_STATE_WIFI_ADD_IP_MODE;
                    render_current_state(false);
                }
            }
            break;

        case APP_STATE_WIFI_ADD_GATEWAY:
            if (key >= '0' && key <= '9') {
                if (view_ip_input_key(&g_ip_input, key)) {
                    render_current_state(true);
                }
            } else if (key == 'Y') {
                if (g_ip_input.field < 3) {
                    view_ip_input_next_field(&g_ip_input);
                    render_current_state(true);
                } else {
                    ip_input_to_string(&g_ip_input, g_wifi_wizard.gateway, sizeof(g_wifi_wizard.gateway));
                    if (ip_string_empty(g_wifi_wizard.subnet)) {
                        strncpy(g_wifi_wizard.subnet, "255.255.255.0", sizeof(g_wifi_wizard.subnet));
                        g_wifi_wizard.subnet[sizeof(g_wifi_wizard.subnet) - 1] = '\0';
                    }
                    view_ip_input_init(&g_ip_input, i18n_str(STR_WIFI_NETMASK), g_wifi_wizard.subnet);
                    g_app_state = APP_STATE_WIFI_ADD_NETMASK;
                    render_current_state(false);
                }
            } else if (key == 'N') {
                if (view_ip_input_clear_field(&g_ip_input)) {
                    render_current_state(true);
                } else {
                    view_ip_input_init(&g_ip_input, i18n_str(STR_WIFI_STATIC), g_wifi_wizard.static_ip);
                    g_app_state = APP_STATE_WIFI_ADD_STATIC_IP;
                    render_current_state(false);
                }
            }
            break;

        case APP_STATE_WIFI_ADD_NETMASK:
            if (key >= '0' && key <= '9') {
                if (view_ip_input_key(&g_ip_input, key)) {
                    render_current_state(true);
                }
            } else if (key == 'Y') {
                if (g_ip_input.field < 3) {
                    view_ip_input_next_field(&g_ip_input);
                    render_current_state(true);
                } else {
                    ip_input_to_string(&g_ip_input, g_wifi_wizard.subnet, sizeof(g_wifi_wizard.subnet));
#if FEATURE_BLE_UART
                    if (ble_is_active()) {
                        view_toast_error(i18n_str(STR_WIFI_DISABLE_BLUETOOTH), 1500);
                        render_current_state(false);
                        break;
                    }
#endif
                    wifi_config_stored_t config = {};
                    strncpy(config.ssid, g_wifi_wizard.ssid, WIFI_SSID_MAX_LEN);
                    strncpy(config.password, g_wifi_wizard.password, WIFI_PASSWORD_MAX_LEN);
                    config.auth_mode = g_wifi_wizard.auth_mode;
                    config.use_dhcp = false;
                    uint32_t ip = 0;
                    uint32_t gateway = 0;
                    uint32_t subnet = 0;
                    if (ip_string_to_u32(g_wifi_wizard.static_ip, &ip)) {
                        config.static_ip = ip;
                    }
                    if (ip_string_to_u32(g_wifi_wizard.gateway, &gateway)) {
                        config.gateway = gateway;
                    }
                    if (ip_string_to_u32(g_wifi_wizard.subnet, &subnet)) {
                        config.subnet = subnet;
                    }
                    config.dns = 0x08080808;  // 8.8.8.8
                    wifi_manager_connect(&config);
                    g_wifi_connect_start = millis();
                    view_info_screen_init(&g_info_view, g_wifi_wizard.ssid, i18n_str(STR_WIFI_CONNECTING));
                    g_app_state = APP_STATE_WIFI_CONNECTING;
                    render_current_state(false);
                }
            } else if (key == 'N') {
                if (view_ip_input_clear_field(&g_ip_input)) {
                    render_current_state(true);
                } else {
                    view_ip_input_init(&g_ip_input, i18n_str(STR_WIFI_GATEWAY), g_wifi_wizard.gateway);
                    g_app_state = APP_STATE_WIFI_ADD_GATEWAY;
                    render_current_state(false);
                }
            }
            break;

        case APP_STATE_WIFI_CONNECTING:
            if (key == 'N') {
                wifi_manager_disconnect();
                wifi_manager_deinit();
                go_to_main_menu();
            }
            break;

        case APP_STATE_WIFI_DETAILS:
            if (key == '2' || key == '8') {
                view_info_screen_scroll(&g_info_view, key == '8');
                render_current_state(true);
            } else if (key == 'N') {
                build_tools_wifi_menu();
                view_list_screen_init(&g_tools_wifi_menu, i18n_str(STR_WIFI_MENU), g_tools_wifi_items, 4);
                g_app_state = APP_STATE_TOOLS_WIFI_MENU;
                render_current_state(false);
            }
            break;

        // Tools menu states
        case APP_STATE_TOOLS_MENU:
            if (key == '2') {
                view_list_screen_navigate(&g_tools_menu, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_tools_menu, true);
                render_current_state(true);
            } else if (key == 'Y' || key == '5') {
                uint8_t sel = view_list_screen_get_selection(&g_tools_menu);
                if (sel == 0) {
                    // NTP Sync - check if WiFi already connected
#if FEATURE_BLE_UART
                    if (ble_is_active()) {
                        view_toast_error(i18n_str(STR_WIFI_DISABLE_BLUETOOTH), 1500);
                        render_current_state(false);
                        break;
                    }
#endif
                    if (wifi_manager_get_state() == WIFI_STATE_CONNECTED) {
                        // WiFi already connected - no session needed, just use existing connection
                        g_ntp_wifi_session = 0;  // No session = don't disconnect after
                        uint32_t ntp_server = wifi_manager_get_ntp_server();
                        ntp_sync_start(ntp_server);
                        g_ntp_sync_phase = 2;  // Skip to NTP sync phase
                        view_info_screen_init(&g_info_view, i18n_str(STR_NTP_SYNC), i18n_str(STR_NTP_SYNCING));
                        g_app_state = APP_STATE_TOOLS_NTP_SYNC;
                        render_current_state(false);
                    } else {
                        // Need to connect WiFi first - acquire session
                        if (!wifi_manager_has_config()) {
                            view_toast_error(i18n_str(STR_WIFI_NO_CONFIG), 1500);
                            render_current_state(false);
                            break;
                        }
                        // Acquire WiFi session for NTP
                        g_ntp_wifi_session = wifi_manager_session_acquire();
                        wifi_manager_init();
                        wifi_config_stored_t config;
                        wifi_manager_load_config(&config);
                        wifi_manager_connect(&config);
                        g_wifi_connect_start = millis();
                        g_ntp_sync_phase = 1;  // Connecting WiFi
                        view_info_screen_init(&g_info_view, i18n_str(STR_NTP_SYNC), i18n_str(STR_WIFI_CONNECTING));
                        g_app_state = APP_STATE_TOOLS_NTP_SYNC;
                        render_current_state(false);
                    }
                } else if (sel == 1) {
                    // WiFi submenu
                    build_tools_wifi_menu();
                    view_list_screen_init(&g_tools_wifi_menu, i18n_str(STR_WIFI_MENU), g_tools_wifi_items, 4);
                    g_app_state = APP_STATE_TOOLS_WIFI_MENU;
                    render_current_state(false);
                }
            } else if (key == 'N') {
                go_to_main_menu();
            }
            break;

        case APP_STATE_TOOLS_NTP_SYNC:
            if (key == 'N') {
                // Cancel NTP sync - cleanup
                ntp_sync_stop();
                // Release session (will auto-deinit if we acquired it)
                if (g_ntp_wifi_session > 0) {
                    wifi_manager_session_release(g_ntp_wifi_session, true);
                    g_ntp_wifi_session = 0;
                }
                g_ntp_sync_phase = 0;
                build_tools_menu();
                view_list_screen_init(&g_tools_menu, i18n_str(STR_TOOLS), g_tools_items, 2);
                g_app_state = APP_STATE_TOOLS_MENU;
                render_current_state(false);
            }
            break;

        case APP_STATE_TOOLS_WIFI_MENU:
            if (key == '2') {
                view_list_screen_navigate(&g_tools_wifi_menu, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_tools_wifi_menu, true);
                render_current_state(true);
            } else if (key == 'Y' || key == '5') {
                uint8_t sel = view_list_screen_get_selection(&g_tools_wifi_menu);
                if (sel == 0) {
                    // Connect - use saved config
                    if (wifi_manager_has_config()) {
#if FEATURE_BLE_UART
                        if (ble_is_active()) {
                            view_toast_error(i18n_str(STR_WIFI_DISABLE_BLUETOOTH), 1500);
                            render_current_state(false);
                            break;
                        }
#endif
                        wifi_manager_init();
                        wifi_config_stored_t config;
                        wifi_manager_load_config(&config);
                        wifi_manager_connect(&config);
                        g_wifi_connect_start = millis();
                        view_info_screen_init(&g_info_view, config.ssid, i18n_str(STR_WIFI_CONNECTING));
                        g_app_state = APP_STATE_WIFI_CONNECTING;
                        render_current_state(false);
                    } else {
                        view_toast_error(i18n_str(STR_WIFI_NO_CONFIG), 1500);
                        render_current_state(false);
                    }
                } else if (sel == 1) {
                    // Setup - start WiFi scan
#if FEATURE_BLE_UART
                    if (ble_is_active()) {
                        view_toast_error(i18n_str(STR_WIFI_DISABLE_BLUETOOTH), 1500);
                        render_current_state(false);
                        break;
                    }
#endif
                    wifi_manager_init();
                    wifi_manager_start_scan();
                    g_wifi_scan_start = millis();
                    view_info_screen_init(&g_info_view, i18n_str(STR_WIFI_MENU), i18n_str(STR_WIFI_SCANNING));
                    g_app_state = APP_STATE_WIFI_SCAN;
                    render_current_state(false);
                } else if (sel == 2) {
                    // Details
                    char details[512];
                    wifi_state_t state = wifi_manager_get_state();
                    if (state == WIFI_STATE_CONNECTED) {
                        wifi_info_t info;
                        wifi_manager_get_info(&info);
                        snprintf(details, sizeof(details),
                            "SSID: %s\n"
                            "MAC: %02X:%02X:%02X:%02X:%02X:%02X\n"
                            "IP: %u.%u.%u.%u\n"
                            "Gateway: %u.%u.%u.%u\n"
                            "Subnet: %u.%u.%u.%u\n"
                            "DNS: %u.%u.%u.%u\n"
                            "Security: %s\n"
                            "RSSI: %d dBm\n"
                            "Channel: %d",
                            info.ssid,
                            info.mac[0], info.mac[1], info.mac[2],
                            info.mac[3], info.mac[4], info.mac[5],
                            (unsigned)IP_BYTE(info.ip, 0), (unsigned)IP_BYTE(info.ip, 1),
                            (unsigned)IP_BYTE(info.ip, 2), (unsigned)IP_BYTE(info.ip, 3),
                            (unsigned)IP_BYTE(info.gateway, 0), (unsigned)IP_BYTE(info.gateway, 1),
                            (unsigned)IP_BYTE(info.gateway, 2), (unsigned)IP_BYTE(info.gateway, 3),
                            (unsigned)IP_BYTE(info.subnet, 0), (unsigned)IP_BYTE(info.subnet, 1),
                            (unsigned)IP_BYTE(info.subnet, 2), (unsigned)IP_BYTE(info.subnet, 3),
                            (unsigned)IP_BYTE(info.dns, 0), (unsigned)IP_BYTE(info.dns, 1),
                            (unsigned)IP_BYTE(info.dns, 2), (unsigned)IP_BYTE(info.dns, 3),
                            wifi_auth_mode_to_string(info.auth_mode),
                            info.rssi,
                            info.channel);
                    } else {
                        wifi_config_stored_t config;
                        if (wifi_manager_load_config(&config)) {
                            snprintf(details, sizeof(details),
                                "Status: %s\n\n"
                                "Saved Config:\n"
                                "SSID: %s\n"
                                "Security: %s\n"
                                "IP Mode: %s\n"
                                "Last IP: %u.%u.%u.%u",
                                i18n_str(STR_WIFI_DISCONNECTED),
                                config.ssid,
                                wifi_auth_mode_to_string(config.auth_mode),
                                config.use_dhcp ? "DHCP" : "Static",
                                (unsigned)IP_BYTE(config.last_ip, 0), (unsigned)IP_BYTE(config.last_ip, 1),
                                (unsigned)IP_BYTE(config.last_ip, 2), (unsigned)IP_BYTE(config.last_ip, 3));
                        } else {
                            snprintf(details, sizeof(details), "%s", i18n_str(STR_WIFI_NO_CONFIG));
                        }
                    }
                    view_info_screen_init(&g_info_view, i18n_str(STR_WIFI_DETAILS), details);
                    g_app_state = APP_STATE_WIFI_DETAILS;
                    render_current_state(false);
                } else if (sel == 3) {
                    // Disconnect
                    wifi_manager_disconnect();
                    wifi_manager_deinit();
                    view_toast_success(i18n_str(STR_WIFI_DISCONNECTED), 1500);
                    render_current_state(false);
                }
            } else if (key == 'N') {
                build_tools_menu();
                view_list_screen_init(&g_tools_menu, i18n_str(STR_TOOLS), g_tools_items, 2);
                g_app_state = APP_STATE_TOOLS_MENU;
                render_current_state(false);
            }
            break;

#if FEATURE_CA
        case APP_STATE_CA_MENU:
            if (key == '2') {
                view_list_screen_navigate(&g_ca_menu, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_ca_menu, true);
                render_current_state(true);
            } else if (key == 'Y' || key == '5') {
                uint8_t sel = view_list_screen_get_selection(&g_ca_menu);
                switch (sel) {
                    case CA_IDX_STATUS: {
                        // Show CA status details
                        ca_status_t status;
                        if (ca_get_status(&status) && status.initialized) {
                            snprintf(g_ca_detail_text, sizeof(g_ca_detail_text),
                                "%s: %s\n\n"
                                "%s: %s\n\n"
                                "%s: %lu\n\n"
                                "%s: %lu",
                                i18n_str(STR_CA_COMMON_NAME), status.common_name,
                                i18n_str(STR_CA_INITIALIZED), i18n_str(STR_OK),
                                i18n_str(STR_CA_ISSUED_CERTS), (unsigned long)status.issued_count,
                                i18n_str(STR_CA_SERIAL), (unsigned long)status.serial_counter);
                        } else {
                            snprintf(g_ca_detail_text, sizeof(g_ca_detail_text),
                                "%s", i18n_str(STR_CA_NOT_INIT));
                        }
                        view_info_screen_init(&g_info_view, i18n_str(STR_CA_STATUS), g_ca_detail_text);
                        g_app_state = APP_STATE_CA_DETAILS;
                        render_current_state(false);
                        break;
                    }
                    case CA_IDX_GENERATE:
                        if (ca_is_initialized()) {
                            // CA exists - show reset confirmation
                            view_info_screen_init(&g_info_view, i18n_str(STR_CA_RESET),
                                i18n_str(STR_CA_RESET_CONFIRM));
                            g_app_state = APP_STATE_CA_RESET_CONFIRM;
                            render_current_state(false);
                        } else {
                            // No CA - start wizard with Common Name
                            memset(&g_ca_wizard, 0, sizeof(g_ca_wizard));
                            g_ca_wizard.validity_years = 10;  // Default 10 years
                            view_t9_input_init(&g_t9_input, i18n_str(STR_CA_ENTER_CN), "");
                            g_app_state = APP_STATE_CA_WIZARD_CN;
                            render_current_state(false);
                        }
                        break;
                    case CA_IDX_EXPORT:
                        if (!ca_is_initialized()) {
                            view_toast_error(i18n_str(STR_CA_NOT_INIT), 1500);
                        } else {
                            size_t out_len;
                            if (ca_export_root_cert_pem(g_ca_detail_text, sizeof(g_ca_detail_text), &out_len)) {
                                view_info_screen_init(&g_info_view, i18n_str(STR_CA_EXPORT), g_ca_detail_text);
                                g_app_state = APP_STATE_CA_DETAILS;
                                render_current_state(false);
                            } else {
                                view_toast_error("Export failed", 1500);
                            }
                        }
                        break;
                    case CA_IDX_QR_CODE:
                        if (!ca_is_initialized()) {
                            view_toast_error(i18n_str(STR_CA_NOT_INIT), 1500);
                        } else {
                            size_t out_len = 0;
                            if (ca_export_pubkey_base64(g_ca_detail_text, sizeof(g_ca_detail_text), &out_len)) {
                                view_qr_code_init(&g_qr_view, "CA Public Key", g_ca_detail_text);
                                g_app_state = APP_STATE_CA_QR_CODE;
                                render_current_state(false);
                            } else {
                                view_toast_error("Export failed", 1500);
                            }
                        }
                        break;
                }
            } else if (key == 'N') {
                go_to_main_menu();
            }
            break;

        case APP_STATE_CA_DETAILS:
            if (key == '2') {
                view_info_screen_scroll(&g_info_view, false);  // Scroll up
                render_current_state(true);
            } else if (key == '8') {
                view_info_screen_scroll(&g_info_view, true);   // Scroll down
                render_current_state(true);
            } else if (key == 'N') {
                g_app_state = APP_STATE_CA_MENU;
                render_current_state(false);
            }
            break;

        case APP_STATE_CA_GENERATING:
            // Wait for generation to complete, no key handling
            break;

        case APP_STATE_CA_QR_CODE:
            if (key == 'N') {
                g_app_state = APP_STATE_CA_MENU;
                render_current_state(false);
            }
            break;

        case APP_STATE_CA_RESET_CONFIRM:
            if (key == 'Y') {
                // User confirmed reset - delete CA
                if (ca_factory_reset()) {
                    view_toast_success(i18n_str(STR_CA_RESET_SUCCESS), 1500);
                } else {
                    view_toast_error("Reset failed", 1500);
                }
                build_ca_menu();
                view_list_screen_init(&g_ca_menu, i18n_str(STR_CA_MENU), g_ca_items, CA_IDX_COUNT);
                g_app_state = APP_STATE_CA_MENU;
                render_current_state(false);
            } else if (key == 'N') {
                // User cancelled
                g_app_state = APP_STATE_CA_MENU;
                render_current_state(false);
            }
            break;

        // CA Wizard: Common Name (required)
        case APP_STATE_CA_WIZARD_CN:
            if (key >= '0' && key <= '9') {
                view_t9_input_key(&g_t9_input, key);
                render_current_state(true);
            } else if (key == 'N') {
                if (g_t9_input.len > 0) {
                    view_t9_input_backspace(&g_t9_input);
                    render_current_state(true);
                } else {
                    // Cancel wizard - go back to menu
                    g_app_state = APP_STATE_CA_MENU;
                    render_current_state(false);
                }
            } else if (key == 'Y') {
                const char *text = view_t9_input_get_text(&g_t9_input);
                if (strlen(text) == 0) {
                    view_toast_error("CN required", 1500);
                } else {
                    strncpy(g_ca_wizard.cn, text, CA_FIELD_MAX_LEN - 1);
                    // Next: Organization
                    view_t9_input_init(&g_t9_input, i18n_str(STR_CA_ENTER_ORG), g_ca_wizard.org);
                    g_app_state = APP_STATE_CA_WIZARD_ORG;
                    render_current_state(false);
                }
            }
            break;

        // CA Wizard: Organization (optional)
        case APP_STATE_CA_WIZARD_ORG:
            if (key >= '0' && key <= '9') {
                view_t9_input_key(&g_t9_input, key);
                render_current_state(true);
            } else if (key == 'N') {
                if (g_t9_input.len > 0) {
                    view_t9_input_backspace(&g_t9_input);
                    render_current_state(true);
                } else {
                    // Back to CN
                    view_t9_input_init(&g_t9_input, i18n_str(STR_CA_ENTER_CN), g_ca_wizard.cn);
                    g_app_state = APP_STATE_CA_WIZARD_CN;
                    render_current_state(false);
                }
            } else if (key == 'Y') {
                strncpy(g_ca_wizard.org, view_t9_input_get_text(&g_t9_input), CA_FIELD_MAX_LEN - 1);
                // Next: Org Unit
                view_t9_input_init(&g_t9_input, i18n_str(STR_CA_ENTER_OU), g_ca_wizard.ou);
                g_app_state = APP_STATE_CA_WIZARD_OU;
                render_current_state(false);
            }
            break;

        // CA Wizard: Organizational Unit (optional)
        case APP_STATE_CA_WIZARD_OU:
            if (key >= '0' && key <= '9') {
                view_t9_input_key(&g_t9_input, key);
                render_current_state(true);
            } else if (key == 'N') {
                if (g_t9_input.len > 0) {
                    view_t9_input_backspace(&g_t9_input);
                    render_current_state(true);
                } else {
                    // Back to Org
                    view_t9_input_init(&g_t9_input, i18n_str(STR_CA_ENTER_ORG), g_ca_wizard.org);
                    g_app_state = APP_STATE_CA_WIZARD_ORG;
                    render_current_state(false);
                }
            } else if (key == 'Y') {
                strncpy(g_ca_wizard.ou, view_t9_input_get_text(&g_t9_input), CA_FIELD_MAX_LEN - 1);
                // Next: Country
                view_t9_input_init(&g_t9_input, i18n_str(STR_CA_ENTER_COUNTRY), g_ca_wizard.country);
                g_app_state = APP_STATE_CA_WIZARD_COUNTRY;
                render_current_state(false);
            }
            break;

        // CA Wizard: Country (optional, 2 letters)
        case APP_STATE_CA_WIZARD_COUNTRY:
            if (key >= '0' && key <= '9') {
                view_t9_input_key(&g_t9_input, key);
                render_current_state(true);
            } else if (key == 'N') {
                if (g_t9_input.len > 0) {
                    view_t9_input_backspace(&g_t9_input);
                    render_current_state(true);
                } else {
                    // Back to OU
                    view_t9_input_init(&g_t9_input, i18n_str(STR_CA_ENTER_OU), g_ca_wizard.ou);
                    g_app_state = APP_STATE_CA_WIZARD_OU;
                    render_current_state(false);
                }
            } else if (key == 'Y') {
                strncpy(g_ca_wizard.country, view_t9_input_get_text(&g_t9_input), 2);
                // Next: Locality
                view_t9_input_init(&g_t9_input, i18n_str(STR_CA_ENTER_LOCALITY), g_ca_wizard.locality);
                g_app_state = APP_STATE_CA_WIZARD_LOCALITY;
                render_current_state(false);
            }
            break;

        // CA Wizard: Locality/City (optional)
        case APP_STATE_CA_WIZARD_LOCALITY:
            if (key >= '0' && key <= '9') {
                view_t9_input_key(&g_t9_input, key);
                render_current_state(true);
            } else if (key == 'N') {
                if (g_t9_input.len > 0) {
                    view_t9_input_backspace(&g_t9_input);
                    render_current_state(true);
                } else {
                    // Back to Country
                    view_t9_input_init(&g_t9_input, i18n_str(STR_CA_ENTER_COUNTRY), g_ca_wizard.country);
                    g_app_state = APP_STATE_CA_WIZARD_COUNTRY;
                    render_current_state(false);
                }
            } else if (key == 'Y') {
                strncpy(g_ca_wizard.locality, view_t9_input_get_text(&g_t9_input), CA_FIELD_MAX_LEN - 1);
                // Next: State/Province
                view_t9_input_init(&g_t9_input, i18n_str(STR_CA_ENTER_STATE), g_ca_wizard.state);
                g_app_state = APP_STATE_CA_WIZARD_STATE;
                render_current_state(false);
            }
            break;

        // CA Wizard: State/Province (optional)
        case APP_STATE_CA_WIZARD_STATE:
            if (key >= '0' && key <= '9') {
                view_t9_input_key(&g_t9_input, key);
                render_current_state(true);
            } else if (key == 'N') {
                if (g_t9_input.len > 0) {
                    view_t9_input_backspace(&g_t9_input);
                    render_current_state(true);
                } else {
                    // Back to Locality
                    view_t9_input_init(&g_t9_input, i18n_str(STR_CA_ENTER_LOCALITY), g_ca_wizard.locality);
                    g_app_state = APP_STATE_CA_WIZARD_LOCALITY;
                    render_current_state(false);
                }
            } else if (key == 'Y') {
                strncpy(g_ca_wizard.state, view_t9_input_get_text(&g_t9_input), CA_FIELD_MAX_LEN - 1);
                // Next: Validity selection
                view_list_screen_init(&g_ca_validity_menu, i18n_str(STR_CA_VALIDITY_YEARS),
                    g_ca_validity_items, 6);
                // Pre-select 10 years (index 2)
                g_ca_validity_menu.selection = 2;
                g_app_state = APP_STATE_CA_WIZARD_VALIDITY;
                render_current_state(false);
            }
            break;

        // CA Wizard: Validity Years selection
        case APP_STATE_CA_WIZARD_VALIDITY:
            if (key == '2') {
                view_list_screen_navigate(&g_ca_validity_menu, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_ca_validity_menu, true);
                render_current_state(true);
            } else if (key == 'Y' || key == '5') {
                // Get selected validity
                uint8_t sel = view_list_screen_get_selection(&g_ca_validity_menu);
                static const uint8_t validity_values[] = {1, 5, 10, 15, 20, 25};
                g_ca_wizard.validity_years = validity_values[sel];

                // Generate CA with wizard data
                view_info_screen_init(&g_info_view, i18n_str(STR_CA_GENERATE),
                    i18n_str(STR_CA_GENERATING));
                g_app_state = APP_STATE_CA_GENERATING;
                render_current_state(false);

                // Build DN string from wizard data
                char dn[256];
                int pos = 0;
                pos += snprintf(dn + pos, sizeof(dn) - pos, "CN=%s", g_ca_wizard.cn);
                if (g_ca_wizard.org[0]) {
                    pos += snprintf(dn + pos, sizeof(dn) - pos, ",O=%s", g_ca_wizard.org);
                }
                if (g_ca_wizard.ou[0]) {
                    pos += snprintf(dn + pos, sizeof(dn) - pos, ",OU=%s", g_ca_wizard.ou);
                }
                if (g_ca_wizard.country[0]) {
                    pos += snprintf(dn + pos, sizeof(dn) - pos, ",C=%s", g_ca_wizard.country);
                }
                if (g_ca_wizard.locality[0]) {
                    pos += snprintf(dn + pos, sizeof(dn) - pos, ",L=%s", g_ca_wizard.locality);
                }
                if (g_ca_wizard.state[0]) {
                    pos += snprintf(dn + pos, sizeof(dn) - pos, ",ST=%s", g_ca_wizard.state);
                }

                // Generate CA
                if (ca_setup(dn)) {
                    view_toast_success(i18n_str(STR_CA_GENERATED), 1500);
                } else {
                    view_toast_error(i18n_str(STR_CA_GENERATE_FAILED), 1500);
                }
                build_ca_menu();
                view_list_screen_init(&g_ca_menu, i18n_str(STR_CA_MENU), g_ca_items, CA_IDX_COUNT);
                g_app_state = APP_STATE_CA_MENU;
                render_current_state(false);
            } else if (key == 'N') {
                // Back to State
                view_t9_input_init(&g_t9_input, i18n_str(STR_CA_ENTER_STATE), g_ca_wizard.state);
                g_app_state = APP_STATE_CA_WIZARD_STATE;
                render_current_state(false);
            }
            break;
#endif
    }
}
