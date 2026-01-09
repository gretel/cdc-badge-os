#include "app_globals.h"

#include "esp_attr.h"

// App state
app_state_t g_app_state = APP_STATE_LOCK_SCREEN;

// View instances
view_lock_screen_t g_lock_screen;
view_pin_entry_t g_pin_entry;
view_list_screen_t g_main_menu;
view_list_screen_t g_settings_menu;
view_list_screen_t g_badge_texts_menu;
view_info_screen_t g_info_view;
view_slider_t g_slider;
view_t9_input_t g_t9_input;
view_qr_code_t g_qr_view;

#if FEATURE_TOTP
view_list_screen_t g_totp_list;
view_list_item_t g_totp_items[TOTP_MAX_ACCOUNTS];
view_totp_code_t g_totp_code;
uint8_t g_totp_selected_index = 0;

// Context menu for TOTP list
view_context_menu_t g_totp_context_menu;
const view_context_item_t g_totp_context_items[] = {
    {"Show Code", 1},
    {"Edit", 2},
    {"Delete", 3},
    {"Add New", 4},
    {"Cancel", 0}
};

// TOTP add/edit wizard data
static_assert(sizeof(totp_wizard_t) > 0, "totp_wizard_t must be defined");
totp_wizard_t g_totp_wizard;

// TOTP add wizard selection views
view_list_screen_t g_totp_digits_menu;
view_list_item_t g_totp_digits_items[] = {
    {"6 Digits"},
    {"4 Digits"},
    {"8 Digits"}
};
view_list_screen_t g_totp_period_menu;
view_list_item_t g_totp_period_items[] = {
    {"30 Seconds"},
    {"60 Seconds"}
};
view_list_screen_t g_totp_algo_menu;
view_list_item_t g_totp_algo_items[] = {
    {"SHA-1"},
    {"SHA-256"},
    {"SHA-512"}
};
#endif

#if FEATURE_FIDO2
view_list_screen_t g_fido_list;
view_list_item_t g_fido_items[FIDO2_MAX_CREDENTIALS];
uint8_t g_fido_selected_index = 0;
char g_fido_detail_text[256];

// Context menu for FIDO2 list
view_context_menu_t g_context_menu;
// Dynamic menu items (allows adding BLE toggle when feature is enabled)
view_context_item_t g_fido_context_items_buf[5];
uint8_t g_fido_context_items_count = 0;

// FIDO2 user presence prompt state
SemaphoreHandle_t g_fido_prompt_sem = NULL;
volatile fido2_user_presence_result_t g_fido_prompt_result = FIDO2_UP_PENDING;
char g_fido_prompt_rp_id[FIDO2_RP_ID_MAX_LEN];
fido2_action_t g_fido_prompt_action;
app_state_t g_fido_return_state = APP_STATE_LOCK_SCREEN;
bool g_fido_was_locked = false;
#endif

// WiFi state and views
view_list_screen_t g_wifi_list;
view_list_item_t g_wifi_items[WIFI_MAX_NETWORKS + 1];  // +1 for "Add Manual"
uint8_t g_wifi_selected_index = 0;
view_context_menu_t g_wifi_context_menu;
const view_context_item_t g_wifi_context_items[] = {
    {"Connect", 1},
    {"Add Manual", 2},
    {"Cancel", 0}
};

// WiFi add/edit wizard data
wifi_wizard_t g_wifi_wizard;

// WiFi auth mode selection
view_list_screen_t g_wifi_auth_menu;
view_list_item_t g_wifi_auth_items[] = {
    {"WPA2 Personal"},
    {"WPA/WPA2"},
    {"WPA3 Personal"},
    {"WPA Personal"},
    {"Open"},
    {"WEP"}
};

// WiFi IP mode selection
view_list_screen_t g_wifi_ip_menu;
view_list_item_t g_wifi_ip_items[2];

// Tools menu
view_list_screen_t g_tools_menu;
view_list_item_t g_tools_items[2];

// Tools WiFi submenu
view_list_screen_t g_tools_wifi_menu;
view_list_item_t g_tools_wifi_items[4];  // Connect, Setup, Details, Disconnect

// WiFi connection timing
uint32_t g_wifi_connect_start = 0;
uint32_t g_wifi_scan_start = 0;

// NTP sync state: 0=idle, 1=connecting WiFi, 2=syncing NTP
uint8_t g_ntp_sync_phase = 0;
// WiFi session ID for NTP sync (0 = no session, >0 = active session)
uint8_t g_ntp_wifi_session = 0;

// Main Menu items (built dynamically for i18n)
view_list_item_t g_menu_items[8];
uint8_t g_menu_item_count = 0;

// Settings Menu items (built dynamically for i18n)
view_list_item_t g_settings_items[SETTINGS_IDX_COUNT];

// Badge Texts submenu items (built dynamically for i18n)
view_list_item_t g_badge_texts_items[BADGE_IDX_COUNT];

// PIN change state
char g_new_pin[VIEW_PIN_MAX_LEN + 1] = {0};

// Date/Time input views
view_date_input_t g_date_input;
view_time_input_t g_time_input;

// Language menu
view_list_screen_t g_language_menu;
view_list_item_t g_language_items[LANG_COUNT];

// Selftest buffer (static to persist across renders)
char g_selftest_buf[512];

// Track last minute for display refresh
int g_last_minute = -1;

// Backlight forced on (user pressed key 1 on lock screen)
bool g_backlight_forced_on = false;

// Security lockout end time
uint32_t g_lockout_end_ms = 0;

// RTC memory: survives deep sleep
RTC_DATA_ATTR bool g_in_deep_sleep_mode = false;

// Light sleep configuration
bool g_light_sleep_configured = false;
uint32_t g_lock_screen_entered_ms = 0;

// Auto-lock: return to lock screen after inactivity
uint32_t g_last_activity_ms = 0;

// Store init results for selftest display
hw_status_t g_hw_status = {false, false, false, false, false};

// BLE UART state
#if FEATURE_BLE_UART
bool g_ble_enabled = false;
#endif

// Lock screen quick menu
view_context_menu_t g_lock_quick_menu;
view_context_item_t g_lock_quick_items[4];

// Main menu quick menu (key 3)
view_context_menu_t g_main_quick_menu;
view_context_item_t g_main_quick_items[4];

// CA (Certificate Authority) state
#if FEATURE_CA
view_list_screen_t g_ca_menu;
view_list_item_t g_ca_items[CA_IDX_COUNT];
char g_ca_detail_text[512];
ca_wizard_t g_ca_wizard;
view_list_screen_t g_ca_validity_menu;
view_list_item_t g_ca_validity_items[] = {
    {"1 Year"},
    {"5 Years"},
    {"10 Years"},
    {"15 Years"},
    {"20 Years"},
    {"25 Years"}
};
#endif

