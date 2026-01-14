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
#if FEATURE_PASSWORD
#include "password_store.h"
#endif

#if FEATURE_FIDO2
#include "fido2.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#endif

#if FEATURE_BLE_BADGE
#include "ble_badge.h"
#include "vcard_store.h"
#endif

// App states
// Keep ordering stable for state transitions and rendering.
typedef enum {
    APP_STATE_LOCK_SCREEN,
    APP_STATE_LOCK_QUICK_MENU,  // Quick menu overlay on lock screen (Light, Sleep only)
    APP_STATE_PIN_ENTRY,
    APP_STATE_MAIN_MENU,
    APP_STATE_MAIN_QUICK_MENU,  // Quick menu overlay on main menu (key 3)
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
#if FEATURE_PASSWORD
    // Password vault states
    APP_STATE_PASSWORD_LIST,
    APP_STATE_PASSWORD_DETAIL,
    APP_STATE_PASSWORD_PREVIEW,
    // Password add/edit wizard states
    APP_STATE_PASSWORD_ADD_NAME,
    APP_STATE_PASSWORD_ADD_USERNAME,
    APP_STATE_PASSWORD_ADD_URL,
    APP_STATE_PASSWORD_ADD_PASSWORD,
    APP_STATE_PASSWORD_ADD_NOTES,
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
    APP_STATE_WIFI_ADD_STATIC_IP, // IP input (numeric)
    APP_STATE_WIFI_ADD_GATEWAY,   // Gateway input (numeric)
    APP_STATE_WIFI_ADD_NETMASK,   // Netmask input (numeric)
    APP_STATE_WIFI_CONNECTING,    // Connection in progress
    APP_STATE_WIFI_DETAILS,       // Show WiFi details
    // Tools menu states
    APP_STATE_TOOLS_MENU,
    APP_STATE_TOOLS_NTP_SYNC,
    APP_STATE_TOOLS_WIFI_MENU,
#if FEATURE_BLE_BADGE
    // Remote Badge main menu
    APP_STATE_REMOTE_BADGE_MENU,     // Main menu: vCards, Broadcast, Settings
    // vCards submenu
    APP_STATE_VCARD_SUBMENU,         // vCards submenu
    APP_STATE_VCARD_ADD_FIRST,       // Edit own vCard: First name
    APP_STATE_VCARD_ADD_LAST,        // Edit own vCard: Last name
    APP_STATE_VCARD_ADD_NOTE,        // Edit own vCard: Note
    APP_STATE_VCARD_FIELD_MENU,      // Field type selection (main categories)
    APP_STATE_VCARD_PHONE_TYPE,      // Phone type submenu (Landline/Mobile/Pager)
    APP_STATE_VCARD_IMPP_TYPE,       // IMPP type submenu (Telegram/Signal/WhatsApp/etc.)
    APP_STATE_VCARD_ADDRESS_TYPE,    // Address type submenu (Home/Work)
    APP_STATE_VCARD_FIELD_VALUE,     // Field value entry
    APP_STATE_VCARD_EXCHANGE,        // Exchange mode (scan & exchange)
    APP_STATE_VCARD_LIST,            // vCard list with context menu
    APP_STATE_VCARD_CONTEXT_MENU,    // Context menu for vCard
    APP_STATE_VCARD_VIEW,            // View vCard details
    APP_STATE_VCARD_QR,              // Show vCard QR code
    // Broadcast submenu
    APP_STATE_BROADCAST_SUBMENU,     // Broadcast submenu: Send, Receive toggles
    APP_STATE_VCARD_NEARBY_LIST,     // Nearby badges list
    APP_STATE_VCARD_SEND_PROGRESS,   // Send progress
    APP_STATE_VCARD_NEARBY_ALERT,    // Nearby alert
    // Settings submenu
    APP_STATE_BROADCAST_SETTINGS,    // Settings submenu: Intervals
    APP_STATE_VCARD_ADV_INTERVAL,    // Send interval slider
    APP_STATE_VCARD_SCAN_INTERVAL,   // Scan interval slider
    // BLE Pairing
    APP_STATE_BLE_PAIRING_CONFIRM,
    APP_STATE_BLE_PAIRING_PASSKEY,
    APP_STATE_BLE_PAIRING_DISPLAY,
#endif
#if FEATURE_CA
    // CA states
    APP_STATE_CA_MENU,
    APP_STATE_CA_DETAILS,
    APP_STATE_CA_GENERATING,
    APP_STATE_CA_QR_CODE,
    APP_STATE_CA_RESET_CONFIRM,
    // CA Wizard states
    APP_STATE_CA_WIZARD_CN,
    APP_STATE_CA_WIZARD_ORG,
    APP_STATE_CA_WIZARD_OU,
    APP_STATE_CA_WIZARD_COUNTRY,
    APP_STATE_CA_WIZARD_LOCALITY,
    APP_STATE_CA_WIZARD_STATE,
    APP_STATE_CA_WIZARD_VALIDITY,
#endif
} app_state_t;

