#include "app_menus.h"

#include "i18n.h"
#if FEATURE_CA
#include "ca.h"
#endif
#if FEATURE_BLE_UART
#include "ble_uart.h"
#endif
#if FEATURE_BLE_BADGE
#include "ble_badge.h"
#endif

void build_main_menu(void) {
    g_menu_item_count = 0;
#if FEATURE_TOTP
    g_menu_items[g_menu_item_count++] = {i18n_str(STR_TOTP_CODES), VIEW_LIST_ICON_NONE, false};
#endif
#if FEATURE_FIDO2
    g_menu_items[g_menu_item_count++] = {i18n_str(STR_FIDO2_KEYS), VIEW_LIST_ICON_NONE, false};
#endif
#if FEATURE_CA
    g_menu_items[g_menu_item_count++] = {i18n_str(STR_CA_MENU), VIEW_LIST_ICON_NONE, false};
#endif
#if FEATURE_BLE_BADGE
    g_menu_items[g_menu_item_count++] = {i18n_str(STR_REMOTE_BADGE), VIEW_LIST_ICON_NONE, false};
#endif
    g_menu_items[g_menu_item_count++] = {i18n_str(STR_TOOLS), VIEW_LIST_ICON_NONE, false};
    g_menu_items[g_menu_item_count++] = {i18n_str(STR_SETTINGS), VIEW_LIST_ICON_NONE, false};
}

void build_tools_menu(void) {
    g_tools_item_count = 0;
    bool wifi_active = wifi_manager_is_init();
    g_tools_items[g_tools_item_count++] = {i18n_str(STR_WIFI_MENU), VIEW_LIST_ICON_WIFI, !wifi_active};
#if FEATURE_BLE_UART
    bool ble_active = g_ble_enabled;
    if (!ble_active) {
        ble_active = ble_uart_is_initialized();
    }
    g_tools_items[g_tools_item_count++] = {i18n_str(STR_BLUETOOTH), VIEW_LIST_ICON_BLE, !ble_active};
#endif
    g_tools_items[g_tools_item_count++] = {i18n_str(STR_NTP_SYNC), VIEW_LIST_ICON_NONE, false};
    g_tools_items[g_tools_item_count++] = {i18n_str(STR_SYSTEM_TEST), VIEW_LIST_ICON_NONE, false};
}

void build_tools_wifi_menu(void) {
    g_tools_wifi_items[0] = {i18n_str(STR_WIFI_CONNECT), VIEW_LIST_ICON_NONE, false};
    g_tools_wifi_items[1] = {i18n_str(STR_WIFI_SETUP), VIEW_LIST_ICON_NONE, false};
    g_tools_wifi_items[2] = {i18n_str(STR_WIFI_DETAILS), VIEW_LIST_ICON_NONE, false};
    g_tools_wifi_items[3] = {i18n_str(STR_WIFI_DISCONNECT), VIEW_LIST_ICON_NONE, false};
}

#if FEATURE_BLE_BADGE
// Remote Badge main menu (3 submenus)
void build_remote_badge_menu(void) {
    g_remote_badge_items[REMOTE_BADGE_IDX_VCARDS] = {i18n_str(STR_VCARDS), VIEW_LIST_ICON_NONE, false};
    g_remote_badge_items[REMOTE_BADGE_IDX_BROADCAST] = {i18n_str(STR_VCARD_BROADCAST), VIEW_LIST_ICON_NONE, false};
    g_remote_badge_items[REMOTE_BADGE_IDX_SETTINGS] = {i18n_str(STR_BROADCAST_SETTINGS), VIEW_LIST_ICON_NONE, false};
}

// vCards submenu
void build_vcard_submenu(void) {
    g_vcard_submenu_items[VCARD_SUB_IDX_EDIT] = {i18n_str(STR_VCARD_ADD), VIEW_LIST_ICON_NONE, false};
    g_vcard_submenu_items[VCARD_SUB_IDX_EXCHANGE] = {i18n_str(STR_VCARD_EXCHANGE), VIEW_LIST_ICON_NONE, false};
    g_vcard_submenu_items[VCARD_SUB_IDX_LIST] = {i18n_str(STR_VCARD_LIST), VIEW_LIST_ICON_NONE, false};
}

