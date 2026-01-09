#include "app_render.h"

#include "badge_settings.h"
#include "cdc_rtc.h"
#include "cdc_time.h"
#include "i18n.h"
#include "power_management.h"

#if FEATURE_BLE_UART
#include "ble_uart.h"
#endif

#include <cstdio>
#include <cstring>

// ============================================================================
// Lock Screen Data
// ============================================================================

void update_lock_screen_data(void) {
    // Update static text
    strncpy(g_lock_screen.name, badge_settings_get_name(), sizeof(g_lock_screen.name));
    strncpy(g_lock_screen.info, badge_settings_get_info(), sizeof(g_lock_screen.info));
    strncpy(g_lock_screen.info2, badge_settings_get_info2(), sizeof(g_lock_screen.info2));

    // Update dynamic data
    g_lock_screen.battery_percent = power_get_battery_percent();
    g_lock_screen.charging = power_is_charging();
    g_lock_screen.show_lock_icon = true;
    g_lock_screen.backlight_on = g_backlight_forced_on;

    // Update status icons (USB detected via PG_STAT, not USB stack)
    g_lock_screen.status_icons = 0;
    if (power_is_usb_connected()) {
        g_lock_screen.status_icons |= ICON_USB;
    }
    if (wifi_manager_get_state() == WIFI_STATE_CONNECTED) {
        g_lock_screen.status_icons |= ICON_WIFI;
    }
#if FEATURE_BLE_UART
    if (g_ble_enabled) {
        g_lock_screen.status_icons |= ICON_BLE;
    }
#endif

    if (cdc_rtc_is_time_set()) {
        cdc_rtc_get_time_str(g_lock_screen.clock, sizeof(g_lock_screen.clock));
        // Compact date format DD.MM
        struct tm timeinfo;
        cdc_rtc_get_time(&timeinfo);
        snprintf(g_lock_screen.date, sizeof(g_lock_screen.date), "%02u.%02u",
                 (unsigned)(timeinfo.tm_mday & 0xFF), (unsigned)((timeinfo.tm_mon + 1) & 0xFF));
    } else {
        strncpy(g_lock_screen.clock, "--:--", sizeof(g_lock_screen.clock));
        g_lock_screen.date[0] = '\0';
    }
}

// ============================================================================
// State Rendering
// ============================================================================

