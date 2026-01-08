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
#include "tropic01_cache.h"
#include "i2c_bus.h"
#include "power_management.h"
#include "pin_expander.h"
#include "hw_config.h"
#include "cdc_rtc.h"
#include "serial_cmd.h"
#include "pin_storage.h"
#include "badge_settings.h"
#include "usb_hid.h"
#include "feature_flags.h"
#include "i18n.h"
#include "psa/crypto.h"
#if FEATURE_TOTP
#include "totp_store.h"
#include "base32.h"
#endif
#if FEATURE_FIDO2
#include "fido2.h"
#endif
#if FEATURE_FIDO2_BT
#include "ble_ctap.h"
#endif
#include "wifi_manager.h"
#include "ntp_sync.h"
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

#if FEATURE_TOTP
static view_list_screen_t g_totp_list;
static view_list_item_t g_totp_items[TOTP_MAX_ACCOUNTS];
static view_totp_code_t g_totp_code;
static uint8_t g_totp_selected_index = 0;

// Context menu for TOTP list
static view_context_menu_t g_totp_context_menu;
static const view_context_item_t g_totp_context_items[] = {
    {"Show Code", 1},
    {"Edit", 2},
    {"Delete", 3},
    {"Add New", 4},
    {"Cancel", 0}
};

// TOTP add/edit wizard data
static struct {
    char name[TOTP_NAME_LEN];
    char secret[64];  // Base32 encoded
    char issuer[TOTP_ISSUER_LEN];
    uint8_t digits;   // 4, 6, or 8
    uint8_t algorithm; // 0=SHA-1, 1=SHA-256, 2=SHA-512
    uint32_t period;  // 30 or 60
    bool edit_mode;   // true = editing existing, false = adding new
    uint8_t edit_index; // index being edited (if edit_mode)
} g_totp_wizard;

// TOTP add wizard selection views
static view_list_screen_t g_totp_digits_menu;
static view_list_item_t g_totp_digits_items[] = {
    {"6 Ziffern", "1"},  // Default, most common
    {"4 Ziffern", "2"},
    {"8 Ziffern", "3"}
};
static view_list_screen_t g_totp_period_menu;
static view_list_item_t g_totp_period_items[] = {
    {"30 Sekunden", "1"},  // Default
    {"60 Sekunden", "2"}
};
static view_list_screen_t g_totp_algo_menu;
static view_list_item_t g_totp_algo_items[] = {
    {"SHA-1", "1"},       // Default, most common
    {"SHA-256", "2"},
    {"SHA-512", "3"}
};
#endif

#if FEATURE_FIDO2
static view_list_screen_t g_fido_list;
static view_list_item_t g_fido_items[FIDO2_MAX_CREDENTIALS];
static uint8_t g_fido_selected_index = 0;
static char g_fido_detail_text[256];

// Context menu for FIDO2 list
static view_context_menu_t g_context_menu;
static const view_context_item_t g_fido_context_items[] = {
    {"Details", 1},
    {"Delete", 2},
    {"Cancel", 0}
};

// FIDO2 user presence prompt state
static SemaphoreHandle_t g_fido_prompt_sem = NULL;
static volatile fido2_user_presence_result_t g_fido_prompt_result = FIDO2_UP_PENDING;
static char g_fido_prompt_rp_id[FIDO2_RP_ID_MAX_LEN];
static fido2_action_t g_fido_prompt_action;
static app_state_t g_fido_return_state = APP_STATE_LOCK_SCREEN;
static bool g_fido_was_locked = false;
#endif

// WiFi state and views
static view_list_screen_t g_wifi_list;
static view_list_item_t g_wifi_items[WIFI_MAX_NETWORKS + 1];  // +1 for "Add Manual"
static uint8_t g_wifi_selected_index = 0;
static view_context_menu_t g_wifi_context_menu;
static const view_context_item_t g_wifi_context_items[] = {
    {"Connect", 1},
    {"Add Manual", 2},
    {"Cancel", 0}
};

// WiFi add/edit wizard data
static struct {
    char ssid[WIFI_SSID_MAX_LEN];
    char password[WIFI_PASSWORD_MAX_LEN];
    wifi_auth_mode_t auth_mode;
    bool use_dhcp;
    char static_ip[16];  // "xxx.xxx.xxx.xxx"
    bool from_scan;      // true = selected from scan list
} g_wifi_wizard;

// WiFi auth mode selection
static view_list_screen_t g_wifi_auth_menu;
static view_list_item_t g_wifi_auth_items[] = {
    {"WPA2 Personal", "1"},
    {"WPA/WPA2", "2"},
    {"WPA3 Personal", "3"},
    {"WPA Personal", "4"},
    {"Open", "5"},
    {"WEP", "6"}
};

// WiFi IP mode selection
static view_list_screen_t g_wifi_ip_menu;
static view_list_item_t g_wifi_ip_items[2];

// Tools menu
static view_list_screen_t g_tools_menu;
static view_list_item_t g_tools_items[2];

// Tools WiFi submenu
static view_list_screen_t g_tools_wifi_menu;
static view_list_item_t g_tools_wifi_items[3];

// WiFi connection timing
static uint32_t g_wifi_connect_start = 0;
static uint32_t g_wifi_scan_start = 0;

// NTP sync state: 0=idle, 1=connecting WiFi, 2=syncing NTP
static uint8_t g_ntp_sync_phase = 0;

// Main Menu items (built dynamically for i18n)
static view_list_item_t g_menu_items[8];
static uint8_t g_menu_item_count = 0;

static void build_main_menu(void) {
    g_menu_item_count = 0;
#if FEATURE_TOTP
    g_menu_items[g_menu_item_count++] = {i18n_str(STR_TOTP_CODES), "1"};
#endif
#if FEATURE_FIDO2
    g_menu_items[g_menu_item_count++] = {i18n_str(STR_FIDO2_KEYS), "2"};
#endif
    g_menu_items[g_menu_item_count++] = {i18n_str(STR_TOOLS), "T"};
    g_menu_items[g_menu_item_count++] = {i18n_str(STR_SYSTEM_TEST), "3"};
    g_menu_items[g_menu_item_count++] = {i18n_str(STR_SETTINGS), "4"};
    g_menu_items[g_menu_item_count++] = {i18n_str(STR_SLEEP), "0"};
}

