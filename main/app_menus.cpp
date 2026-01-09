#include "app_menus.h"

#include "i18n.h"
#if FEATURE_CA
#include "ca.h"
#endif

void build_main_menu(void) {
    g_menu_item_count = 0;
#if FEATURE_TOTP
    g_menu_items[g_menu_item_count++] = {i18n_str(STR_TOTP_CODES)};
#endif
#if FEATURE_FIDO2
    g_menu_items[g_menu_item_count++] = {i18n_str(STR_FIDO2_KEYS)};
#endif
#if FEATURE_CA
    g_menu_items[g_menu_item_count++] = {i18n_str(STR_CA_MENU)};
#endif
    g_menu_items[g_menu_item_count++] = {i18n_str(STR_TOOLS)};
    g_menu_items[g_menu_item_count++] = {i18n_str(STR_SYSTEM_TEST)};
    g_menu_items[g_menu_item_count++] = {i18n_str(STR_SETTINGS)};
}

void build_tools_menu(void) {
    g_tools_items[0] = {i18n_str(STR_NTP_SYNC)};
    g_tools_items[1] = {i18n_str(STR_WIFI_MENU)};
}

void build_tools_wifi_menu(void) {
    g_tools_wifi_items[0] = {i18n_str(STR_WIFI_CONNECT)};
    g_tools_wifi_items[1] = {i18n_str(STR_WIFI_SETUP)};
    g_tools_wifi_items[2] = {i18n_str(STR_WIFI_DETAILS)};
    g_tools_wifi_items[3] = {i18n_str(STR_WIFI_DISCONNECT)};
}

void build_wifi_ip_menu(void) {
    g_wifi_ip_items[0] = {i18n_str(STR_WIFI_DHCP)};
    g_wifi_ip_items[1] = {i18n_str(STR_WIFI_STATIC)};
}

void build_settings_menu(void) {
    g_settings_items[SETTINGS_IDX_CHANGE_PIN] = {i18n_str(STR_CHANGE_PIN)};
    g_settings_items[SETTINGS_IDX_BRIGHTNESS] = {i18n_str(STR_BRIGHTNESS)};
    g_settings_items[SETTINGS_IDX_TIMEZONE] = {i18n_str(STR_TIMEZONE)};
    g_settings_items[SETTINGS_IDX_BADGE_TEXTS] = {i18n_str(STR_BADGE_TEXTS)};
    g_settings_items[SETTINGS_IDX_SET_DATETIME] = {i18n_str(STR_SET_DATETIME)};
    g_settings_items[SETTINGS_IDX_LANGUAGE] = {i18n_str(STR_LANGUAGE)};
}

void build_badge_texts_menu(void) {
    g_badge_texts_items[BADGE_IDX_NAME] = {i18n_str(STR_NAME)};
    g_badge_texts_items[BADGE_IDX_INFO] = {i18n_str(STR_INFO)};
    g_badge_texts_items[BADGE_IDX_INFO2] = {i18n_str(STR_INFO2)};
}

#if FEATURE_CA
void build_ca_menu(void) {
    g_ca_items[CA_IDX_STATUS] = {i18n_str(STR_CA_STATUS)};
    g_ca_items[CA_IDX_EXPORT] = {i18n_str(STR_CA_EXPORT)};
    g_ca_items[CA_IDX_QR_CODE] = {i18n_str(STR_CA_SHOW_QR)};
    // Dynamic: "Generate" if no CA exists, "Reset" if CA exists (last item)
    if (ca_is_initialized()) {
        g_ca_items[CA_IDX_GENERATE] = {i18n_str(STR_CA_RESET)};
    } else {
        g_ca_items[CA_IDX_GENERATE] = {i18n_str(STR_CA_GENERATE)};
    }
}
#endif
