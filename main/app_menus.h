#pragma once

#include "app_globals.h"

void build_main_menu(void);
void build_tools_menu(void);
void build_tools_wifi_menu(void);
void build_wifi_ip_menu(void);
void build_settings_menu(void);
void build_badge_texts_menu(void);

#if FEATURE_BLE_BADGE
void build_remote_badge_menu(void);
void build_vcard_submenu(void);
void build_broadcast_submenu(void);
void build_broadcast_settings_menu(void);
#endif

#if FEATURE_CA
void build_ca_menu(void);
#endif