static void build_tools_menu(void) {
    g_tools_items[0] = {i18n_str(STR_NTP_SYNC), "1"};
    g_tools_items[1] = {i18n_str(STR_WIFI_MENU), "2"};
}

static void build_tools_wifi_menu(void) {
    g_tools_wifi_items[0] = {i18n_str(STR_WIFI_CONNECT), "1"};
    g_tools_wifi_items[1] = {i18n_str(STR_WIFI_DETAILS), "2"};
    g_tools_wifi_items[2] = {i18n_str(STR_WIFI_DISCONNECT), "3"};
}

static void build_wifi_ip_menu(void) {
    g_wifi_ip_items[0] = {i18n_str(STR_WIFI_DHCP), "1"};
    g_wifi_ip_items[1] = {i18n_str(STR_WIFI_STATIC), "2"};
}

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

// Settings Menu items (built dynamically for i18n)
static view_list_item_t g_settings_items[SETTINGS_IDX_COUNT];

static void build_settings_menu(void) {
    g_settings_items[SETTINGS_IDX_CHANGE_PIN] = {i18n_str(STR_CHANGE_PIN), "1"};
    g_settings_items[SETTINGS_IDX_BRIGHTNESS] = {i18n_str(STR_BRIGHTNESS), "2"};
    g_settings_items[SETTINGS_IDX_TIMEZONE] = {i18n_str(STR_TIMEZONE), "Z"};
    g_settings_items[SETTINGS_IDX_BADGE_TEXTS] = {i18n_str(STR_BADGE_TEXTS), "3"};
    g_settings_items[SETTINGS_IDX_SET_DATE] = {i18n_str(STR_SET_DATE), "4"};
    g_settings_items[SETTINGS_IDX_SET_TIME] = {i18n_str(STR_SET_TIME), "5"};
    g_settings_items[SETTINGS_IDX_WIFI_SETUP] = {i18n_str(STR_WIFI_SETUP), "W"};
    g_settings_items[SETTINGS_IDX_LANGUAGE] = {i18n_str(STR_LANGUAGE), "6"};
    g_settings_items[SETTINGS_IDX_BACK] = {i18n_str(STR_BACK), "0"};
}

// Badge Texts submenu indices
enum {
    BADGE_IDX_NAME,
    BADGE_IDX_INFO,
    BADGE_IDX_INFO2,
    BADGE_IDX_BACK,
    BADGE_IDX_COUNT
};

// Badge Texts submenu items (built dynamically for i18n)
static view_list_item_t g_badge_texts_items[BADGE_IDX_COUNT];

static void build_badge_texts_menu(void) {
    g_badge_texts_items[BADGE_IDX_NAME] = {i18n_str(STR_NAME), "1"};
    g_badge_texts_items[BADGE_IDX_INFO] = {i18n_str(STR_INFO), "2"};
    g_badge_texts_items[BADGE_IDX_INFO2] = {i18n_str(STR_INFO2), "3"};
    g_badge_texts_items[BADGE_IDX_BACK] = {i18n_str(STR_BACK), "4"};
}

// PIN change state
static char g_new_pin[VIEW_PIN_MAX_LEN + 1] = {0};

// Date/Time input views
static view_date_input_t g_date_input;
static view_time_input_t g_time_input;

// Language menu
static view_list_screen_t g_language_menu;
static view_list_item_t g_language_items[LANG_COUNT];

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

// Security lockout: 1 minute lockout after too many failed PIN attempts
#define LOCKOUT_DURATION_MS (60 * 1000)  // 1 minute
static uint32_t g_lockout_end_ms = 0;  // When lockout expires (millis())

// RTC memory: survives deep sleep
RTC_DATA_ATTR static bool g_in_deep_sleep_mode = false;

// Light sleep configuration
#define LIGHT_SLEEP_WAKEUP_INTERVAL_US (60 * 1000000ULL)  // 60 seconds
#define LIGHT_SLEEP_DELAY_MS 120000  // Wait 120s on lock screen before sleeping
static bool g_light_sleep_configured = false;
static uint32_t g_lock_screen_entered_ms = 0;

// Auto-lock: return to lock screen after inactivity
#define AUTOLOCK_TIMEOUT_MS (3 * 60 * 1000)  // 3 minutes
static uint32_t g_last_activity_ms = 0;

// Forward declarations
static void render_current_state(bool partial);
static void handle_key(char key);
static void update_lock_screen_data(void);

// ============================================================================
// TOTP List Helpers
// ============================================================================

#if FEATURE_TOTP
// Static buffers for list item labels (must persist)
static char g_totp_labels[TOTP_MAX_ACCOUNTS][72];
static char g_totp_shortcuts[TOTP_MAX_ACCOUNTS][4];

static void populate_totp_list(void) {
    uint8_t count = totp_store_count();
    for (uint8_t i = 0; i < count && i < TOTP_MAX_ACCOUNTS; i++) {
        totp_account_info_t info;
        if (totp_store_get_info(i, &info)) {
            // Format: "Name (Issuer)" or just "Name" - truncate safely
            if (strlen(info.issuer) > 0) {
                snprintf(g_totp_labels[i], sizeof(g_totp_labels[i]),
                         "%.30s (%.30s)", info.name, info.issuer);
            } else {
                snprintf(g_totp_labels[i], sizeof(g_totp_labels[i]),
                         "%.30s", info.name);
            }
            snprintf(g_totp_shortcuts[i], sizeof(g_totp_shortcuts[i]), "%d", i + 1);
            g_totp_items[i].label = g_totp_labels[i];
            g_totp_items[i].shortcut = g_totp_shortcuts[i];
        }
    }
    if (count > 0) {
        view_list_screen_init(&g_totp_list, i18n_str(STR_TOTP_CODES), g_totp_items, count);
    }
}