// Menu item indices (adjusted for feature flags)
enum {
#if FEATURE_TOTP
    MENU_IDX_TOTP,
#endif
#if FEATURE_PASSWORD
    MENU_IDX_PASSWORD,
#endif
#if FEATURE_FIDO2
    MENU_IDX_FIDO2,
#endif
#if FEATURE_CA
    MENU_IDX_CA,
#endif
#if FEATURE_BLE_BADGE
    MENU_IDX_REMOTE_BADGE,
#endif
    MENU_IDX_TOOLS,
    MENU_IDX_SETTINGS
    // Deep Sleep moved to lock screen quick menu
};

// Tools Menu indices (dynamic, order defined in build_tools_menu)
enum {
    TOOLS_IDX_WIFI,
#if FEATURE_BLE_UART
    TOOLS_IDX_BLE_SERIAL,
#endif
    TOOLS_IDX_NTP,
    TOOLS_IDX_SELFTEST,
    TOOLS_IDX_COUNT
};

// Settings Menu indices (WiFi moved to Tools > WiFi > Setup)
enum {
    SETTINGS_IDX_CHANGE_PIN,
    SETTINGS_IDX_BRIGHTNESS,
    SETTINGS_IDX_TIMEZONE,
    SETTINGS_IDX_BADGE_TEXTS,
    SETTINGS_IDX_SET_DATETIME,  // Combined Date/Time wizard
    SETTINGS_IDX_LANGUAGE,
    SETTINGS_IDX_COUNT
};

// Badge Texts submenu indices (no Back - use N key)
enum {
    BADGE_IDX_NAME,
    BADGE_IDX_INFO,
    BADGE_IDX_INFO2,
    BADGE_IDX_COUNT
};

// CA Menu indices (no Back - use N key, Generate/Reset at end)
#if FEATURE_CA
enum {
    CA_IDX_STATUS,
    CA_IDX_EXPORT,
    CA_IDX_QR_CODE,
    CA_IDX_GENERATE,  // Generate/Reset as last item
    CA_IDX_COUNT
};
#endif

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

#if FEATURE_PASSWORD
typedef struct {
    char name[PASSWORD_NAME_LEN];
    char username[PASSWORD_USERNAME_LEN];
    char url[PASSWORD_URL_LEN];
    char password[PASSWORD_MAX_LEN + 1];
    char notes[PASSWORD_NOTES_LEN + 1];
    bool edit_mode;
    uint16_t edit_slot;
} password_wizard_t;
#endif

#if FEATURE_CA
// CA Wizard data (X.509 DN fields + validity)
#define CA_FIELD_MAX_LEN 64
typedef struct {
    char cn[CA_FIELD_MAX_LEN];       // Common Name (required)
    char org[CA_FIELD_MAX_LEN];      // Organization (optional)
    char ou[CA_FIELD_MAX_LEN];       // Organizational Unit (optional)
    char country[3];                 // Country code (2 letters, e.g., "DE")
    char locality[CA_FIELD_MAX_LEN]; // City/Locality (optional)
    char state[CA_FIELD_MAX_LEN];    // State/Province (optional)
    uint8_t validity_years;          // 1-25 years
} ca_wizard_t;
#endif

