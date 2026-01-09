#include "app_menus.h"

#include "i18n.h"

void build_main_menu(void) {
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

void build_tools_menu(void) {
    g_tools_items[0] = {i18n_str(STR_NTP_SYNC), "1"};
    g_tools_items[1] = {i18n_str(STR_WIFI_MENU), "2"};
}

void build_tools_wifi_menu(void) {
    g_tools_wifi_items[0] = {i18n_str(STR_WIFI_CONNECT), "1"};
    g_tools_wifi_items[1] = {i18n_str(STR_WIFI_DETAILS), "2"};
    g_tools_wifi_items[2] = {i18n_str(STR_WIFI_DISCONNECT), "3"};
}

void build_wifi_ip_menu(void) {
    g_wifi_ip_items[0] = {i18n_str(STR_WIFI_DHCP), "1"};
    g_wifi_ip_items[1] = {i18n_str(STR_WIFI_STATIC), "2"};
}

void build_settings_menu(void) {
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

void build_badge_texts_menu(void) {
    g_badge_texts_items[BADGE_IDX_NAME] = {i18n_str(STR_NAME), "1"};
    g_badge_texts_items[BADGE_IDX_INFO] = {i18n_str(STR_INFO), "2"};
    g_badge_texts_items[BADGE_IDX_INFO2] = {i18n_str(STR_INFO2), "3"};
    g_badge_texts_items[BADGE_IDX_BACK] = {i18n_str(STR_BACK), "4"};
}