static void go_to_totp_list(void) {
    populate_totp_list();
    uint8_t count = totp_store_count();
    // Always show list (even when empty) so user can access context menu to add new codes
    view_list_screen_init(&g_totp_list, i18n_str(STR_TOTP_CODES), g_totp_items, count);
    g_totp_list.hint = i18n_str(STR_HINT_LIST_MENU);  // Show [3] Menu hint
    g_app_state = APP_STATE_TOTP_LIST;
    render_current_state(false);
}

static void show_totp_code(uint8_t index) {
    g_totp_selected_index = index;
    char code[12];
    int8_t remaining = totp_store_generate_code(index, code);

    totp_account_info_t info;
    totp_store_get_info(index, &info);

    // Initialize TOTP code view
    view_totp_code_init(&g_totp_code,
                        info.issuer[0] ? info.issuer : NULL,
                        info.name,
                        remaining >= 0 ? code : NULL,
                        info.digits,
                        info.period,
                        remaining,
                        i18n_str(STR_HINT_TOTP_CODE));

    g_app_state = APP_STATE_TOTP_CODE;
    render_current_state(false);
}

// TOTP Add/Edit Wizard helpers
static void totp_wizard_start(void) {
    // Reset wizard data for new entry
    memset(&g_totp_wizard, 0, sizeof(g_totp_wizard));
    g_totp_wizard.digits = 6;     // Default
    g_totp_wizard.algorithm = 0;  // SHA-1 default
    g_totp_wizard.period = 30;    // Default
    g_totp_wizard.edit_mode = false;

    // Start with name input
    view_t9_input_init(&g_t9_input, "Account Name", "");
    g_app_state = APP_STATE_TOTP_ADD_NAME;
    render_current_state(false);
}

static void totp_wizard_edit(uint8_t index) {
    // Pre-fill wizard with existing data (including secret from TROPIC01)
    totp_account_t account;
    if (!totp_store_get(index, &account)) {
        view_toast_error("Load failed", 1500);
        return;
    }

    memset(&g_totp_wizard, 0, sizeof(g_totp_wizard));
    strncpy(g_totp_wizard.name, account.name, sizeof(g_totp_wizard.name) - 1);
    strncpy(g_totp_wizard.issuer, account.issuer, sizeof(g_totp_wizard.issuer) - 1);
    // Encode secret back to base32 for editing
    base32_encode(account.secret, account.secretLen, g_totp_wizard.secret, sizeof(g_totp_wizard.secret));
    g_totp_wizard.digits = account.digits;
    g_totp_wizard.algorithm = account.algorithm;
    g_totp_wizard.period = account.period;
    g_totp_wizard.edit_mode = true;
    g_totp_wizard.edit_index = index;

    // Clear secret from RAM after encoding
    memset(account.secret, 0, sizeof(account.secret));

    // Start with name input (pre-filled)
    view_t9_input_init(&g_t9_input, "Account Name", g_totp_wizard.name);
    g_app_state = APP_STATE_TOTP_ADD_NAME;
    render_current_state(false);
}

static void totp_wizard_next_from_name(void) {
    // Save name and go to secret
    strncpy(g_totp_wizard.name, g_t9_input.buffer, sizeof(g_totp_wizard.name) - 1);
    // Pre-fill secret if editing (already loaded from chip)
    view_t9_input_init(&g_t9_input, "Secret (Base32)", g_totp_wizard.secret);
    g_app_state = APP_STATE_TOTP_ADD_SECRET;
    render_current_state(false);
}

static void totp_wizard_next_from_secret(void) {
    // Save secret and go to issuer
    strncpy(g_totp_wizard.secret, g_t9_input.buffer, sizeof(g_totp_wizard.secret) - 1);
    // Pre-fill issuer if editing
    view_t9_input_init(&g_t9_input, "Issuer (optional)", g_totp_wizard.issuer);
    g_app_state = APP_STATE_TOTP_ADD_ISSUER;
    render_current_state(false);
}

static void totp_wizard_next_from_issuer(void) {
    // Save issuer and go to digits selection
    strncpy(g_totp_wizard.issuer, g_t9_input.buffer, sizeof(g_totp_wizard.issuer) - 1);
    view_list_screen_init(&g_totp_digits_menu, "Digits", g_totp_digits_items, 3);
    g_app_state = APP_STATE_TOTP_ADD_DIGITS;
    render_current_state(false);
}

static void totp_wizard_next_from_digits(uint8_t selection) {
    // Save digits (0=6 digits, 1=4 digits, 2=8 digits)
    static const uint8_t digit_map[] = {6, 4, 8};
    g_totp_wizard.digits = digit_map[selection % 3];
    view_list_screen_init(&g_totp_algo_menu, "Algorithm", g_totp_algo_items, 3);
    g_app_state = APP_STATE_TOTP_ADD_ALGO;
    render_current_state(false);
}

static void totp_wizard_next_from_algo(uint8_t selection) {
    // Save algorithm (0=SHA-1, 1=SHA-256, 2=SHA-512)
    g_totp_wizard.algorithm = selection % 3;
    view_list_screen_init(&g_totp_period_menu, "Period", g_totp_period_items, 2);
    g_app_state = APP_STATE_TOTP_ADD_PERIOD;
    render_current_state(false);
}

static void totp_wizard_finish(uint8_t selection) {
    // Save period (0=30s, 1=60s)
    g_totp_wizard.period = (selection == 0) ? 30 : 60;

    // Validate required fields
    if (strlen(g_totp_wizard.name) == 0) {
        view_toast_error("Name required", 1500);
        go_to_totp_list();
        return;
    }
    if (strlen(g_totp_wizard.secret) == 0) {
        view_toast_error("Secret required", 1500);
        go_to_totp_list();
        return;
    }

    // Add to store
    int8_t result = totp_store_add(
        g_totp_wizard.name,
        strlen(g_totp_wizard.issuer) > 0 ? g_totp_wizard.issuer : NULL,
        g_totp_wizard.secret,
        g_totp_wizard.digits,
        g_totp_wizard.period,
        0  // SHA-1 default
    );

    if (result >= 0) {
        view_toast_show(i18n_str(STR_SAVED), 1500);
    } else {
        view_toast_error(i18n_str(STR_SAVE_FAILED), 1500);
    }

    // Return to list
    go_to_totp_list();
}
#endif