// Broadcast submenu (with toggle status icons)
void build_broadcast_submenu(void) {
    bool send_active = ble_badge_is_adv_enabled();
    bool recv_active = ble_badge_is_scan_enabled();
    g_broadcast_submenu_items[BROADCAST_SUB_IDX_SEND] = {
        i18n_str(STR_BEACON_SEND), VIEW_LIST_ICON_BLE, !send_active};
    g_broadcast_submenu_items[BROADCAST_SUB_IDX_RECEIVE] = {
        i18n_str(STR_BEACON_RECEIVE), VIEW_LIST_ICON_BLE, !recv_active};
}

// Settings submenu
void build_broadcast_settings_menu(void) {
    g_broadcast_settings_items[SETTINGS_SUB_IDX_SEND_INTERVAL] = {
        i18n_str(STR_VCARD_ADV_INTERVAL), VIEW_LIST_ICON_NONE, false};
    g_broadcast_settings_items[SETTINGS_SUB_IDX_SCAN_INTERVAL] = {
        i18n_str(STR_VCARD_SCAN_INTERVAL), VIEW_LIST_ICON_NONE, false};
}

#endif

void build_wifi_ip_menu(void) {
    g_wifi_ip_items[0] = {i18n_str(STR_WIFI_DHCP), VIEW_LIST_ICON_NONE, false};
    g_wifi_ip_items[1] = {i18n_str(STR_WIFI_STATIC), VIEW_LIST_ICON_NONE, false};
}

void build_settings_menu(void) {
    g_settings_items[SETTINGS_IDX_CHANGE_PIN] = {i18n_str(STR_CHANGE_PIN), VIEW_LIST_ICON_NONE, false};
    g_settings_items[SETTINGS_IDX_BRIGHTNESS] = {i18n_str(STR_BRIGHTNESS), VIEW_LIST_ICON_NONE, false};
    g_settings_items[SETTINGS_IDX_TIMEZONE] = {i18n_str(STR_TIMEZONE), VIEW_LIST_ICON_NONE, false};
    g_settings_items[SETTINGS_IDX_BADGE_TEXTS] = {i18n_str(STR_BADGE_TEXTS), VIEW_LIST_ICON_NONE, false};
    g_settings_items[SETTINGS_IDX_SET_DATETIME] = {i18n_str(STR_SET_DATETIME), VIEW_LIST_ICON_NONE, false};
    g_settings_items[SETTINGS_IDX_LANGUAGE] = {i18n_str(STR_LANGUAGE), VIEW_LIST_ICON_NONE, false};
}

void build_badge_texts_menu(void) {
    g_badge_texts_items[BADGE_IDX_NAME] = {i18n_str(STR_NAME), VIEW_LIST_ICON_NONE, false};
    g_badge_texts_items[BADGE_IDX_INFO] = {i18n_str(STR_INFO), VIEW_LIST_ICON_NONE, false};
    g_badge_texts_items[BADGE_IDX_INFO2] = {i18n_str(STR_INFO2), VIEW_LIST_ICON_NONE, false};
}

#if FEATURE_CA
void build_ca_menu(void) {
    g_ca_items[CA_IDX_STATUS] = {i18n_str(STR_CA_STATUS), VIEW_LIST_ICON_NONE, false};
    g_ca_items[CA_IDX_EXPORT] = {i18n_str(STR_CA_EXPORT), VIEW_LIST_ICON_NONE, false};
    g_ca_items[CA_IDX_QR_CODE] = {i18n_str(STR_CA_SHOW_QR), VIEW_LIST_ICON_NONE, false};
    // Dynamic: "Generate" if no CA exists, "Reset" if CA exists (last item)
    if (ca_is_initialized()) {
        g_ca_items[CA_IDX_GENERATE] = {i18n_str(STR_CA_RESET), VIEW_LIST_ICON_NONE, false};
    } else {
        g_ca_items[CA_IDX_GENERATE] = {i18n_str(STR_CA_GENERATE), VIEW_LIST_ICON_NONE, false};
    }
}
#endif
