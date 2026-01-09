#pragma once

#include <cstddef>
#include <cstdint>

#include "feature_flags.h"
#include "i18n.h"
#include "system_status.h"
#include "views.h"
#include "wifi_manager.h"

#if FEATURE_TOTP
#include "totp_store.h"
#endif

#if FEATURE_FIDO2
#include "fido2.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#endif

// App states
// Keep ordering stable for state transitions and rendering.
typedef enum {
    APP_STATE_LOCK_SCREEN,
    APP_STATE_PIN_ENTRY,
    APP_STATE_MAIN_MENU,
    APP_STATE_SETTINGS_MENU,
    APP_STATE_BADGE_TEXTS_MENU,
    APP_STATE_INFO_DEMO,
    APP_STATE_SLIDER_BRIGHTNESS,
    APP_STATE_SLIDER_TIMEZONE,
    APP_STATE_T9_DEMO,
    APP_STATE_SELFTEST,
    // PIN Change flow
    APP_STATE_PIN_CHANGE_OLD,
    APP_STATE_PIN_CHANGE_NEW,
    APP_STATE_PIN_CHANGE_CONFIRM,
    // Badge Text editing
    APP_STATE_BADGE_EDIT_NAME,
    APP_STATE_BADGE_EDIT_INFO,
    APP_STATE_BADGE_EDIT_INFO2,
    // Date/Time setting
    APP_STATE_SET_DATE,
    APP_STATE_SET_TIME,
    // Language selection
    APP_STATE_LANGUAGE,
    // Security lockout (after too many failed PIN attempts)
    APP_STATE_LOCKOUT,
#if FEATURE_TOTP
    // TOTP states
    APP_STATE_TOTP_LIST,
    APP_STATE_TOTP_CODE,
    // TOTP add wizard states
    APP_STATE_TOTP_ADD_NAME,
    APP_STATE_TOTP_ADD_SECRET,
    APP_STATE_TOTP_ADD_ISSUER,
    APP_STATE_TOTP_ADD_DIGITS,
    APP_STATE_TOTP_ADD_ALGO,
    APP_STATE_TOTP_ADD_PERIOD,
#endif
#if FEATURE_FIDO2
    // FIDO2 states
    APP_STATE_FIDO_LIST,
    APP_STATE_FIDO_DETAIL,
    // FIDO2 user presence prompt
    APP_STATE_FIDO_PROMPT,
    APP_STATE_FIDO_PROMPT_PIN,  // PIN entry before approving when locked
#endif
    // WiFi states
    APP_STATE_WIFI_SCAN,          // Scanning for networks
    APP_STATE_WIFI_LIST,          // Network list + context menu
    APP_STATE_WIFI_ADD_SSID,      // T9: SSID input
    APP_STATE_WIFI_ADD_PASSWORD,  // T9: Password input
    APP_STATE_WIFI_ADD_AUTH,      // Auth mode selection
    APP_STATE_WIFI_ADD_IP_MODE,   // DHCP or Static
    APP_STATE_WIFI_ADD_STATIC_IP, // IP input (numpad)
    APP_STATE_WIFI_CONNECTING,    // Connection in progress
    APP_STATE_WIFI_DETAILS,       // Show WiFi details
    // Tools menu states
    APP_STATE_TOOLS_MENU,
    APP_STATE_TOOLS_NTP_SYNC,
    APP_STATE_TOOLS_WIFI_MENU,
} app_state_t;

// Menu item indices (adjusted for feature flags)
enum {
#if FEATURE_TOTP
    MENU_IDX_TOTP,
#endif
#if FEATURE_FIDO2
    MENU_IDX_FIDO2,
#endif
    MENU_IDX_TOOLS,
    MENU_IDX_SELFTEST,
    MENU_IDX_SETTINGS,
    MENU_IDX_LOCK
};

// Settings Menu indices
enum {
    SETTINGS_IDX_CHANGE_PIN,
    SETTINGS_IDX_BRIGHTNESS,
    SETTINGS_IDX_TIMEZONE,
    SETTINGS_IDX_BADGE_TEXTS,
    SETTINGS_IDX_SET_DATE,
    SETTINGS_IDX_SET_TIME,
    SETTINGS_IDX_WIFI_SETUP,
    SETTINGS_IDX_LANGUAGE,
    SETTINGS_IDX_BACK,
    SETTINGS_IDX_COUNT
};

// Badge Texts submenu indices
enum {
    BADGE_IDX_NAME,
    BADGE_IDX_INFO,
    BADGE_IDX_INFO2,
    BADGE_IDX_BACK,
    BADGE_IDX_COUNT
};

#if FEATURE_TOTP
typedef struct {
    char name[TOTP_NAME_LEN];
    char secret[64];  // Base32 encoded
    char issuer[TOTP_ISSUER_LEN];
    uint8_t digits;   // 4, 6, or 8
    uint8_t algorithm; // 0=SHA-1, 1=SHA-256, 2=SHA-512
    uint32_t period;  // 30 or 60
    bool edit_mode;   // true = editing existing, false = adding new
    uint8_t edit_index; // index being edited (if edit_mode)
} totp_wizard_t;
#endif