// ============================================================================
// FIDO2 List Helpers
// ============================================================================

#if FEATURE_FIDO2
// Static buffers for list item labels (must persist)
static char g_fido_labels[FIDO2_MAX_CREDENTIALS][100];
static char g_fido_shortcuts[FIDO2_MAX_CREDENTIALS][4];

static void populate_fido_list(void) {
    uint8_t count = fido2_get_credential_count();
    for (uint8_t i = 0; i < count && i < FIDO2_MAX_CREDENTIALS; i++) {
        fido2_credential_info_t info;
        if (fido2_get_credential_info(i, &info)) {
            // Format: "rp_id (user)" or just "rp_id" - truncate safely
            if (strlen(info.user_name) > 0) {
                snprintf(g_fido_labels[i], sizeof(g_fido_labels[i]),
                         "%.45s (%.45s)", info.rp_id, info.user_name);
            } else {
                snprintf(g_fido_labels[i], sizeof(g_fido_labels[i]),
                         "%.45s", info.rp_id);
            }
            snprintf(g_fido_shortcuts[i], sizeof(g_fido_shortcuts[i]), "%d", i + 1);
            g_fido_items[i].label = g_fido_labels[i];
            g_fido_items[i].shortcut = g_fido_shortcuts[i];
        }
    }
    if (count > 0) {
        view_list_screen_init(&g_fido_list, i18n_str(STR_FIDO2_KEYS), g_fido_items, count);
    }
}

static void go_to_fido_list(void) {
    populate_fido_list();
    uint8_t count = fido2_get_credential_count();
    // Always show list (even when empty) like TOTP - allows context menu access
    view_list_screen_init(&g_fido_list, i18n_str(STR_FIDO2_KEYS), g_fido_items, count);
    g_fido_list.hint = i18n_str(STR_HINT_LIST_MENU);  // Show [3] Menu hint
    g_app_state = APP_STATE_FIDO_LIST;
    render_current_state(false);
}

static void show_fido_detail(uint8_t index) {
    g_fido_selected_index = index;
    fido2_credential_info_t info;
    if (fido2_get_credential_info(index, &info)) {
        snprintf(g_fido_detail_text, sizeof(g_fido_detail_text),
                 "Relying Party:\n%s\n\n"
                 "User: %s\n"
                 "Slot: %d\n"
                 "Sign count: %lu\n"
                 "Resident: %s\n\n"
                 "%s",
                 info.rp_id,
                 strlen(info.user_name) > 0 ? info.user_name : "(none)",
                 info.slot,
                 info.sign_count,
                 info.resident_key ? "Yes" : "No",
                 i18n_str(STR_HINT_BACK));
        view_info_screen_init(&g_info_view, "FIDO2 Key", g_fido_detail_text);
        g_app_state = APP_STATE_FIDO_DETAIL;
        render_current_state(false);
    }
}

// FIDO2 user presence callback - called from USB task context
// Simple flow: Show request → Y/N → (optional PIN) → result
static fido2_user_presence_result_t fido2_user_presence_callback(
    const char *rp_id,
    fido2_action_t action,
    const char *user_name
) {
    LOG_I("FIDO2", "User presence: %s @ %s",
          action == FIDO2_ACTION_REGISTER ? "register" : "auth", rp_id ? rp_id : "?");

    // Store request info
    strncpy(g_fido_prompt_rp_id, rp_id ? rp_id : "Unknown", sizeof(g_fido_prompt_rp_id) - 1);
    g_fido_prompt_action = action;
    g_fido_prompt_result = FIDO2_UP_PENDING;

    // Drain stale semaphore signals
    while (xSemaphoreTake(g_fido_prompt_sem, 0) == pdTRUE) {
        LOG_W("FIDO2", "Drained stale semaphore");
    }

    // Save current state for return
    g_fido_return_state = g_app_state;
    g_fido_prompt_action = action;
    // Need PIN only if on lock screen AND not a browser probe (SELECT never needs PIN)
    g_fido_was_locked = (g_app_state == APP_STATE_LOCK_SCREEN) && (action != FIDO2_ACTION_SELECT);

    // Wake up display
    gui_backlight_on();

    // Show simple prompt: "Approve? Y/N"
    // IMPORTANT: static buffer because view_info_screen stores pointer, not copy
    const char *headline;
    if (action == FIDO2_ACTION_SELECT) {
        headline = i18n_str(STR_USE_DEVICE);  // Browser probe - just asking if user wants this authenticator
    } else if (action == FIDO2_ACTION_REGISTER) {
        headline = i18n_str(STR_REGISTER_KEY);
    } else {
        headline = i18n_str(STR_SIGN_IN);
    }
    static char prompt_text[200];
    if (action == FIDO2_ACTION_SELECT) {
        // Browser probe - don't show the fake RP ID (.dummy, make.me.blink)
        snprintf(prompt_text, sizeof(prompt_text),
                 "%s\n\n%s",
                 headline,
                 i18n_str(STR_HINT_APPROVE_DENY));
    } else {
        snprintf(prompt_text, sizeof(prompt_text),
                 "%s\n\n%s\n\n%s",
                 headline,
                 g_fido_prompt_rp_id,
                 i18n_str(STR_HINT_APPROVE_DENY));
    }

    view_info_screen_init(&g_info_view, i18n_str(STR_FIDO2_REQUEST), prompt_text);
    g_app_state = APP_STATE_FIDO_PROMPT;
    g_last_activity_ms = millis();  // Reset autolock timer - active FIDO2 request
    render_current_state(false);

    // Wait for result (30s timeout)
    if (xSemaphoreTake(g_fido_prompt_sem, pdMS_TO_TICKS(30000)) == pdTRUE) {
        LOG_I("FIDO2", "Result: %d", g_fido_prompt_result);
        return g_fido_prompt_result;
    }

    // Timeout
    LOG_W("FIDO2", "Timeout");
    g_app_state = g_fido_return_state;

    // Turn off backlight when returning to lock screen (unless forced on)
    if (g_fido_return_state == APP_STATE_LOCK_SCREEN && !g_backlight_forced_on) {
        gui_backlight_off();
    }

    render_current_state(false);
    return FIDO2_UP_TIMEOUT;
}

