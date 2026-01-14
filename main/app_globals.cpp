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
view_ip_input_t g_ip_input;
view_qr_code_t g_qr_view;

#if FEATURE_TOTP
view_list_screen_t g_totp_list;
view_list_item_t g_totp_items[TOTP_MAX_ACCOUNTS];
uint8_t g_totp_sort_map[TOTP_MAX_ACCOUNTS];  // Maps display index -> store index
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
    {"6 Digits", VIEW_LIST_ICON_NONE, false},
    {"4 Digits", VIEW_LIST_ICON_NONE, false},
    {"8 Digits", VIEW_LIST_ICON_NONE, false}
};
view_list_screen_t g_totp_period_menu;
view_list_item_t g_totp_period_items[] = {
    {"30 Seconds", VIEW_LIST_ICON_NONE, false},
    {"60 Seconds", VIEW_LIST_ICON_NONE, false}
};
view_list_screen_t g_totp_algo_menu;
view_list_item_t g_totp_algo_items[] = {
    {"SHA-1", VIEW_LIST_ICON_NONE, false},
    {"SHA-256", VIEW_LIST_ICON_NONE, false},
    {"SHA-512", VIEW_LIST_ICON_NONE, false}
};
#endif

#if FEATURE_PASSWORD
view_list_screen_t g_password_list;
EXT_RAM_BSS_ATTR view_list_item_t g_password_items[PASSWORD_MAX_ENTRIES];
EXT_RAM_BSS_ATTR uint16_t g_password_slots[PASSWORD_MAX_ENTRIES];
uint16_t g_password_count = 0;
uint16_t g_password_selected_slot = 0;
char g_password_detail_text[512];
char g_password_preview_msg[32];

view_context_menu_t g_password_context_menu;
const view_context_item_t g_password_context_items[] = {
    {"View", 1},
    {"Send", 2},
    {"Edit", 3},
    {"Delete", 4},
    {"Add New", 5},
    {"Cancel", 0}
};

password_wizard_t g_password_wizard;
#endif

#if FEATURE_FIDO2
view_list_screen_t g_fido_list;
view_list_item_t g_fido_items[FIDO2_MAX_CREDENTIALS];
uint8_t g_fido_sort_map[FIDO2_MAX_CREDENTIALS];  // Maps display index -> store index
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
view_wifi_list_t g_wifi_list;
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
    {"WPA2 Personal", VIEW_LIST_ICON_NONE, false},
    {"WPA/WPA2", VIEW_LIST_ICON_NONE, false},
    {"WPA3 Personal", VIEW_LIST_ICON_NONE, false},
    {"WPA Personal", VIEW_LIST_ICON_NONE, false},
    {"Open", VIEW_LIST_ICON_NONE, false},
    {"WEP", VIEW_LIST_ICON_NONE, false}
};

// WiFi IP mode selection
view_list_screen_t g_wifi_ip_menu;
view_list_item_t g_wifi_ip_items[2];

// Tools menu
view_list_screen_t g_tools_menu;
view_list_item_t g_tools_items[5];
uint8_t g_tools_item_count = 0;

// Tools WiFi submenu
view_list_screen_t g_tools_wifi_menu;
view_list_item_t g_tools_wifi_items[4];  // Connect, Setup, Details, Disconnect

#if FEATURE_BLE_BADGE
// Remote Badge menus
view_list_screen_t g_remote_badge_menu;
view_list_item_t g_remote_badge_items[REMOTE_BADGE_IDX_COUNT];
// vCards submenu
view_list_screen_t g_vcard_submenu;
view_list_item_t g_vcard_submenu_items[VCARD_SUB_IDX_COUNT];
// Broadcast submenu
view_list_screen_t g_broadcast_submenu;
view_list_item_t g_broadcast_submenu_items[BROADCAST_SUB_IDX_COUNT];
// Settings submenu
view_list_screen_t g_broadcast_settings_menu;
view_list_item_t g_broadcast_settings_items[SETTINGS_SUB_IDX_COUNT];
// vCard list and state
view_list_screen_t g_vcard_list;
EXT_RAM_BSS_ATTR view_list_item_t g_vcard_list_items[VCARD_MAX_CARDS + 1];
view_context_menu_t g_vcard_context_menu;
view_context_item_t g_vcard_context_items[3];
view_list_screen_t g_vcard_field_menu;
view_list_item_t g_vcard_field_items[12];  // Field categories + Save
// Phone type submenu
view_list_screen_t g_phone_type_menu;
view_list_item_t g_phone_type_items[PHONE_TYPE_IDX_COUNT];
// IMPP type submenu
view_list_screen_t g_impp_type_menu;
view_list_item_t g_impp_type_items[IMPP_TYPE_IDX_COUNT];
// Address type submenu
view_list_screen_t g_address_type_menu;
view_list_item_t g_address_type_items[ADDRESS_TYPE_IDX_COUNT];
view_list_screen_t g_vcard_nearby_list;
view_list_item_t g_vcard_nearby_items[16];
ble_badge_peer_t g_vcard_nearby_peers[16];
uint16_t g_vcard_nearby_count = 0;
vcard_editor_t g_vcard_editor = {};
app_state_t g_vcard_nearby_return_state = APP_STATE_LOCK_SCREEN;
uint32_t g_vcard_nearby_alert_until = 0;
app_state_t g_ble_pairing_return_state = APP_STATE_LOCK_SCREEN;
uint32_t g_ble_pairing_passkey = 0;
#endif

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
    {"1 Year", VIEW_LIST_ICON_NONE, false},
    {"5 Years", VIEW_LIST_ICON_NONE, false},
    {"10 Years", VIEW_LIST_ICON_NONE, false},
    {"15 Years", VIEW_LIST_ICON_NONE, false},
    {"20 Years", VIEW_LIST_ICON_NONE, false},
    {"25 Years", VIEW_LIST_ICON_NONE, false}
};
#endif