typedef struct {
    char ssid[WIFI_SSID_MAX_LEN];
    char password[WIFI_PASSWORD_MAX_LEN];
    wifi_auth_mode_t auth_mode;
    bool use_dhcp;
    char static_ip[16];  // "xxx.xxx.xxx.xxx"
    bool from_scan;      // true = selected from scan list
} wifi_wizard_t;

// Global app state
extern app_state_t g_app_state;

// View instances
extern view_lock_screen_t g_lock_screen;
extern view_pin_entry_t g_pin_entry;
extern view_list_screen_t g_main_menu;
extern view_list_screen_t g_settings_menu;
extern view_list_screen_t g_badge_texts_menu;
extern view_info_screen_t g_info_view;
extern view_slider_t g_slider;
extern view_t9_input_t g_t9_input;

#if FEATURE_TOTP
extern view_list_screen_t g_totp_list;
extern view_list_item_t g_totp_items[TOTP_MAX_ACCOUNTS];
extern view_totp_code_t g_totp_code;
extern uint8_t g_totp_selected_index;
extern view_context_menu_t g_totp_context_menu;
extern const view_context_item_t g_totp_context_items[];
extern totp_wizard_t g_totp_wizard;
extern view_list_screen_t g_totp_digits_menu;
extern view_list_item_t g_totp_digits_items[3];
extern view_list_screen_t g_totp_period_menu;
extern view_list_item_t g_totp_period_items[2];
extern view_list_screen_t g_totp_algo_menu;
extern view_list_item_t g_totp_algo_items[3];
#endif

#if FEATURE_FIDO2
extern view_list_screen_t g_fido_list;
extern view_list_item_t g_fido_items[FIDO2_MAX_CREDENTIALS];
extern uint8_t g_fido_selected_index;
extern char g_fido_detail_text[256];
extern view_context_menu_t g_context_menu;
extern const view_context_item_t g_fido_context_items[];
extern SemaphoreHandle_t g_fido_prompt_sem;
extern volatile fido2_user_presence_result_t g_fido_prompt_result;
extern char g_fido_prompt_rp_id[FIDO2_RP_ID_MAX_LEN];
extern fido2_action_t g_fido_prompt_action;
extern app_state_t g_fido_return_state;
extern bool g_fido_was_locked;
#endif

// WiFi state and views
extern view_list_screen_t g_wifi_list;
extern view_list_item_t g_wifi_items[WIFI_MAX_NETWORKS + 1];  // +1 for "Add Manual"
extern uint8_t g_wifi_selected_index;
extern view_context_menu_t g_wifi_context_menu;
extern const view_context_item_t g_wifi_context_items[];
extern wifi_wizard_t g_wifi_wizard;
extern view_list_screen_t g_wifi_auth_menu;
extern view_list_item_t g_wifi_auth_items[6];
extern view_list_screen_t g_wifi_ip_menu;
extern view_list_item_t g_wifi_ip_items[2];

// Tools menu
extern view_list_screen_t g_tools_menu;
extern view_list_item_t g_tools_items[2];

// Tools WiFi submenu
extern view_list_screen_t g_tools_wifi_menu;
extern view_list_item_t g_tools_wifi_items[3];

// WiFi connection timing
extern uint32_t g_wifi_connect_start;
extern uint32_t g_wifi_scan_start;

// NTP sync state: 0=idle, 1=connecting WiFi, 2=syncing NTP
extern uint8_t g_ntp_sync_phase;

// Main Menu items (built dynamically for i18n)
extern view_list_item_t g_menu_items[8];
extern uint8_t g_menu_item_count;

// Settings Menu items (built dynamically for i18n)
extern view_list_item_t g_settings_items[SETTINGS_IDX_COUNT];

// Badge Texts submenu items (built dynamically for i18n)
extern view_list_item_t g_badge_texts_items[BADGE_IDX_COUNT];

// PIN change state
extern char g_new_pin[VIEW_PIN_MAX_LEN + 1];

// Date/Time input views
extern view_date_input_t g_date_input;
extern view_time_input_t g_time_input;

// Language menu
extern view_list_screen_t g_language_menu;
extern view_list_item_t g_language_items[LANG_COUNT];

// Selftest buffer (static to persist across renders)
extern char g_selftest_buf[512];

// Track last minute for display refresh
extern int g_last_minute;

// Backlight forced on (user pressed key 1 on lock screen)
extern bool g_backlight_forced_on;

// Security lockout end time
extern uint32_t g_lockout_end_ms;

// RTC memory: survives deep sleep
extern bool g_in_deep_sleep_mode;

// Light sleep configuration
extern bool g_light_sleep_configured;
extern uint32_t g_lock_screen_entered_ms;

// Auto-lock: return to lock screen after inactivity
extern uint32_t g_last_activity_ms;

// Hardware status for selftest
extern hw_status_t g_hw_status;