static void fido2_prompt_complete(fido2_user_presence_result_t result) {
    // Show feedback toast (2s as user requested)
    if (result == FIDO2_UP_APPROVED) {
        view_toast_success(i18n_str(STR_APPROVE), 2000);
    } else {
        view_toast_error(i18n_str(STR_DENY), 2000);
    }

    // Return to previous state
    g_app_state = g_fido_return_state;

    // If a registration was approved while on FIDO list, rebuild the list
    if (result == FIDO2_UP_APPROVED &&
        g_fido_prompt_action == FIDO2_ACTION_REGISTER &&
        g_fido_return_state == APP_STATE_FIDO_LIST) {
        LOG_I("FIDO2", "New key registered - refreshing list");
        populate_fido_list();
    }

    // Turn off backlight when returning to lock screen (unless forced on)
    if (g_fido_return_state == APP_STATE_LOCK_SCREEN && !g_backlight_forced_on) {
        gui_backlight_off();
    }

    render_current_state(false);

    // Signal semaphore AFTER screen is restored
    g_fido_prompt_result = result;
    xSemaphoreGive(g_fido_prompt_sem);
}
#endif

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

    // Update status icons (USB detected via PG_STAT, not USB stack)
    g_lock_screen.status_icons = 0;
    if (power_is_usb_connected()) {
        g_lock_screen.status_icons |= ICON_USB;
    }
    if (wifi_manager_get_state() == WIFI_STATE_CONNECTED) {
        g_lock_screen.status_icons |= ICON_WIFI;
    }

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

// ============================================================================
// Selftest - Hardware status
// ============================================================================

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
            view_t9_input_render(&g_t9_input, partial);
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
    }
}

// ============================================================================
// State Transitions
// ============================================================================

static void go_to_pin_entry(void) {
    // Support 4-6 digit PINs, require Y to confirm
    view_pin_entry_init(&g_pin_entry, i18n_str(STR_ENTER_PIN), PIN_MAX_LEN, 3);
    g_app_state = APP_STATE_PIN_ENTRY;
    render_current_state(false);
}

static void go_to_main_menu(void) {
    build_main_menu();
    view_list_screen_init(&g_main_menu, i18n_str(STR_MAIN_MENU), g_menu_items, g_menu_item_count);
    g_app_state = APP_STATE_MAIN_MENU;
    g_last_activity_ms = millis();  // Start autolock timer
    render_current_state(false);
}

static void go_to_settings_menu(void) {
    build_settings_menu();
    view_list_screen_init(&g_settings_menu, i18n_str(STR_SETTINGS), g_settings_items, SETTINGS_IDX_COUNT);
    g_app_state = APP_STATE_SETTINGS_MENU;
    render_current_state(false);
}

static void go_to_badge_texts_menu(void) {
    build_badge_texts_menu();
    view_list_screen_init(&g_badge_texts_menu, i18n_str(STR_BADGE_TEXTS), g_badge_texts_items, BADGE_IDX_COUNT);
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
    g_last_activity_ms = 0;  // Reset autolock timer (re-starts on unlock)
    render_current_state(false);
}