typedef struct {
    char ssid[WIFI_SSID_MAX_LEN];
    char password[WIFI_PASSWORD_MAX_LEN];
    wifi_auth_mode_t auth_mode;
    bool use_dhcp;
    char static_ip[16];  // "xxx.xxx.xxx.xxx"
    char gateway[16];    // "xxx.xxx.xxx.xxx"
    char subnet[16];     // "xxx.xxx.xxx.xxx"
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
extern view_ip_input_t g_ip_input;
extern view_qr_code_t g_qr_view;

#if FEATURE_TOTP
extern view_list_screen_t g_totp_list;
extern view_list_item_t g_totp_items[TOTP_MAX_ACCOUNTS];
extern uint8_t g_totp_sort_map[TOTP_MAX_ACCOUNTS];  // Maps display index -> store index
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

#if FEATURE_PASSWORD
extern view_list_screen_t g_password_list;
extern view_list_item_t g_password_items[PASSWORD_MAX_ENTRIES];
extern uint16_t g_password_slots[PASSWORD_MAX_ENTRIES];
extern uint16_t g_password_count;
extern uint16_t g_password_selected_slot;
extern char g_password_detail_text[512];
extern char g_password_preview_msg[32];
extern view_context_menu_t g_password_context_menu;
extern const view_context_item_t g_password_context_items[];
extern password_wizard_t g_password_wizard;
#endif

#if FEATURE_FIDO2
extern view_list_screen_t g_fido_list;
extern view_list_item_t g_fido_items[FIDO2_MAX_CREDENTIALS];
extern uint8_t g_fido_sort_map[FIDO2_MAX_CREDENTIALS];  // Maps display index -> store index
extern uint8_t g_fido_selected_index;
extern char g_fido_detail_text[256];
extern view_context_menu_t g_context_menu;
extern view_context_item_t g_fido_context_items_buf[5];
extern uint8_t g_fido_context_items_count;
extern SemaphoreHandle_t g_fido_prompt_sem;
extern volatile fido2_user_presence_result_t g_fido_prompt_result;
extern char g_fido_prompt_rp_id[FIDO2_RP_ID_MAX_LEN];
extern fido2_action_t g_fido_prompt_action;
extern app_state_t g_fido_return_state;
extern bool g_fido_was_locked;
#endif

// WiFi state and views
extern view_wifi_list_t g_wifi_list;
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
extern view_list_item_t g_tools_items[5];
extern uint8_t g_tools_item_count;

// Tools WiFi submenu
extern view_list_screen_t g_tools_wifi_menu;
extern view_list_item_t g_tools_wifi_items[4];

#if FEATURE_BLE_BADGE
// Remote Badge main menu (3 submenus)
enum {
    REMOTE_BADGE_IDX_VCARDS,     // vCards submenu
    REMOTE_BADGE_IDX_BROADCAST,  // Broadcast submenu
    REMOTE_BADGE_IDX_SETTINGS,   // Settings submenu
    REMOTE_BADGE_IDX_COUNT
};

// vCards submenu indices
enum {
    VCARD_SUB_IDX_EDIT,          // Edit own vCard
    VCARD_SUB_IDX_EXCHANGE,      // Exchange mode
    VCARD_SUB_IDX_LIST,          // vCard list
    VCARD_SUB_IDX_COUNT
};

// Broadcast submenu indices
enum {
    BROADCAST_SUB_IDX_SEND,      // Beacon send toggle
    BROADCAST_SUB_IDX_RECEIVE,   // Beacon receive toggle
    BROADCAST_SUB_IDX_COUNT
};

// Settings submenu indices
enum {
    SETTINGS_SUB_IDX_SEND_INTERVAL,   // Send interval
    SETTINGS_SUB_IDX_SCAN_INTERVAL,   // Scan interval
    SETTINGS_SUB_IDX_COUNT
};

// Phone type submenu indices
enum {
    PHONE_TYPE_IDX_LANDLINE,
    PHONE_TYPE_IDX_MOBILE,
    PHONE_TYPE_IDX_BUSINESS,
    PHONE_TYPE_IDX_PAGER,
    PHONE_TYPE_IDX_COUNT
};

// IMPP type submenu indices
enum {
    IMPP_TYPE_IDX_TELEGRAM,
    IMPP_TYPE_IDX_SIGNAL,
    IMPP_TYPE_IDX_WHATSAPP,
    IMPP_TYPE_IDX_DISCORD,
    IMPP_TYPE_IDX_MATRIX,
    IMPP_TYPE_IDX_THREEMA,
    IMPP_TYPE_IDX_OTHER,
    IMPP_TYPE_IDX_COUNT
};

// Address type submenu indices
enum {
    ADDRESS_TYPE_IDX_HOME,
    ADDRESS_TYPE_IDX_WORK,
    ADDRESS_TYPE_IDX_COUNT
};

#define VCARD_EDITOR_MAX_EXTRAS 12
typedef struct {
    char first[32];
    char last[32];
    char note[128];
    uint8_t extra_type[VCARD_EDITOR_MAX_EXTRAS];
    char extra_value[VCARD_EDITOR_MAX_EXTRAS][64];
    uint8_t extra_count;
    uint8_t extra_edit_index;
} vcard_editor_t;

// Remote Badge menus
extern view_list_screen_t g_remote_badge_menu;
extern view_list_item_t g_remote_badge_items[REMOTE_BADGE_IDX_COUNT];
// vCards submenu
extern view_list_screen_t g_vcard_submenu;
extern view_list_item_t g_vcard_submenu_items[VCARD_SUB_IDX_COUNT];
// Broadcast submenu
extern view_list_screen_t g_broadcast_submenu;
extern view_list_item_t g_broadcast_submenu_items[BROADCAST_SUB_IDX_COUNT];
// Settings submenu
extern view_list_screen_t g_broadcast_settings_menu;
extern view_list_item_t g_broadcast_settings_items[SETTINGS_SUB_IDX_COUNT];

// vCard list and context menu
extern view_list_screen_t g_vcard_list;
extern view_list_item_t g_vcard_list_items[VCARD_MAX_CARDS + 1];
extern view_context_menu_t g_vcard_context_menu;
extern view_context_item_t g_vcard_context_items[3];
extern view_list_screen_t g_vcard_field_menu;
extern view_list_item_t g_vcard_field_items[12];  // Field categories + Save
// Phone type submenu
extern view_list_screen_t g_phone_type_menu;
extern view_list_item_t g_phone_type_items[PHONE_TYPE_IDX_COUNT];
// IMPP type submenu
extern view_list_screen_t g_impp_type_menu;
extern view_list_item_t g_impp_type_items[IMPP_TYPE_IDX_COUNT];
// Address type submenu
extern view_list_screen_t g_address_type_menu;
extern view_list_item_t g_address_type_items[ADDRESS_TYPE_IDX_COUNT];
extern view_list_screen_t g_vcard_nearby_list;
extern view_list_item_t g_vcard_nearby_items[16];
extern ble_badge_peer_t g_vcard_nearby_peers[16];
extern uint16_t g_vcard_nearby_count;
extern vcard_editor_t g_vcard_editor;
extern app_state_t g_vcard_nearby_return_state;
extern uint32_t g_vcard_nearby_alert_until;
extern app_state_t g_ble_pairing_return_state;
extern uint32_t g_ble_pairing_passkey;
#endif

// WiFi connection timing
extern uint32_t g_wifi_connect_start;
extern uint32_t g_wifi_scan_start;

// NTP sync state: 0=idle, 1=connecting WiFi, 2=syncing NTP
extern uint8_t g_ntp_sync_phase;
// WiFi session ID for NTP sync (0 = no session, >0 = active session)
extern uint8_t g_ntp_wifi_session;

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

// BLE UART state
#if FEATURE_BLE_UART
extern bool g_ble_enabled;
#endif

// Lock screen quick menu
extern view_context_menu_t g_lock_quick_menu;
extern view_context_item_t g_lock_quick_items[4];

// Main menu quick menu (key 3)
extern view_context_menu_t g_main_quick_menu;
extern view_context_item_t g_main_quick_items[4];

// CA state
#if FEATURE_CA
extern view_list_screen_t g_ca_menu;
extern view_list_item_t g_ca_items[CA_IDX_COUNT];
extern char g_ca_detail_text[512];
extern ca_wizard_t g_ca_wizard;
extern view_list_screen_t g_ca_validity_menu;
extern view_list_item_t g_ca_validity_items[6];  // 1, 5, 10, 15, 20, 25 years
#endif