void render_current_state(bool partial) {
    switch (g_app_state) {
        case APP_STATE_LOCK_SCREEN:
            view_lock_screen_render(&g_lock_screen, partial);
            break;

        case APP_STATE_LOCK_QUICK_MENU:
            // Quick menu is rendered as overlay by context_menu_show/render
            view_context_menu_render(&g_lock_quick_menu);
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

        case APP_STATE_MAIN_QUICK_MENU:
            // Quick menu is rendered as overlay by context_menu_show/render
            view_context_menu_render(&g_main_quick_menu);
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
        case APP_STATE_SLIDER_TIMEZONE:
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

        case APP_STATE_SET_DATE:
            view_date_input_render(&g_date_input, partial);
            break;

        case APP_STATE_SET_TIME:
            view_time_input_render(&g_time_input, partial);
            break;

        case APP_STATE_LANGUAGE:
            view_list_screen_render(&g_language_menu, partial);
            break;

        case APP_STATE_LOCKOUT: {
            // Render lockout screen with countdown
            uint32_t remaining_ms = 0;
            if (g_lockout_end_ms > millis()) {
                remaining_ms = g_lockout_end_ms - millis();
            }
            uint32_t remaining_sec = (remaining_ms + 999) / 1000;  // Round up

            char lockout_text[128];
            snprintf(lockout_text, sizeof(lockout_text),
                     "%s\n\n%lu s",
                     i18n_str(STR_LOCKED_OUT),
                     (unsigned long)remaining_sec);
            view_info_screen_init(&g_info_view, i18n_str(STR_LOCKED_OUT), lockout_text);
            view_info_screen_render(&g_info_view, partial);
            break;
        }

#if FEATURE_TOTP
        case APP_STATE_TOTP_LIST:
            view_list_screen_render(&g_totp_list, partial);
            break;

        case APP_STATE_TOTP_CODE:
            view_totp_code_render(&g_totp_code, partial);
            break;

        // TOTP Add Wizard states
        case APP_STATE_TOTP_ADD_NAME:
        case APP_STATE_TOTP_ADD_SECRET:
        case APP_STATE_TOTP_ADD_ISSUER:
            view_t9_input_render(&g_t9_input, partial);
            break;

        case APP_STATE_TOTP_ADD_DIGITS:
            view_list_screen_render(&g_totp_digits_menu, partial);
            break;

        case APP_STATE_TOTP_ADD_PERIOD:
            view_list_screen_render(&g_totp_period_menu, partial);
            break;

        case APP_STATE_TOTP_ADD_ALGO:
            view_list_screen_render(&g_totp_algo_menu, partial);
            break;
#endif

#if FEATURE_FIDO2
        case APP_STATE_FIDO_LIST:
            view_list_screen_render(&g_fido_list, partial);
            break;

        case APP_STATE_FIDO_DETAIL:
        case APP_STATE_FIDO_PROMPT:
            view_info_screen_render(&g_info_view, partial);
            break;

        case APP_STATE_FIDO_PROMPT_PIN:
            view_pin_entry_render(&g_pin_entry, partial);
            break;
#endif

        // WiFi states
        case APP_STATE_WIFI_SCAN:
            view_info_screen_render(&g_info_view, partial);
            break;

        case APP_STATE_WIFI_LIST:
            if (view_context_menu_is_visible(&g_wifi_context_menu)) {
                view_context_menu_render(&g_wifi_context_menu);
            } else {
                view_list_screen_render(&g_wifi_list, partial);
            }
            break;

        case APP_STATE_WIFI_ADD_SSID:
        case APP_STATE_WIFI_ADD_PASSWORD:
            view_t9_input_render(&g_t9_input, partial);
            break;

        case APP_STATE_WIFI_ADD_AUTH:
            view_list_screen_render(&g_wifi_auth_menu, partial);
            break;

        case APP_STATE_WIFI_ADD_IP_MODE:
            view_list_screen_render(&g_wifi_ip_menu, partial);
            break;

        case APP_STATE_WIFI_ADD_STATIC_IP:
            view_ip_input_render(&g_ip_input, partial);
            break;

        case APP_STATE_WIFI_ADD_GATEWAY:
        case APP_STATE_WIFI_ADD_NETMASK:
            view_ip_input_render(&g_ip_input, partial);
            break;

        case APP_STATE_WIFI_CONNECTING:
            view_info_screen_render(&g_info_view, partial);
            break;

        case APP_STATE_WIFI_DETAILS:
            view_info_screen_render(&g_info_view, partial);
            break;

        // Tools states
        case APP_STATE_TOOLS_MENU:
            view_list_screen_render(&g_tools_menu, partial);
            break;

        case APP_STATE_TOOLS_NTP_SYNC:
            view_info_screen_render(&g_info_view, partial);
            break;

        case APP_STATE_TOOLS_WIFI_MENU:
            view_list_screen_render(&g_tools_wifi_menu, partial);
            break;

#if FEATURE_CA
        case APP_STATE_CA_MENU:
            view_list_screen_render(&g_ca_menu, partial);
            break;

        case APP_STATE_CA_DETAILS:
        case APP_STATE_CA_GENERATING:
        case APP_STATE_CA_RESET_CONFIRM:
            view_info_screen_render(&g_info_view, partial);
            break;

        case APP_STATE_CA_QR_CODE:
            view_qr_code_render(&g_qr_view, partial);
            break;

        // CA Wizard: T9 input states
        case APP_STATE_CA_WIZARD_CN:
        case APP_STATE_CA_WIZARD_ORG:
        case APP_STATE_CA_WIZARD_OU:
        case APP_STATE_CA_WIZARD_COUNTRY:
        case APP_STATE_CA_WIZARD_LOCALITY:
        case APP_STATE_CA_WIZARD_STATE:
            view_t9_input_render(&g_t9_input, partial);
            break;

        // CA Wizard: Validity selection
        case APP_STATE_CA_WIZARD_VALIDITY:
            view_list_screen_render(&g_ca_validity_menu, partial);
            break;
#endif
    }
}