#if !DEBUG_MODE
static void go_to_lockout(void) {
    // Set lockout end time (1 minute from now)
    g_lockout_end_ms = millis() + LOCKOUT_DURATION_MS;

    // Turn off backlight
    if (!g_backlight_forced_on) {
        gui_backlight_off();
    }

    g_app_state = APP_STATE_LOCKOUT;
    render_current_state(false);
}
#endif

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
                } else if (sel == MENU_IDX_LOCK) {
                    // Block deep sleep when USB is connected
                    if (power_is_usb_connected()) {
                        view_toast_error(i18n_str(STR_UNPLUG_USB), 1500);
                        render_current_state(false);
                    } else {
                        enter_deep_sleep();  // "Sleep" triggers deep sleep
                    }
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

                    case SETTINGS_IDX_SET_DATE: {
                        struct tm timeinfo;
                        if (cdc_rtc_is_time_set()) {
                            cdc_rtc_get_time(&timeinfo);
                        } else {
                            timeinfo.tm_mday = 1;
                            timeinfo.tm_mon = 0;
                            timeinfo.tm_year = 125;  // 2025
                        }
                        view_date_input_init(&g_date_input,
                                             timeinfo.tm_mday,
                                             timeinfo.tm_mon + 1,
                                             timeinfo.tm_year + 1900);
                        g_app_state = APP_STATE_SET_DATE;
                        render_current_state(false);
                        break;
                    }

                    case SETTINGS_IDX_SET_TIME: {
                        struct tm timeinfo;
                        if (cdc_rtc_is_time_set()) {
                            cdc_rtc_get_time(&timeinfo);
                        } else {
                            timeinfo.tm_hour = 12;
                            timeinfo.tm_min = 0;
                        }
                        view_time_input_init(&g_time_input,
                                             timeinfo.tm_hour,
                                             timeinfo.tm_min);
                        g_app_state = APP_STATE_SET_TIME;
                        render_current_state(false);
                        break;
                    }

                    case SETTINGS_IDX_WIFI_SETUP: {
                        // Start WiFi scan
                        wifi_manager_init();
                        wifi_manager_start_scan();
                        g_wifi_scan_start = millis();
                        view_info_screen_init(&g_info_view, "WiFi", i18n_str(STR_WIFI_SCANNING));
                        g_app_state = APP_STATE_WIFI_SCAN;
                        render_current_state(false);
                        break;
                    }

                    case SETTINGS_IDX_LANGUAGE: {
                        // Build language list
                        static char lang_shortcuts[LANG_COUNT][4];
                        for (int i = 0; i < LANG_COUNT; i++) {
                            g_language_items[i].label = i18n_get_language_name((language_t)i);
                            snprintf(lang_shortcuts[i], sizeof(lang_shortcuts[i]), "%d", i + 1);
                            g_language_items[i].shortcut = lang_shortcuts[i];
                        }
                        view_list_screen_init(&g_language_menu, i18n_str(STR_LANGUAGE), g_language_items, LANG_COUNT);
                        // Pre-select current language
                        g_language_menu.selection = i18n_get_language();
                        g_app_state = APP_STATE_LANGUAGE;
                        render_current_state(false);
                        break;
                    }

                    case SETTINGS_IDX_BACK:
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

                    case BADGE_IDX_BACK:
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
                // Save date to RTC
                cdc_rtc_set_date(g_date_input.year, g_date_input.month, g_date_input.day);
                view_toast_success(i18n_str(STR_DATE_SAVED), 1000);
                go_to_settings_menu();
            } else if (key == 'N') {
                go_to_settings_menu();
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
                // Save time to RTC
                cdc_rtc_set_time(g_time_input.hour, g_time_input.minute, 0);
                g_last_minute = -1;  // Force clock update
                view_toast_success(i18n_str(STR_TIME_SAVED), 1000);
                go_to_settings_menu();
            } else if (key == 'N') {
                go_to_settings_menu();
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
                    view_context_menu_init(&g_context_menu, info.rp_id, g_fido_context_items, 3);
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
                    view_t9_input_init(&g_t9_input, i18n_str(STR_WIFI_STATIC), "");
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
            // Numpad input for IP address
            if (key >= '0' && key <= '9') {
                size_t len = strlen(g_t9_input.buffer);
                if (len < 15) {  // Max IP: xxx.xxx.xxx.xxx
                    // Count dots already in string
                    int dots = 0;
                    for (size_t i = 0; i < len; i++) {
                        if (g_t9_input.buffer[i] == '.') dots++;
                    }
                    // Find digits since last dot
                    int digits_in_octet = 0;
                    for (int i = (int)len - 1; i >= 0 && g_t9_input.buffer[i] != '.'; i--) {
                        digits_in_octet++;
                    }
                    // Add digit
                    g_t9_input.buffer[len] = key;
                    g_t9_input.buffer[len + 1] = '\0';
                    digits_in_octet++;
                    // Auto-add dot after 3 digits if not at end
                    if (digits_in_octet == 3 && dots < 3) {
                        g_t9_input.buffer[len + 1] = '.';
                        g_t9_input.buffer[len + 2] = '\0';
                    }
                    render_current_state(true);
                }
            } else if (key == 'N') {
                size_t len = strlen(g_t9_input.buffer);
                if (len > 0) {
                    g_t9_input.buffer[len - 1] = '\0';
                    render_current_state(true);
                } else {
                    // Back to IP mode
                    build_wifi_ip_menu();
                    view_list_screen_init(&g_wifi_ip_menu, i18n_str(STR_WIFI_IP_MODE), g_wifi_ip_items, 2);
                    g_app_state = APP_STATE_WIFI_ADD_IP_MODE;
                    render_current_state(false);
                }
            } else if (key == 'Y') {
                // Parse IP and start connecting
                strncpy(g_wifi_wizard.static_ip, g_t9_input.buffer, 16);
                wifi_config_stored_t config = {};
                strncpy(config.ssid, g_wifi_wizard.ssid, WIFI_SSID_MAX_LEN);
                strncpy(config.password, g_wifi_wizard.password, WIFI_PASSWORD_MAX_LEN);
                config.auth_mode = g_wifi_wizard.auth_mode;
                config.use_dhcp = false;
                // Parse IP address
                uint32_t ip = 0;
                int a, b, c, d;
                if (sscanf(g_wifi_wizard.static_ip, "%d.%d.%d.%d", &a, &b, &c, &d) == 4) {
                    ip = ((uint32_t)d << 24) | ((uint32_t)c << 16) | ((uint32_t)b << 8) | (uint32_t)a;
                }
                config.static_ip = ip;
                config.gateway = (ip & 0x00FFFFFF) | 0x01000000;  // x.x.x.1
                config.subnet = 0x00FFFFFF;  // 255.255.255.0
                config.dns = 0x08080808;     // 8.8.8.8
                wifi_manager_connect(&config);
                g_wifi_connect_start = millis();
                view_info_screen_init(&g_info_view, g_wifi_wizard.ssid, i18n_str(STR_WIFI_CONNECTING));
                g_app_state = APP_STATE_WIFI_CONNECTING;
                render_current_state(false);
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
                view_list_screen_init(&g_tools_wifi_menu, i18n_str(STR_WIFI_MENU), g_tools_wifi_items, 3);
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
                    // NTP Sync - requires WiFi config
                    if (!wifi_manager_has_config()) {
                        view_toast_error(i18n_str(STR_WIFI_NO_CONFIG), 1500);
                        render_current_state(false);
                        break;
                    }
                    // Start WiFi connection for NTP
                    wifi_manager_init();
                    wifi_config_stored_t config;
                    wifi_manager_load_config(&config);
                    wifi_manager_connect(&config);
                    g_wifi_connect_start = millis();
                    g_ntp_sync_phase = 1;  // Connecting WiFi
                    view_info_screen_init(&g_info_view, i18n_str(STR_NTP_SYNC), i18n_str(STR_WIFI_CONNECTING));
                    g_app_state = APP_STATE_TOOLS_NTP_SYNC;
                    render_current_state(false);
                } else if (sel == 1) {
                    // WiFi submenu
                    build_tools_wifi_menu();
                    view_list_screen_init(&g_tools_wifi_menu, i18n_str(STR_WIFI_MENU), g_tools_wifi_items, 3);
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
                wifi_manager_deinit();
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
                } else if (sel == 2) {
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

            // Load cache from TROPIC01 (prevents chip lockout)
            tropic01_cache_init();

            // Load PIN from TROPIC01
            pin_storage_load();
        } else {
            LOG_E("INIT", "TROPIC01 session failed");
        }
    } else {
        LOG_E("INIT", "TROPIC01 init failed");
    }

    // Initialize USB (TinyUSB composite device: CDC + optional HID)
    // Requires FEATURE_USB=1 and CONFIG_USJ_ENABLE_USB_SERIAL_JTAG=n in sdkconfig
    #if FEATURE_USB
    if (usb_hid_init()) {
        LOG_I("INIT", "USB OK (TinyUSB)");
    } else {
        LOG_E("INIT", "USB init failed");
    }
    #endif

    // Initialize console (TinyUSB CDC or JTAG/UART fallback)
    console_init();

    // Initialize TOTP store (uses TROPIC01 cache)
    #if FEATURE_TOTP
    uint8_t totp_count = totp_store_init();
    LOG_I("INIT", "TOTP: %d accounts", totp_count);
    #endif

    // Initialize FIDO2 module
    #if FEATURE_FIDO2
    // Create semaphore for user presence callback
    g_fido_prompt_sem = xSemaphoreCreateBinary();
    if (!g_fido_prompt_sem) {
        LOG_E("INIT", "Failed to create FIDO2 semaphore");
    }

    if (fido2_init()) {
        // Register user presence callback
        fido2_set_user_presence_callback(fido2_user_presence_callback);
        LOG_I("INIT", "FIDO2: %d credentials", fido2_get_credential_count());
    } else {
        LOG_E("INIT", "FIDO2 init failed");
    }
    #endif

    // Initialize BLE FIDO2 transport
    #if FEATURE_FIDO2_BT
    if (ble_ctap_init()) {
        LOG_I("INIT", "BLE FIDO2 OK");
    } else {
        LOG_E("INIT", "BLE FIDO2 init failed");
    }
    #endif

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

    // Initialize i18n (language setting)
    i18n_init();

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

        // Auto-lock: return to lock screen after inactivity
        // Only exempt: lock screen and lockout (already locked)
        if (g_app_state != APP_STATE_LOCK_SCREEN &&
            g_app_state != APP_STATE_LOCKOUT &&
            g_last_activity_ms > 0) {
            uint32_t elapsed = millis() - g_last_activity_ms;
            if (elapsed >= AUTOLOCK_TIMEOUT_MS) {
                LOG_I("LOCK", "Auto-lock: elapsed=%lu, last=%lu, now=%lu",
                      (unsigned long)elapsed, (unsigned long)g_last_activity_ms, (unsigned long)millis());
                go_to_lock_screen();
            }
        }

#if FEATURE_TOTP
        // TOTP code state: update countdown every second
        if (g_app_state == APP_STATE_TOTP_CODE) {
            static uint32_t last_totp_update = 0;
            if (millis() - last_totp_update >= 1000) {
                last_totp_update = millis();
                // Regenerate code and update view
                char code[12];
                int8_t remaining = totp_store_generate_code(g_totp_selected_index, code);
                view_totp_code_update(&g_totp_code, remaining >= 0 ? code : NULL, remaining);
                render_current_state(true);  // Partial refresh
            }
        }
#endif

        // WiFi scan completion check
        if (g_app_state == APP_STATE_WIFI_SCAN) {
            if (wifi_manager_scan_complete()) {
                uint8_t count = wifi_manager_get_network_count();
                if (count == 0) {
                    view_toast_error(i18n_str(STR_WIFI_NO_NETWORKS), 2000);
                    wifi_manager_deinit();
                    go_to_main_menu();
                } else {
                    // Build network list
                    for (uint8_t i = 0; i < count && i < WIFI_MAX_NETWORKS; i++) {
                        wifi_network_t net;
                        if (wifi_manager_get_network(i, &net)) {
                            static char net_labels[WIFI_MAX_NETWORKS][48];
                            snprintf(net_labels[i], sizeof(net_labels[i]), "%s %ddBm%s",
                                    net.ssid, net.rssi,
                                    net.auth_mode == WIFI_AUTH_OPEN ? "" : " *");
                            g_wifi_items[i] = {net_labels[i], ""};
                        }
                    }
                    view_list_screen_init(&g_wifi_list, "WiFi", g_wifi_items, count);
                    g_app_state = APP_STATE_WIFI_LIST;
                    render_current_state(false);
                }
            } else if (millis() - g_wifi_scan_start > 15000) {
                // Scan timeout
                view_toast_error(i18n_str(STR_TIMEOUT), 1500);
                wifi_manager_deinit();
                go_to_main_menu();
            }
        }

        // WiFi connection completion check
        if (g_app_state == APP_STATE_WIFI_CONNECTING) {
            wifi_state_t state = wifi_manager_get_state();
            if (state == WIFI_STATE_CONNECTED) {
                // Save config with last IP
                wifi_config_stored_t config = {};
                strncpy(config.ssid, g_wifi_wizard.ssid, WIFI_SSID_MAX_LEN);
                strncpy(config.password, g_wifi_wizard.password, WIFI_PASSWORD_MAX_LEN);
                config.auth_mode = g_wifi_wizard.auth_mode;
                config.use_dhcp = g_wifi_wizard.use_dhcp;
                config.last_ip = wifi_manager_get_ip();
                wifi_manager_save_config(&config);

                view_toast_success(i18n_str(STR_WIFI_CONNECTED), 1500);
                // Keep connected for Tools > WiFi > Details
                build_tools_wifi_menu();
                view_list_screen_init(&g_tools_wifi_menu, i18n_str(STR_WIFI_MENU), g_tools_wifi_items, 3);
                g_app_state = APP_STATE_TOOLS_WIFI_MENU;
                render_current_state(false);
            } else if (state == WIFI_STATE_FAILED) {
                view_toast_error(i18n_str(STR_WIFI_FAILED), 1500);
                wifi_manager_deinit();
                go_to_main_menu();
            } else if (millis() - g_wifi_connect_start > 20000) {
                // Connection timeout
                view_toast_error(i18n_str(STR_TIMEOUT), 1500);
                wifi_manager_disconnect();
                wifi_manager_deinit();
                go_to_main_menu();
            }
        }

        // NTP sync state machine
        if (g_app_state == APP_STATE_TOOLS_NTP_SYNC) {
            if (g_ntp_sync_phase == 1) {
                // Phase 1: Waiting for WiFi connection
                wifi_state_t state = wifi_manager_get_state();
                if (state == WIFI_STATE_CONNECTED) {
                    // WiFi connected - start NTP sync
                    uint32_t ntp_server = wifi_manager_get_ntp_server();
                    ntp_sync_start(ntp_server);
                    g_ntp_sync_phase = 2;
                    view_info_screen_init(&g_info_view, i18n_str(STR_NTP_SYNC), i18n_str(STR_NTP_SYNCING));
                    render_current_state(false);
                } else if (state == WIFI_STATE_FAILED) {
                    // WiFi connection failed
                    view_toast_error(i18n_str(STR_WIFI_FAILED), 1500);
                    wifi_manager_deinit();
                    g_ntp_sync_phase = 0;
                    build_tools_menu();
                    view_list_screen_init(&g_tools_menu, i18n_str(STR_TOOLS), g_tools_items, 2);
                    g_app_state = APP_STATE_TOOLS_MENU;
                    render_current_state(false);
                } else if (millis() - g_wifi_connect_start > 15000) {
                    // WiFi timeout
                    view_toast_error(i18n_str(STR_TIMEOUT), 1500);
                    wifi_manager_deinit();
                    g_ntp_sync_phase = 0;
                    build_tools_menu();
                    view_list_screen_init(&g_tools_menu, i18n_str(STR_TOOLS), g_tools_items, 2);
                    g_app_state = APP_STATE_TOOLS_MENU;
                    render_current_state(false);
                }
            } else if (g_ntp_sync_phase == 2) {
                // Phase 2: NTP sync in progress
                ntp_state_t ntp_state = ntp_sync_get_state();
                if (ntp_state == NTP_STATE_SUCCESS) {
                    // Success!
                    view_toast_success(i18n_str(STR_NTP_SUCCESS), 1500);
                    ntp_sync_stop();
                    wifi_manager_deinit();
                    g_ntp_sync_phase = 0;
                    build_tools_menu();
                    view_list_screen_init(&g_tools_menu, i18n_str(STR_TOOLS), g_tools_items, 2);
                    g_app_state = APP_STATE_TOOLS_MENU;
                    render_current_state(false);
                } else if (ntp_state == NTP_STATE_FAILED || ntp_sync_timed_out()) {
                    // NTP failed or timed out
                    view_toast_error(i18n_str(STR_NTP_FAILED), 1500);
                    ntp_sync_stop();
                    wifi_manager_deinit();
                    g_ntp_sync_phase = 0;
                    build_tools_menu();
                    view_list_screen_init(&g_tools_menu, i18n_str(STR_TOOLS), g_tools_items, 2);
                    g_app_state = APP_STATE_TOOLS_MENU;
                    render_current_state(false);
                }
            }
        }

        // Lockout state: countdown and auto-return to lock screen
        if (g_app_state == APP_STATE_LOCKOUT) {
            // Check if lockout has expired
            if (millis() >= g_lockout_end_ms) {
                go_to_lock_screen();
            } else {
                // Update countdown every second
                static uint32_t last_update = 0;
                if (millis() - last_update >= 1000) {
                    last_update = millis();
                    render_current_state(true);  // Update countdown display
                }
                // Enter light sleep during lockout to save power
                // Wake up every second to update countdown
                enter_light_sleep();
            }
        }

        // Lock screen: use light sleep for power saving
        if (g_app_state == APP_STATE_LOCK_SCREEN) {
            // Check for USB status change and update display immediately
            static bool last_usb_status = false;
            bool current_usb_status = power_is_usb_connected();
            if (current_usb_status != last_usb_status) {
                last_usb_status = current_usb_status;
                LOG_D("USB", "Status changed: %s", current_usb_status ? "connected" : "disconnected");
                update_lock_screen_data();
                render_current_state(true);
            }

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
            // Skip light sleep when USB is connected (keep device responsive)
            uint32_t time_on_lock = millis() - g_lock_screen_entered_ms;
            bool usb_connected = power_is_usb_connected();
            if (!pin_expander_has_key() && !pin_expander_any_key_down() &&
                !usb_connected && time_on_lock >= LIGHT_SLEEP_DELAY_MS) {
                // Show light sleep indicator
                g_lock_screen.show_light_sleep_icon = true;
                render_current_state(true);

                LOG_D("SLEEP", "Entering light sleep...");
                fflush(stdout);
                enter_light_sleep();
                // Longer delay for I2C/USB to stabilize after sleep
                vTaskDelay(pdMS_TO_TICKS(200));

                // Check wakeup cause - only reset timer and hide icon if user pressed a key
                esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
                if (cause == ESP_SLEEP_WAKEUP_GPIO) {
                    LOG_D("SLEEP", "Woke up (key press)");
                    g_lock_screen.show_light_sleep_icon = false;  // Hide on key press
                    g_lock_screen_entered_ms = millis();  // User interaction - reset timer
                    update_lock_screen_data();  // Refresh USB/battery status
                    render_current_state(true);  // Refresh display immediately
                } else {
                    LOG_D("SLEEP", "Woke up (timer)");
                    // Timer wakeup - keep icon, don't reset timer, go back to sleep after clock update
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
        // T9 input states - update display and check for 2s N abort
        bool is_t9_state = (g_app_state == APP_STATE_T9_DEMO ||
                            g_app_state == APP_STATE_BADGE_EDIT_NAME ||
                            g_app_state == APP_STATE_BADGE_EDIT_INFO ||
                            g_app_state == APP_STATE_BADGE_EDIT_INFO2 ||
                            g_app_state == APP_STATE_TOTP_ADD_NAME ||
                            g_app_state == APP_STATE_TOTP_ADD_SECRET ||
                            g_app_state == APP_STATE_TOTP_ADD_ISSUER);
        if (is_t9_state) {
            view_t9_input_update(&g_t9_input);

            // Check for N long-press abort (2s)
            if (pin_expander_is_key_down('N')) {
                if (g_n_key_press_start == 0) {
                    g_n_key_press_start = millis();
                } else if (millis() - g_n_key_press_start >= T9_ABORT_HOLD_MS) {
                    g_n_key_press_start = 0;
                    // Abort without saving - return to appropriate list
                    if (g_app_state == APP_STATE_T9_DEMO) {
                        go_to_main_menu();
                    } else if (g_app_state == APP_STATE_TOTP_ADD_NAME ||
                               g_app_state == APP_STATE_TOTP_ADD_SECRET ||
                               g_app_state == APP_STATE_TOTP_ADD_ISSUER) {
                        go_to_totp_list();
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
