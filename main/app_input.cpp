#include "app_input.h"

#include "app_fido.h"
#include "app_globals.h"
#include "app_menus.h"
#include "app_power.h"
#include "app_render.h"
#include "app_state.h"
#include "app_status.h"
#include "app_totp.h"

#include "badge_settings.h"
#include "cdc_log.h"
#include "cdc_rtc.h"
#include "cdc_time.h"
#include "gui.h"
#include "i18n.h"
#include "ntp_sync.h"
#include "pin_storage.h"
#include "power_management.h"

#if FEATURE_TOTP
#include "totp_store.h"
#endif
#if FEATURE_FIDO2
#include "fido2.h"
#endif

#if FEATURE_BLE_UART
#include "ble_uart.h"
#endif
#if FEATURE_BLE_BADGE
#include "vcard_store.h"
#include "ble_badge.h"
#endif

#if FEATURE_CA
#include "ca.h"
#endif

#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <cctype>

#if FEATURE_BLE_UART || FEATURE_BLE_BADGE
static bool bluetooth_is_active(void) {
    bool active = false;
#if FEATURE_BLE_UART
    active = active || g_ble_enabled || ble_uart_is_initialized();
#endif
#if FEATURE_BLE_BADGE
    active = active || ble_badge_is_adv_active() || ble_badge_is_scan_active() ||
             ble_badge_is_exchange_enabled() || ble_badge_exchange_in_progress();
#endif
    return active;
}
#endif

#if FEATURE_BLE_BADGE
// Field categories shown in the main field menu
enum {
    VCARD_FIELD_CAT_PHONE,      // -> Phone type submenu
    VCARD_FIELD_CAT_EMAIL,
    VCARD_FIELD_CAT_URL,
    VCARD_FIELD_CAT_ORG,
    VCARD_FIELD_CAT_TITLE,
    VCARD_FIELD_CAT_SOCIAL,
    VCARD_FIELD_CAT_IMPP,       // -> IMPP type submenu
    VCARD_FIELD_CAT_ADDRESS,    // -> Address type submenu
    VCARD_FIELD_CAT_COUNT
};

// Storage field types (include subtypes for vCard export)
enum {
    VCARD_FIELD_TYPE_PHONE_LANDLINE,
    VCARD_FIELD_TYPE_PHONE_MOBILE,
    VCARD_FIELD_TYPE_PHONE_BUSINESS,
    VCARD_FIELD_TYPE_PHONE_PAGER,
    VCARD_FIELD_TYPE_EMAIL,
    VCARD_FIELD_TYPE_URL,
    VCARD_FIELD_TYPE_ORG,
    VCARD_FIELD_TYPE_TITLE,
    VCARD_FIELD_TYPE_SOCIAL,
    VCARD_FIELD_TYPE_IMPP_TELEGRAM,
    VCARD_FIELD_TYPE_IMPP_SIGNAL,
    VCARD_FIELD_TYPE_IMPP_WHATSAPP,
    VCARD_FIELD_TYPE_IMPP_DISCORD,
    VCARD_FIELD_TYPE_IMPP_MATRIX,
    VCARD_FIELD_TYPE_IMPP_THREEMA,
    VCARD_FIELD_TYPE_IMPP_OTHER,
    VCARD_FIELD_TYPE_ADDRESS_HOME,
    VCARD_FIELD_TYPE_ADDRESS_WORK,
    VCARD_FIELD_TYPE_COUNT
};

// Currently selected category and subtype for two-step field selection
static uint8_t g_vcard_field_category = 0;
static uint8_t g_vcard_field_subtype = 0;

static uint16_t g_vcard_list_slots[VCARD_MAX_CARDS + 1];
static uint16_t g_vcard_list_count = 0;
static uint16_t g_vcard_selected_slot = 0xFFFF;
static bool g_vcard_selected_is_own = false;
static char g_vcard_list_labels[VCARD_MAX_CARDS + 1][VIEW_MAX_TEXT_LEN];
static char g_broadcast_menu_labels[BROADCAST_SUB_IDX_COUNT][VIEW_MAX_TEXT_LEN];
static char g_broadcast_settings_labels[SETTINGS_SUB_IDX_COUNT][VIEW_MAX_TEXT_LEN];
static char g_vcard_view_buf[VCARD_MAX_LEN + 1];
static char g_vcard_qr_buf[VCARD_MAX_LEN + 1];
static char g_vcard_qr_name[64];

static void vcard_editor_reset(void) {
    memset(&g_vcard_editor, 0, sizeof(g_vcard_editor));
}

static void vcard_editor_add_extra(uint8_t type, const char *value) {
    if (!value || !value[0]) return;
    if (g_vcard_editor.extra_count >= VCARD_EDITOR_MAX_EXTRAS) return;
    uint8_t idx = g_vcard_editor.extra_count++;
    g_vcard_editor.extra_type[idx] = type;
    strncpy(g_vcard_editor.extra_value[idx], value, sizeof(g_vcard_editor.extra_value[idx]) - 1);
    g_vcard_editor.extra_value[idx][sizeof(g_vcard_editor.extra_value[idx]) - 1] = '\0';
}

// Unescape vCard escape sequences: \, -> , | \; -> ; | \\ -> \ | \n -> newline
static void vcard_unescape(char *str) {
    if (!str) return;
    char *src = str;
    char *dst = str;
    while (*src) {
        if (*src == '\\' && *(src + 1)) {
            char next = *(src + 1);
            if (next == ',' || next == ';' || next == '\\') {
                *dst++ = next;
                src += 2;
            } else if (next == 'n' || next == 'N') {
                *dst++ = ' ';  // Replace newline with space for display
                src += 2;
            } else {
                *dst++ = *src++;
            }
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

// Extract property name from grouped field (e.g., "item1.EMAIL" -> "EMAIL")
static const char *vcard_extract_property(const char *key, char *out, size_t out_len) {
    const char *dot = strchr(key, '.');
    if (dot) {
        // Skip group prefix (e.g., "item1.")
        key = dot + 1;
    }
    size_t len = strlen(key);
    if (len >= out_len) len = out_len - 1;
    for (size_t i = 0; i < len; i++) {
        out[i] = (char)toupper((unsigned char)key[i]);
    }
    out[len] = '\0';
    return out;
}

static void vcard_editor_load_from_vcard(const char *vcard) {
    if (!vcard || !vcard[0]) return;
    LOG_I("VCARD", "Loading vCard into editor, len=%d", (int)strlen(vcard));
    const char *p = vcard;
    while (*p) {
        const char *line_end = strchr(p, '\n');
        size_t line_len = line_end ? (size_t)(line_end - p) : strlen(p);
        if (line_len == 0) {
            if (!line_end) break;
            p = line_end + 1;
            continue;
        }
        char line[256];
        if (line_len >= sizeof(line)) line_len = sizeof(line) - 1;
        memcpy(line, p, line_len);
        line[line_len] = '\0';
        if (line_len > 0 && line[line_len - 1] == '\r') {
            line[line_len - 1] = '\0';
        }

        char *colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            const char *key = line;
            char value[256];
            strncpy(value, colon + 1, sizeof(value) - 1);
            value[sizeof(value) - 1] = '\0';
            vcard_unescape(value);  // Decode escape sequences

            // Extract property name (handles grouped fields like "item1.EMAIL")
            char key_upper[128];
            vcard_extract_property(key, key_upper, sizeof(key_upper));

            // =========================================================================
            // vCard 4.0 (RFC 6350) Property Mapping
            // Reference: https://github.com/kevinioi/vcf-parser ValidationHelper.c
            // =========================================================================

            // --- IGNORED FIELDS (binary data, metadata, Apple-specific) ---
            // PHOTO, LOGO, SOUND, KEY: Binary/base64 data - too large for badge
            // PRODID, REV, UID, VERSION: Metadata - not user-relevant
            // SOURCE, KIND, XML, CLIENTPIDMAP: Technical metadata
            // BDAY, ANNIVERSARY, GENDER: Date fields - not supported in editor
            // LANG, TZ, GEO: Locale data - not needed
            // MEMBER, RELATED, CATEGORIES: Group/relationship data
            // FBURL, CALADRURI, CALURI: Calendar URLs - not needed
            // X-AB*: Apple-specific labels and metadata
            if (strncmp(key_upper, "PHOTO", 5) == 0 ||
                strncmp(key_upper, "LOGO", 4) == 0 ||
                strncmp(key_upper, "SOUND", 5) == 0 ||
                strncmp(key_upper, "KEY", 3) == 0 ||
                strncmp(key_upper, "PRODID", 6) == 0 ||
                strncmp(key_upper, "REV", 3) == 0 ||
                strncmp(key_upper, "UID", 3) == 0 ||
                strncmp(key_upper, "VERSION", 7) == 0 ||
                strncmp(key_upper, "SOURCE", 6) == 0 ||
                strncmp(key_upper, "KIND", 4) == 0 ||
                strncmp(key_upper, "XML", 3) == 0 ||
                strncmp(key_upper, "CLIENTPIDMAP", 12) == 0 ||
                strncmp(key_upper, "BDAY", 4) == 0 ||
                strncmp(key_upper, "ANNIVERSARY", 11) == 0 ||
                strncmp(key_upper, "GENDER", 6) == 0 ||
                strncmp(key_upper, "LANG", 4) == 0 ||
                strncmp(key_upper, "TZ", 2) == 0 ||
                strncmp(key_upper, "GEO", 3) == 0 ||
                strncmp(key_upper, "MEMBER", 6) == 0 ||
                strncmp(key_upper, "RELATED", 7) == 0 ||
                strncmp(key_upper, "CATEGORIES", 10) == 0 ||
                strncmp(key_upper, "FBURL", 5) == 0 ||
                strncmp(key_upper, "CALADRURI", 9) == 0 ||
                strncmp(key_upper, "CALURI", 6) == 0 ||
                strncmp(key_upper, "BEGIN", 5) == 0 ||
                strncmp(key_upper, "END", 3) == 0 ||
                strncmp(key_upper, "X-AB", 4) == 0 ||           // Apple X-ABLabel, X-ABADR, etc.
                strncmp(key_upper, "X-IMAGETYPE", 11) == 0 ||
                strncmp(key_upper, "X-IMAGEHASH", 11) == 0 ||
                strncmp(key_upper, "X-APPLE", 7) == 0 ||
                strncmp(key_upper, "X-PHONETIC", 10) == 0) {
                // Skip these fields - not relevant for badge display
            }
            // --- NAME FIELDS ---
            // N: Structured name (Family;Given;Additional;Prefix;Suffix)
            else if (strncmp(key_upper, "N", 1) == 0 && (key_upper[1] == '\0' || key_upper[1] == ';')) {
                char tmp[128];
                strncpy(tmp, value, sizeof(tmp) - 1);
                tmp[sizeof(tmp) - 1] = '\0';
                char *family = tmp;
                char *given = strchr(tmp, ';');
                if (given) {
                    *given = '\0';
                    given++;
                    char *next_semi = strchr(given, ';');
                    if (next_semi) *next_semi = '\0';
                }
                LOG_D("VCARD", "N field: family='%s', given='%s'", family ? family : "(null)", given ? given : "(null)");
                if (family && *family) {
                    strncpy(g_vcard_editor.last, family, sizeof(g_vcard_editor.last) - 1);
                    g_vcard_editor.last[sizeof(g_vcard_editor.last) - 1] = '\0';
                    LOG_D("VCARD", "Set last='%s'", g_vcard_editor.last);
                }
                if (given && *given) {
                    strncpy(g_vcard_editor.first, given, sizeof(g_vcard_editor.first) - 1);
                    g_vcard_editor.first[sizeof(g_vcard_editor.first) - 1] = '\0';
                    LOG_D("VCARD", "Set first='%s'", g_vcard_editor.first);
                }
            }
            // FN: Formatted name (fallback if N not present)
            else if (strncmp(key_upper, "FN", 2) == 0 && (key_upper[2] == '\0' || key_upper[2] == ';')) {
                if (g_vcard_editor.first[0] == '\0' && g_vcard_editor.last[0] == '\0') {
                    strncpy(g_vcard_editor.first, value, sizeof(g_vcard_editor.first) - 1);
                    g_vcard_editor.first[sizeof(g_vcard_editor.first) - 1] = '\0';
                }
            }
            // NICKNAME: Alternative name -> store in note if empty
            else if (strncmp(key_upper, "NICKNAME", 8) == 0) {
                if (g_vcard_editor.note[0] == '\0') {
                    strncpy(g_vcard_editor.note, value, sizeof(g_vcard_editor.note) - 1);
                    g_vcard_editor.note[sizeof(g_vcard_editor.note) - 1] = '\0';
                }
            }
            // --- NOTE FIELD ---
            // NOTE: Free-form text
            else if (strncmp(key_upper, "NOTE", 4) == 0) {
                strncpy(g_vcard_editor.note, value, sizeof(g_vcard_editor.note) - 1);
                g_vcard_editor.note[sizeof(g_vcard_editor.note) - 1] = '\0';
            }
            // --- PHONE FIELDS ---
            // TEL: Telephone number with TYPE parameter
            else if (strncmp(key_upper, "TEL", 3) == 0) {
                if (strstr(key_upper, "PAGER") || strstr(key_upper, "BBP")) {
                    vcard_editor_add_extra(VCARD_FIELD_TYPE_PHONE_PAGER, value);
                } else if (strstr(key_upper, "CELL") || strstr(key_upper, "MOBILE") ||
                           strstr(key_upper, "IPHONE") || strstr(key_upper, "MAIN")) {
                    vcard_editor_add_extra(VCARD_FIELD_TYPE_PHONE_MOBILE, value);
                } else if (strstr(key_upper, "WORK") || strstr(key_upper, "BUSINESS")) {
                    vcard_editor_add_extra(VCARD_FIELD_TYPE_PHONE_BUSINESS, value);
                } else if (strstr(key_upper, "FAX")) {
                    vcard_editor_add_extra(VCARD_FIELD_TYPE_PHONE_LANDLINE, value);  // Fax as landline
                } else {
                    vcard_editor_add_extra(VCARD_FIELD_TYPE_PHONE_LANDLINE, value);
                }
            }
            // --- EMAIL FIELD ---
            // EMAIL: Email address
            else if (strncmp(key_upper, "EMAIL", 5) == 0) {
                vcard_editor_add_extra(VCARD_FIELD_TYPE_EMAIL, value);
            }
            // --- URL FIELDS ---
            // URL: Web address
            else if (strncmp(key_upper, "URL", 3) == 0) {
                vcard_editor_add_extra(VCARD_FIELD_TYPE_URL, value);
            }
            // --- ORGANIZATION FIELDS ---
            // ORG: Organization name (may have multiple components separated by ;)
            else if (strncmp(key_upper, "ORG", 3) == 0) {
                vcard_editor_add_extra(VCARD_FIELD_TYPE_ORG, value);
            }
            // TITLE: Job title
            // ROLE: Function/occupation
            else if (strncmp(key_upper, "TITLE", 5) == 0 || strncmp(key_upper, "ROLE", 4) == 0) {
                vcard_editor_add_extra(VCARD_FIELD_TYPE_TITLE, value);
            }
            // --- IMPP FIELDS (Instant Messaging / Messengers) ---
            // IMPP: RFC 6350 standard for instant messaging
            // Modern messengers: Telegram, Signal, WhatsApp, Discord, Matrix, etc.
            else if (strncmp(key_upper, "IMPP", 4) == 0 ||
                     strncmp(key_upper, "X-TELEGRAM", 10) == 0 ||
                     strncmp(key_upper, "X-SIGNAL", 8) == 0 ||
                     strncmp(key_upper, "X-WHATSAPP", 10) == 0 ||
                     strncmp(key_upper, "X-DISCORD", 9) == 0 ||
                     strncmp(key_upper, "X-MATRIX", 8) == 0 ||
                     strncmp(key_upper, "X-THREEMA", 9) == 0 ||
                     strncmp(key_upper, "X-WIRE", 6) == 0 ||
                     strncmp(key_upper, "X-JABBER", 8) == 0 ||
                     strncmp(key_upper, "X-XMPP", 6) == 0 ||
                     strncmp(key_upper, "X-AIM", 5) == 0 ||
                     strncmp(key_upper, "X-MSN", 5) == 0 ||
                     strncmp(key_upper, "X-SKYPE", 7) == 0 ||
                     strncmp(key_upper, "X-YAHOO", 7) == 0 ||
                     strncmp(key_upper, "X-ICQ", 5) == 0 ||
                     strncmp(key_upper, "X-GOOGLE", 8) == 0) {
                // Detect specific messenger type from key or value
                uint8_t impp_type = VCARD_FIELD_TYPE_IMPP_OTHER;
                if (strstr(key_upper, "TELEGRAM") || strstr(value, "telegram:") || strstr(value, "t.me/")) {
                    impp_type = VCARD_FIELD_TYPE_IMPP_TELEGRAM;
                } else if (strstr(key_upper, "SIGNAL") || strstr(value, "signal:")) {
                    impp_type = VCARD_FIELD_TYPE_IMPP_SIGNAL;
                } else if (strstr(key_upper, "WHATSAPP") || strstr(value, "whatsapp:") || strstr(value, "wa.me/")) {
                    impp_type = VCARD_FIELD_TYPE_IMPP_WHATSAPP;
                } else if (strstr(key_upper, "DISCORD") || strstr(value, "discord:")) {
                    impp_type = VCARD_FIELD_TYPE_IMPP_DISCORD;
                } else if (strstr(key_upper, "MATRIX") || strstr(value, "matrix:") || (strstr(value, "@") && strstr(value, ":"))) {
                    impp_type = VCARD_FIELD_TYPE_IMPP_MATRIX;
                } else if (strstr(key_upper, "THREEMA") || strstr(value, "threema:")) {
                    impp_type = VCARD_FIELD_TYPE_IMPP_THREEMA;
                }
                vcard_editor_add_extra(impp_type, value);
            }
            // --- SOCIAL FIELDS (Social Media Profiles) ---
            // X-SOCIALPROFILE: Apple extension for social media
            // Social networks: Twitter/X, Facebook, LinkedIn, GitHub, Instagram, Mastodon
            else if (strncmp(key_upper, "X-SOCIALPROFILE", 15) == 0 ||
                     strncmp(key_upper, "X-TWITTER", 9) == 0 ||
                     strncmp(key_upper, "X-FACEBOOK", 10) == 0 ||
                     strncmp(key_upper, "X-LINKEDIN", 10) == 0 ||
                     strncmp(key_upper, "X-GITHUB", 8) == 0 ||
                     strncmp(key_upper, "X-GITLAB", 8) == 0 ||
                     strncmp(key_upper, "X-INSTAGRAM", 11) == 0 ||
                     strncmp(key_upper, "X-MASTODON", 10) == 0 ||
                     strncmp(key_upper, "X-BLUESKY", 9) == 0 ||
                     strncmp(key_upper, "X-TIKTOK", 8) == 0 ||
                     strncmp(key_upper, "X-YOUTUBE", 9) == 0 ||
                     strncmp(key_upper, "X-REDDIT", 8) == 0) {
                vcard_editor_add_extra(VCARD_FIELD_TYPE_SOCIAL, value);
            }
            // --- ADDRESS FIELD ---
            // ADR: Structured address (POBox;Extended;Street;City;Region;PostalCode;Country)
            else if (strncmp(key_upper, "ADR", 3) == 0) {
                if (strstr(key_upper, "WORK")) {
                    vcard_editor_add_extra(VCARD_FIELD_TYPE_ADDRESS_WORK, value);
                } else {
                    vcard_editor_add_extra(VCARD_FIELD_TYPE_ADDRESS_HOME, value);
                }
            }
            // All other fields are silently ignored (unknown X- fields, etc.)
        }

        if (!line_end) break;
        p = line_end + 1;
    }
    g_vcard_editor.extra_edit_index = 0;
}

// Update broadcast submenu labels with toggle states
static void broadcast_submenu_update_labels(void) {
    const char *on = i18n_str(STR_ON);
    const char *off = i18n_str(STR_OFF);

    snprintf(g_broadcast_menu_labels[BROADCAST_SUB_IDX_SEND],
             sizeof(g_broadcast_menu_labels[BROADCAST_SUB_IDX_SEND]),
             "%s: %s", i18n_str(STR_BEACON_SEND), ble_badge_is_adv_enabled() ? on : off);
    snprintf(g_broadcast_menu_labels[BROADCAST_SUB_IDX_RECEIVE],
             sizeof(g_broadcast_menu_labels[BROADCAST_SUB_IDX_RECEIVE]),
             "%s: %s", i18n_str(STR_BEACON_RECEIVE), ble_badge_is_scan_enabled() ? on : off);

    g_broadcast_submenu_items[BROADCAST_SUB_IDX_SEND].label = g_broadcast_menu_labels[BROADCAST_SUB_IDX_SEND];
    g_broadcast_submenu_items[BROADCAST_SUB_IDX_RECEIVE].label = g_broadcast_menu_labels[BROADCAST_SUB_IDX_RECEIVE];
}

// Update broadcast settings submenu labels with current values
static void broadcast_settings_update_labels(void) {
    snprintf(g_broadcast_settings_labels[SETTINGS_SUB_IDX_SEND_INTERVAL],
             sizeof(g_broadcast_settings_labels[SETTINGS_SUB_IDX_SEND_INTERVAL]),
             "%s: %lus", i18n_str(STR_VCARD_ADV_INTERVAL), (unsigned long)(ble_badge_get_adv_interval() / 1000));
    snprintf(g_broadcast_settings_labels[SETTINGS_SUB_IDX_SCAN_INTERVAL],
             sizeof(g_broadcast_settings_labels[SETTINGS_SUB_IDX_SCAN_INTERVAL]),
             "%s: %lus", i18n_str(STR_VCARD_SCAN_INTERVAL), (unsigned long)(ble_badge_get_scan_interval() / 1000));

    g_broadcast_settings_items[SETTINGS_SUB_IDX_SEND_INTERVAL].label = g_broadcast_settings_labels[SETTINGS_SUB_IDX_SEND_INTERVAL];
    g_broadcast_settings_items[SETTINGS_SUB_IDX_SCAN_INTERVAL].label = g_broadcast_settings_labels[SETTINGS_SUB_IDX_SCAN_INTERVAL];
}

// Navigate back to vCards submenu
static void go_to_vcard_submenu(void) {
    build_vcard_submenu();
    view_list_screen_init(&g_vcard_submenu, i18n_str(STR_VCARDS),
                          g_vcard_submenu_items, VCARD_SUB_IDX_COUNT);
    g_app_state = APP_STATE_VCARD_SUBMENU;
    render_current_state(false);
}

static void vcard_fields_init_menu(void) {
    // Main field category menu
    g_vcard_field_items[VCARD_FIELD_CAT_PHONE].label = i18n_str(STR_PHONE);
    g_vcard_field_items[VCARD_FIELD_CAT_EMAIL].label = i18n_str(STR_EMAIL);
    g_vcard_field_items[VCARD_FIELD_CAT_URL].label = i18n_str(STR_URL);
    g_vcard_field_items[VCARD_FIELD_CAT_ORG].label = i18n_str(STR_ORG);
    g_vcard_field_items[VCARD_FIELD_CAT_TITLE].label = i18n_str(STR_TITLE);
    g_vcard_field_items[VCARD_FIELD_CAT_SOCIAL].label = i18n_str(STR_SOCIAL);
    g_vcard_field_items[VCARD_FIELD_CAT_IMPP].label = i18n_str(STR_IMPP);
    g_vcard_field_items[VCARD_FIELD_CAT_ADDRESS].label = i18n_str(STR_ADDRESS);
    g_vcard_field_items[VCARD_FIELD_CAT_COUNT].label = i18n_str(STR_SAVE);
}

static void vcard_phone_type_init_menu(void) {
    g_phone_type_items[PHONE_TYPE_IDX_LANDLINE].label = i18n_str(STR_PHONE_LANDLINE);
    g_phone_type_items[PHONE_TYPE_IDX_MOBILE].label = i18n_str(STR_PHONE_MOBILE);
    g_phone_type_items[PHONE_TYPE_IDX_BUSINESS].label = i18n_str(STR_PHONE_BUSINESS);
    g_phone_type_items[PHONE_TYPE_IDX_PAGER].label = i18n_str(STR_PHONE_PAGER);
}

static void vcard_impp_type_init_menu(void) {
    g_impp_type_items[IMPP_TYPE_IDX_TELEGRAM].label = i18n_str(STR_IMPP_TELEGRAM);
    g_impp_type_items[IMPP_TYPE_IDX_SIGNAL].label = i18n_str(STR_IMPP_SIGNAL);
    g_impp_type_items[IMPP_TYPE_IDX_WHATSAPP].label = i18n_str(STR_IMPP_WHATSAPP);
    g_impp_type_items[IMPP_TYPE_IDX_DISCORD].label = i18n_str(STR_IMPP_DISCORD);
    g_impp_type_items[IMPP_TYPE_IDX_MATRIX].label = i18n_str(STR_IMPP_MATRIX);
    g_impp_type_items[IMPP_TYPE_IDX_THREEMA].label = i18n_str(STR_IMPP_THREEMA);
    g_impp_type_items[IMPP_TYPE_IDX_OTHER].label = i18n_str(STR_IMPP_OTHER);
}

static void vcard_address_type_init_menu(void) {
    g_address_type_items[ADDRESS_TYPE_IDX_HOME].label = i18n_str(STR_ADDRESS_HOME);
    g_address_type_items[ADDRESS_TYPE_IDX_WORK].label = i18n_str(STR_ADDRESS_WORK);
}

// Map phone type submenu index to storage type
static uint8_t phone_type_to_field_type(uint8_t idx) {
    switch (idx) {
        case PHONE_TYPE_IDX_LANDLINE: return VCARD_FIELD_TYPE_PHONE_LANDLINE;
        case PHONE_TYPE_IDX_MOBILE:   return VCARD_FIELD_TYPE_PHONE_MOBILE;
        case PHONE_TYPE_IDX_BUSINESS: return VCARD_FIELD_TYPE_PHONE_BUSINESS;
        case PHONE_TYPE_IDX_PAGER:    return VCARD_FIELD_TYPE_PHONE_PAGER;
        default:                      return VCARD_FIELD_TYPE_PHONE_LANDLINE;
    }
}

// Map IMPP type submenu index to storage type
static uint8_t impp_type_to_field_type(uint8_t idx) {
    switch (idx) {
        case IMPP_TYPE_IDX_TELEGRAM: return VCARD_FIELD_TYPE_IMPP_TELEGRAM;
        case IMPP_TYPE_IDX_SIGNAL:   return VCARD_FIELD_TYPE_IMPP_SIGNAL;
        case IMPP_TYPE_IDX_WHATSAPP: return VCARD_FIELD_TYPE_IMPP_WHATSAPP;
        case IMPP_TYPE_IDX_DISCORD:  return VCARD_FIELD_TYPE_IMPP_DISCORD;
        case IMPP_TYPE_IDX_MATRIX:   return VCARD_FIELD_TYPE_IMPP_MATRIX;
        case IMPP_TYPE_IDX_THREEMA:  return VCARD_FIELD_TYPE_IMPP_THREEMA;
        case IMPP_TYPE_IDX_OTHER:    return VCARD_FIELD_TYPE_IMPP_OTHER;
        default:                     return VCARD_FIELD_TYPE_IMPP_OTHER;
    }
}

// Map address type submenu index to storage type
static uint8_t address_type_to_field_type(uint8_t idx) {
    switch (idx) {
        case ADDRESS_TYPE_IDX_HOME: return VCARD_FIELD_TYPE_ADDRESS_HOME;
        case ADDRESS_TYPE_IDX_WORK: return VCARD_FIELD_TYPE_ADDRESS_WORK;
        default:                    return VCARD_FIELD_TYPE_ADDRESS_HOME;
    }
}

static void vcard_refresh_list(void) {
    g_vcard_list_count = 0;
    g_vcard_selected_slot = 0xFFFF;
    g_vcard_selected_is_own = false;

    if (vcard_store_has_own()) {
        snprintf(g_vcard_list_labels[g_vcard_list_count], sizeof(g_vcard_list_labels[g_vcard_list_count]),
                 "%s", i18n_str(STR_MY_VCARD));
        g_vcard_list_items[g_vcard_list_count].label = g_vcard_list_labels[g_vcard_list_count];
        g_vcard_list_slots[g_vcard_list_count] = 0xFFFF;
        g_vcard_list_count++;
    }

    uint16_t sorted[VCARD_MAX_CARDS];
    uint16_t count = vcard_store_get_sorted(sorted, VCARD_MAX_CARDS);
    for (uint16_t i = 0; i < count && g_vcard_list_count < (VCARD_MAX_CARDS + 1); i++) {
        char name[VIEW_MAX_TEXT_LEN];
        if (vcard_store_get_display(sorted[i], name, sizeof(name))) {
            snprintf(g_vcard_list_labels[g_vcard_list_count], sizeof(g_vcard_list_labels[g_vcard_list_count]),
                     "%s", name);
        } else {
            snprintf(g_vcard_list_labels[g_vcard_list_count], sizeof(g_vcard_list_labels[g_vcard_list_count]),
                     "%s", i18n_str(STR_VCARDS));
        }
        g_vcard_list_items[g_vcard_list_count].label = g_vcard_list_labels[g_vcard_list_count];
        g_vcard_list_slots[g_vcard_list_count] = sorted[i];
        g_vcard_list_count++;
    }

    if (g_vcard_list_count == 0) {
        g_vcard_list_items[0].label = i18n_str(STR_NO_VCARDS);
        g_vcard_list_slots[0] = 0xFFFF;
        g_vcard_list_count = 1;
    }
}

static bool vcard_build_from_editor(char *out, size_t max_len, char *err, size_t err_len) {
    if (!out || max_len == 0) return false;
    size_t used = 0;

    auto append = [&](const char *fmt, ...) -> bool {
        va_list ap;
        va_start(ap, fmt);
        int n = vsnprintf(out + used, max_len - used, fmt, ap);
        va_end(ap);
        if (n < 0 || (size_t)n >= max_len - used) {
            if (err && err_len > 0) snprintf(err, err_len, "vCard too large");
            return false;
        }
        used += (size_t)n;
        return true;
    };

    if (!append("BEGIN:VCARD\n")) return false;
    if (!append("VERSION:4.0\n")) return false;
    // Only include N and FN fields if at least one name is set
    if (g_vcard_editor.first[0] || g_vcard_editor.last[0]) {
        if (!append("N:%s;%s;;;\n", g_vcard_editor.last, g_vcard_editor.first)) return false;
        // FN: trim leading/trailing spaces when one name is empty
        if (g_vcard_editor.first[0] && g_vcard_editor.last[0]) {
            if (!append("FN:%s %s\n", g_vcard_editor.first, g_vcard_editor.last)) return false;
        } else if (g_vcard_editor.first[0]) {
            if (!append("FN:%s\n", g_vcard_editor.first)) return false;
        } else {
            if (!append("FN:%s\n", g_vcard_editor.last)) return false;
        }
    }
    if (g_vcard_editor.note[0]) {
        if (!append("NOTE:%s\n", g_vcard_editor.note)) return false;
    }

    for (uint8_t i = 0; i < g_vcard_editor.extra_count; i++) {
        const char *val = g_vcard_editor.extra_value[i];
        if (!val[0]) continue;
        switch (g_vcard_editor.extra_type[i]) {
            // Phone types
            case VCARD_FIELD_TYPE_PHONE_LANDLINE:
                if (!append("TEL;TYPE=HOME:%s\n", val)) return false;
                break;
            case VCARD_FIELD_TYPE_PHONE_MOBILE:
                if (!append("TEL;TYPE=CELL:%s\n", val)) return false;
                break;
            case VCARD_FIELD_TYPE_PHONE_BUSINESS:
                if (!append("TEL;TYPE=WORK:%s\n", val)) return false;
                break;
            case VCARD_FIELD_TYPE_PHONE_PAGER:
                if (!append("TEL;TYPE=PAGER:%s\n", val)) return false;
                break;
            // Email, URL, Org, Title
            case VCARD_FIELD_TYPE_EMAIL:
                if (!append("EMAIL:%s\n", val)) return false;
                break;
            case VCARD_FIELD_TYPE_URL:
                if (!append("URL:%s\n", val)) return false;
                break;
            case VCARD_FIELD_TYPE_ORG:
                if (!append("ORG:%s\n", val)) return false;
                break;
            case VCARD_FIELD_TYPE_TITLE:
                if (!append("TITLE:%s\n", val)) return false;
                break;
            case VCARD_FIELD_TYPE_SOCIAL:
                if (!append("X-SOCIALPROFILE:%s\n", val)) return false;
                break;
            // IMPP types
            case VCARD_FIELD_TYPE_IMPP_TELEGRAM:
                if (!append("IMPP:telegram:%s\n", val)) return false;
                break;
            case VCARD_FIELD_TYPE_IMPP_SIGNAL:
                if (!append("IMPP:signal:%s\n", val)) return false;
                break;
            case VCARD_FIELD_TYPE_IMPP_WHATSAPP:
                if (!append("IMPP:whatsapp:%s\n", val)) return false;
                break;
            case VCARD_FIELD_TYPE_IMPP_DISCORD:
                if (!append("IMPP:discord:%s\n", val)) return false;
                break;
            case VCARD_FIELD_TYPE_IMPP_MATRIX:
                if (!append("IMPP:matrix:%s\n", val)) return false;
                break;
            case VCARD_FIELD_TYPE_IMPP_THREEMA:
                if (!append("IMPP:threema:%s\n", val)) return false;
                break;
            case VCARD_FIELD_TYPE_IMPP_OTHER:
                if (!append("IMPP:%s\n", val)) return false;
                break;
            // Address types
            case VCARD_FIELD_TYPE_ADDRESS_HOME:
                if (!append("ADR;TYPE=HOME:%s\n", val)) return false;
                break;
            case VCARD_FIELD_TYPE_ADDRESS_WORK:
                if (!append("ADR;TYPE=WORK:%s\n", val)) return false;
                break;
            default:
                break;
        }
    }

    if (!append("END:VCARD\n")) return false;
    return true;
}

static bool vcard_load_selected(char *out, size_t max_len) {
    if (!out || max_len == 0) return false;
    if (g_vcard_selected_is_own) {
        return vcard_store_get_own(out, max_len) > 0;
    }
    if (g_vcard_selected_slot == 0xFFFF) {
        return false;
    }
    return vcard_store_get(g_vcard_selected_slot, out, max_len) > 0;
}

static app_state_t g_vcard_qr_return_state = APP_STATE_LOCK_SCREEN;

static void vcard_show_qr(const char *vcard, const char *name, app_state_t return_state) {
    if (!vcard || !vcard[0]) return;
    strncpy(g_vcard_qr_buf, vcard, sizeof(g_vcard_qr_buf) - 1);
    g_vcard_qr_buf[sizeof(g_vcard_qr_buf) - 1] = '\0';
    // Filter empty fields to make QR code smaller
    size_t len = strlen(g_vcard_qr_buf);
    vcard_filter_empty_fields(g_vcard_qr_buf, len);
    if (name && name[0]) {
        strncpy(g_vcard_qr_name, name, sizeof(g_vcard_qr_name) - 1);
        g_vcard_qr_name[sizeof(g_vcard_qr_name) - 1] = '\0';
    } else {
        g_vcard_qr_name[0] = '\0';
    }
    view_qr_code_init(&g_qr_view, i18n_str(STR_VCARD_QR),
                      g_vcard_qr_name[0] ? g_vcard_qr_name : NULL,
                      g_vcard_qr_buf);
    g_vcard_qr_return_state = return_state;
    g_app_state = APP_STATE_VCARD_QR;
    render_current_state(false);
}
#endif

static uint32_t ip_octets_to_u32(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return ((uint32_t)d << 24) | ((uint32_t)c << 16) | ((uint32_t)b << 8) | (uint32_t)a;
}

static void ip_input_to_string(const view_ip_input_t *view, char *out, size_t out_len) {
    if (!view || !out || out_len == 0) return;
    snprintf(out, out_len, "%u.%u.%u.%u",
             view->octet[0], view->octet[1], view->octet[2], view->octet[3]);
}

static bool ip_string_to_u32(const char *ip_str, uint32_t *out) {
    if (!ip_str || !out) return false;
    int a = 0;
    int b = 0;
    int c = 0;
    int d = 0;
    if (sscanf(ip_str, "%d.%d.%d.%d", &a, &b, &c, &d) != 4) {
        return false;
    }
    if (a < 0 || a > 255 || b < 0 || b > 255 || c < 0 || c > 255 || d < 0 || d > 255) {
        return false;
    }
    *out = ip_octets_to_u32((uint8_t)a, (uint8_t)b, (uint8_t)c, (uint8_t)d);
    return true;
}

static bool ip_string_empty(const char *ip_str) {
    return !ip_str || ip_str[0] == '\0';
}

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

void handle_key(char key) {
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
            } else if (key == '3') {
                // Open quick menu (limited - WiFi/BLE moved to main menu)
                g_lock_quick_items[0] = {"Light", 1};
                uint8_t quick_count = 1;
#if FEATURE_BLE_BADGE
                g_lock_quick_items[quick_count++] = {i18n_str(STR_VCARD_QR), 3};
#endif
                g_lock_quick_items[quick_count++] = {i18n_str(STR_SLEEP), 2};
                view_context_menu_init(&g_lock_quick_menu, "Quick Menu", g_lock_quick_items, quick_count);
                view_context_menu_show(&g_lock_quick_menu);
                g_app_state = APP_STATE_LOCK_QUICK_MENU;
            }
            break;

        case APP_STATE_LOCK_QUICK_MENU:
            if (key == '2') {
                view_context_menu_navigate(&g_lock_quick_menu, false);
                view_context_menu_render(&g_lock_quick_menu);
            } else if (key == '8') {
                view_context_menu_navigate(&g_lock_quick_menu, true);
                view_context_menu_render(&g_lock_quick_menu);
            } else if (key == 'Y') {
                uint8_t action = view_context_menu_get_action(&g_lock_quick_menu);
                view_context_menu_hide(&g_lock_quick_menu);
                switch (action) {
                    case 1:  // Light
                        g_backlight_forced_on = !g_backlight_forced_on;
                        if (g_backlight_forced_on) {
                            gui_backlight_on();
                        } else {
                            gui_backlight_off();
                        }
                        LOG_I("KEY", "Backlight forced: %s", g_backlight_forced_on ? "ON" : "OFF");
                        break;
                    case 2:  // Deep Sleep
                        if (power_is_usb_connected()) {
                            view_toast_error(i18n_str(STR_UNPLUG_USB), 1500);
                        } else {
                            enter_deep_sleep();  // Does not return
                        }
                        break;
#if FEATURE_BLE_BADGE
                    case 3:  // QR vCard
                        if (vcard_store_get_own(g_vcard_view_buf, sizeof(g_vcard_view_buf)) > 0) {
                            char name_buf[64] = {0};
                            vcard_store_get_display_own(name_buf, sizeof(name_buf));
                            vcard_show_qr(g_vcard_view_buf, name_buf, APP_STATE_LOCK_SCREEN);
                            return;  // Don't fall through to lock screen
                        }
                        view_toast_error(i18n_str(STR_NO_VCARDS), 1000);
                        break;
#endif
                    default:  // Cancel (0)
                        break;
                }
                update_lock_screen_data();
                g_app_state = APP_STATE_LOCK_SCREEN;
                render_current_state(false);
            } else if (key == 'N') {
                view_context_menu_hide(&g_lock_quick_menu);
                g_app_state = APP_STATE_LOCK_SCREEN;
                render_current_state(false);
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
#if FEATURE_CA
                if (sel == MENU_IDX_CA) {
                    build_ca_menu();
                    view_list_screen_init(&g_ca_menu, i18n_str(STR_CA_MENU), g_ca_items, CA_IDX_COUNT);
                    g_app_state = APP_STATE_CA_MENU;
                    render_current_state(false);
                } else
#endif
#if FEATURE_BLE_BADGE
                if (sel == MENU_IDX_REMOTE_BADGE) {
                    build_remote_badge_menu();
                    view_list_screen_init(&g_remote_badge_menu, i18n_str(STR_REMOTE_BADGE),
                                          g_remote_badge_items, REMOTE_BADGE_IDX_COUNT);
                    g_app_state = APP_STATE_REMOTE_BADGE_MENU;
                    render_current_state(false);
                } else
#endif
                if (sel == MENU_IDX_TOOLS) {
                    build_tools_menu();
                    view_list_screen_init(&g_tools_menu, i18n_str(STR_TOOLS), g_tools_items, g_tools_item_count);
                    g_app_state = APP_STATE_TOOLS_MENU;
                    render_current_state(false);
                } else if (sel == MENU_IDX_SETTINGS) {
                    go_to_settings_menu();
                }
                // Deep Sleep moved to lock screen quick menu (key 3)
            } else if (key == '3') {
                // Open quick menu (Light/WiFi/Sleep)
                g_main_quick_items[0] = {"Light", 1};
                g_main_quick_items[1] = {i18n_str(STR_WIFI_MENU), 2};
                g_main_quick_items[2] = {i18n_str(STR_SLEEP), 3};
                view_context_menu_init(&g_main_quick_menu, "Quick Menu", g_main_quick_items, 3);
                view_context_menu_show(&g_main_quick_menu);
                g_app_state = APP_STATE_MAIN_QUICK_MENU;
            } else if (key == 'N') {
                go_to_lock_screen();
            }
            break;

        case APP_STATE_MAIN_QUICK_MENU:
            if (key == '2') {
                view_context_menu_navigate(&g_main_quick_menu, false);
                view_context_menu_render(&g_main_quick_menu);
            } else if (key == '8') {
                view_context_menu_navigate(&g_main_quick_menu, true);
                view_context_menu_render(&g_main_quick_menu);
            } else if (key == 'Y') {
                uint8_t action = view_context_menu_get_action(&g_main_quick_menu);
                view_context_menu_hide(&g_main_quick_menu);
                switch (action) {
                    case 1:  // Light
                        g_backlight_forced_on = !g_backlight_forced_on;
                        if (g_backlight_forced_on) {
                            gui_backlight_on();
                            view_toast_show("Light ON", 800);
                        } else {
                            gui_backlight_off();
                            view_toast_show("Light OFF", 800);
                        }
                        LOG_I("KEY", "Backlight forced: %s", g_backlight_forced_on ? "ON" : "OFF");
                        break;
                    case 2:  // WiFi
                        if (wifi_manager_get_state() == WIFI_STATE_CONNECTED) {
                            wifi_manager_disconnect();
                            wifi_manager_deinit();
                            view_toast_show(i18n_str(STR_WIFI_DISCONNECTED), 1000);
                        } else if (wifi_manager_has_config()) {
#if FEATURE_BLE_UART || FEATURE_BLE_BADGE
                            if (bluetooth_is_active()) {
                                view_toast_error(i18n_str(STR_WIFI_DISABLE_BLUETOOTH), 1500);
                                break;
                            }
#endif
                            if (!wifi_manager_is_init()) {
                                wifi_manager_init();
                            }
                            wifi_config_stored_t config;
                            wifi_manager_load_config(&config);
                            wifi_manager_connect(&config);
                            view_toast_show(i18n_str(STR_WIFI_CONNECTING), 1000);
                        } else {
                            view_toast_error(i18n_str(STR_WIFI_NO_CONFIG), 1000);
                        }
                        break;
                    case 3:  // Deep Sleep
                        if (power_is_usb_connected()) {
                            view_toast_error(i18n_str(STR_UNPLUG_USB), 1500);
                        } else {
                            enter_deep_sleep();  // Does not return
                        }
                        break;
                    default:  // Cancel (0)
                        break;
                }
                g_app_state = APP_STATE_MAIN_MENU;
                render_current_state(false);
            } else if (key == 'N') {
                view_context_menu_hide(&g_main_quick_menu);
                g_app_state = APP_STATE_MAIN_MENU;
                render_current_state(false);
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

                    case SETTINGS_IDX_SET_DATETIME: {
                        // Start Date/Time wizard: Date first, then Time
                        struct tm timeinfo;
                        if (cdc_rtc_is_time_set()) {
                            cdc_rtc_get_time(&timeinfo);
                        } else {
                            timeinfo.tm_mday = 1;
                            timeinfo.tm_mon = 0;
                            timeinfo.tm_year = 125;  // 2025
                            timeinfo.tm_hour = 12;
                            timeinfo.tm_min = 0;
                        }
                        // Initialize both views (time values preserved for wizard step 2)
                        view_date_input_init(&g_date_input,
                                             timeinfo.tm_mday,
                                             timeinfo.tm_mon + 1,
                                             timeinfo.tm_year + 1900);
                        view_time_input_init(&g_time_input,
                                             timeinfo.tm_hour,
                                             timeinfo.tm_min);
                        g_app_state = APP_STATE_SET_DATE;
                        render_current_state(false);
                        break;
                    }

                    case SETTINGS_IDX_LANGUAGE: {
                        // Build language list
                        for (int i = 0; i < LANG_COUNT; i++) {
                            g_language_items[i].label = i18n_get_language_name((language_t)i);
                        }
                        view_list_screen_init(&g_language_menu, i18n_str(STR_LANGUAGE), g_language_items, LANG_COUNT);
                        // Pre-select current language
                        g_language_menu.selection = i18n_get_language();
                        g_app_state = APP_STATE_LANGUAGE;
                        render_current_state(false);
                        break;
                    }
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
                // Date confirmed - advance to Time input (wizard step 2)
                g_app_state = APP_STATE_SET_TIME;
                render_current_state(false);
            } else if (key == 'N') {
                // N = clear current field, or go back if at start
                if (!view_date_input_clear_field(&g_date_input)) {
                    // At start of first field - cancel wizard
                    go_to_settings_menu();
                } else {
                    render_current_state(true);
                }
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
                // Save both date and time to RTC
                cdc_rtc_set_date(g_date_input.year, g_date_input.month, g_date_input.day);
                cdc_rtc_set_time(g_time_input.hour, g_time_input.minute, 0);
                g_last_minute = -1;  // Force clock update
                view_toast_success(i18n_str(STR_SAVED), 1000);
                go_to_settings_menu();
            } else if (key == 'N') {
                // N = clear current field, or go back to date if at start
                if (!view_time_input_clear_field(&g_time_input)) {
                    // At start of first field - go back to date
                    g_app_state = APP_STATE_SET_DATE;
                    render_current_state(false);
                } else {
                    render_current_state(true);
                }
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
                    build_fido_context_menu();
                    view_context_menu_init(&g_context_menu, info.rp_id, g_fido_context_items_buf, g_fido_context_items_count);
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
                view_wifi_list_navigate(&g_wifi_list, false);
                render_current_state(true);
            } else if (key == '8') {
                view_wifi_list_navigate(&g_wifi_list, true);
                render_current_state(true);
            } else if (key == 'Y' || key == '5') {
                // Select network or show context menu
                g_wifi_selected_index = view_wifi_list_get_selection(&g_wifi_list);
                wifi_network_t net;
                if (wifi_manager_get_network(g_wifi_selected_index, &net)) {
                    view_context_menu_init(&g_wifi_context_menu, net.ssid, g_wifi_context_items, 3);
                    view_context_menu_show(&g_wifi_context_menu);
                }
            } else if (key == '3') {
                // Context menu
                g_wifi_selected_index = view_wifi_list_get_selection(&g_wifi_list);
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
#if FEATURE_BLE_UART || FEATURE_BLE_BADGE
                    if (bluetooth_is_active()) {
                        view_toast_error(i18n_str(STR_WIFI_DISABLE_BLUETOOTH), 1500);
                        render_current_state(false);
                        break;
                    }
#endif
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
                    view_ip_input_init(&g_ip_input, i18n_str(STR_WIFI_STATIC), g_wifi_wizard.static_ip);
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
            if (key >= '0' && key <= '9') {
                if (view_ip_input_key(&g_ip_input, key)) {
                    render_current_state(true);
                }
            } else if (key == 'Y') {
                if (g_ip_input.field < 3) {
                    view_ip_input_next_field(&g_ip_input);
                    render_current_state(true);
                } else {
                    ip_input_to_string(&g_ip_input, g_wifi_wizard.static_ip, sizeof(g_wifi_wizard.static_ip));
                    if (ip_string_empty(g_wifi_wizard.gateway)) {
                        snprintf(g_wifi_wizard.gateway, sizeof(g_wifi_wizard.gateway), "%u.%u.%u.1",
                                 g_ip_input.octet[0], g_ip_input.octet[1], g_ip_input.octet[2]);
                    }
                    if (ip_string_empty(g_wifi_wizard.subnet)) {
                        strncpy(g_wifi_wizard.subnet, "255.255.255.0", sizeof(g_wifi_wizard.subnet));
                        g_wifi_wizard.subnet[sizeof(g_wifi_wizard.subnet) - 1] = '\0';
                    }
                    view_ip_input_init(&g_ip_input, i18n_str(STR_WIFI_GATEWAY), g_wifi_wizard.gateway);
                    g_app_state = APP_STATE_WIFI_ADD_GATEWAY;
                    render_current_state(false);
                }
            } else if (key == 'N') {
                if (view_ip_input_clear_field(&g_ip_input)) {
                    render_current_state(true);
                } else {
                    build_wifi_ip_menu();
                    view_list_screen_init(&g_wifi_ip_menu, i18n_str(STR_WIFI_IP_MODE), g_wifi_ip_items, 2);
                    g_app_state = APP_STATE_WIFI_ADD_IP_MODE;
                    render_current_state(false);
                }
            }
            break;

        case APP_STATE_WIFI_ADD_GATEWAY:
            if (key >= '0' && key <= '9') {
                if (view_ip_input_key(&g_ip_input, key)) {
                    render_current_state(true);
                }
            } else if (key == 'Y') {
                if (g_ip_input.field < 3) {
                    view_ip_input_next_field(&g_ip_input);
                    render_current_state(true);
                } else {
                    ip_input_to_string(&g_ip_input, g_wifi_wizard.gateway, sizeof(g_wifi_wizard.gateway));
                    if (ip_string_empty(g_wifi_wizard.subnet)) {
                        strncpy(g_wifi_wizard.subnet, "255.255.255.0", sizeof(g_wifi_wizard.subnet));
                        g_wifi_wizard.subnet[sizeof(g_wifi_wizard.subnet) - 1] = '\0';
                    }
                    view_ip_input_init(&g_ip_input, i18n_str(STR_WIFI_NETMASK), g_wifi_wizard.subnet);
                    g_app_state = APP_STATE_WIFI_ADD_NETMASK;
                    render_current_state(false);
                }
            } else if (key == 'N') {
                if (view_ip_input_clear_field(&g_ip_input)) {
                    render_current_state(true);
                } else {
                    view_ip_input_init(&g_ip_input, i18n_str(STR_WIFI_STATIC), g_wifi_wizard.static_ip);
                    g_app_state = APP_STATE_WIFI_ADD_STATIC_IP;
                    render_current_state(false);
                }
            }
            break;

        case APP_STATE_WIFI_ADD_NETMASK:
            if (key >= '0' && key <= '9') {
                if (view_ip_input_key(&g_ip_input, key)) {
                    render_current_state(true);
                }
            } else if (key == 'Y') {
                if (g_ip_input.field < 3) {
                    view_ip_input_next_field(&g_ip_input);
                    render_current_state(true);
                } else {
                    ip_input_to_string(&g_ip_input, g_wifi_wizard.subnet, sizeof(g_wifi_wizard.subnet));
#if FEATURE_BLE_UART || FEATURE_BLE_BADGE
                    if (bluetooth_is_active()) {
                        view_toast_error(i18n_str(STR_WIFI_DISABLE_BLUETOOTH), 1500);
                        render_current_state(false);
                        break;
                    }
#endif
                    wifi_config_stored_t config = {};
                    strncpy(config.ssid, g_wifi_wizard.ssid, WIFI_SSID_MAX_LEN);
                    strncpy(config.password, g_wifi_wizard.password, WIFI_PASSWORD_MAX_LEN);
                    config.auth_mode = g_wifi_wizard.auth_mode;
                    config.use_dhcp = false;
                    uint32_t ip = 0;
                    uint32_t gateway = 0;
                    uint32_t subnet = 0;
                    if (ip_string_to_u32(g_wifi_wizard.static_ip, &ip)) {
                        config.static_ip = ip;
                    }
                    if (ip_string_to_u32(g_wifi_wizard.gateway, &gateway)) {
                        config.gateway = gateway;
                    }
                    if (ip_string_to_u32(g_wifi_wizard.subnet, &subnet)) {
                        config.subnet = subnet;
                    }
                    config.dns = 0x08080808;  // 8.8.8.8
                    wifi_manager_connect(&config);
                    g_wifi_connect_start = millis();
                    view_info_screen_init(&g_info_view, g_wifi_wizard.ssid, i18n_str(STR_WIFI_CONNECTING));
                    g_app_state = APP_STATE_WIFI_CONNECTING;
                    render_current_state(false);
                }
            } else if (key == 'N') {
                if (view_ip_input_clear_field(&g_ip_input)) {
                    render_current_state(true);
                } else {
                    view_ip_input_init(&g_ip_input, i18n_str(STR_WIFI_GATEWAY), g_wifi_wizard.gateway);
                    g_app_state = APP_STATE_WIFI_ADD_GATEWAY;
                    render_current_state(false);
                }
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
                view_list_screen_init(&g_tools_wifi_menu, i18n_str(STR_WIFI_MENU), g_tools_wifi_items, 4);
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
                switch (sel) {
                    case TOOLS_IDX_WIFI:
                        // WiFi submenu
                        build_tools_wifi_menu();
                        view_list_screen_init(&g_tools_wifi_menu, i18n_str(STR_WIFI_MENU), g_tools_wifi_items, 4);
                        g_app_state = APP_STATE_TOOLS_WIFI_MENU;
                        render_current_state(false);
                        break;
#if FEATURE_BLE_UART
                    case TOOLS_IDX_BLE_SERIAL:
                        // BLE Serial toggle
                        if (!g_ble_enabled) {
                            if (wifi_manager_is_init()) {
                                view_toast_error(i18n_str(STR_BLUETOOTH_DISABLE_WIFI), 1500);
                                break;
                            }
                            if (!ble_uart_is_initialized()) {
                                ble_uart_init();
                            }
                            ble_uart_set_power_mode(BLE_POWER_ACTIVE);
                            g_ble_enabled = true;
                            view_toast_show(i18n_str(STR_BLUETOOTH_ON), 1000);
                        } else {
                            ble_uart_set_power_mode(BLE_POWER_OFF);
                            ble_uart_deinit();
                            g_ble_enabled = false;
                            view_toast_show(i18n_str(STR_BLUETOOTH_OFF), 1000);
                        }
                        LOG_I("KEY", "BLE: %s", g_ble_enabled ? "ON" : "OFF");
                        g_tools_items[sel].icon_disabled = !g_ble_enabled;
                        render_current_state(false);
                        break;
#endif
                    case TOOLS_IDX_NTP:
                        // NTP Sync - check if WiFi already connected
#if FEATURE_BLE_UART || FEATURE_BLE_BADGE
                        if (bluetooth_is_active()) {
                            view_toast_error(i18n_str(STR_WIFI_DISABLE_BLUETOOTH), 1500);
                            render_current_state(false);
                            break;
                        }
#endif
                        if (wifi_manager_get_state() == WIFI_STATE_CONNECTED) {
                            // WiFi already connected - no session needed, just use existing connection
                            g_ntp_wifi_session = 0;  // No session = don't disconnect after
                            uint32_t ntp_server = wifi_manager_get_ntp_server();
                            ntp_sync_start(ntp_server);
                            g_ntp_sync_phase = 2;  // Skip to NTP sync phase
                            view_info_screen_init(&g_info_view, i18n_str(STR_NTP_SYNC), i18n_str(STR_NTP_SYNCING));
                            g_app_state = APP_STATE_TOOLS_NTP_SYNC;
                            render_current_state(false);
                        } else {
                            // Need to connect WiFi first - acquire session
                            if (!wifi_manager_has_config()) {
                                view_toast_error(i18n_str(STR_WIFI_NO_CONFIG), 1500);
                                render_current_state(false);
                                break;
                            }
                            // Acquire WiFi session for NTP
                            g_ntp_wifi_session = wifi_manager_session_acquire();
                            wifi_manager_init();
                            wifi_config_stored_t config;
                            wifi_manager_load_config(&config);
                            wifi_manager_connect(&config);
                            g_wifi_connect_start = millis();
                            g_ntp_sync_phase = 1;  // Connecting WiFi
                            view_info_screen_init(&g_info_view, i18n_str(STR_NTP_SYNC), i18n_str(STR_WIFI_CONNECTING));
                            g_app_state = APP_STATE_TOOLS_NTP_SYNC;
                            render_current_state(false);
                        }
                        break;
                    case TOOLS_IDX_SELFTEST:
                        build_selftest_text(g_selftest_buf, sizeof(g_selftest_buf));
                        view_info_screen_init(&g_info_view, "System Test", g_selftest_buf);
                        g_app_state = APP_STATE_SELFTEST;
                        render_current_state(false);
                        break;
                }
            } else if (key == 'N') {
                go_to_main_menu();
            }
            break;

#if FEATURE_BLE_BADGE
        // Remote Badge main menu (3 submenus: vCards, Broadcast, Settings)
        case APP_STATE_REMOTE_BADGE_MENU:
            if (key == '2') {
                view_list_screen_navigate(&g_remote_badge_menu, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_remote_badge_menu, true);
                render_current_state(true);
            } else if (key == 'Y' || key == '5') {
                uint8_t sel = view_list_screen_get_selection(&g_remote_badge_menu);
                switch (sel) {
                    case REMOTE_BADGE_IDX_VCARDS:
                        build_vcard_submenu();
                        view_list_screen_init(&g_vcard_submenu, i18n_str(STR_VCARDS),
                                              g_vcard_submenu_items, VCARD_SUB_IDX_COUNT);
                        g_app_state = APP_STATE_VCARD_SUBMENU;
                        render_current_state(false);
                        break;
                    case REMOTE_BADGE_IDX_BROADCAST:
                        build_broadcast_submenu();
                        broadcast_submenu_update_labels();
                        view_list_screen_init(&g_broadcast_submenu, i18n_str(STR_VCARD_BROADCAST),
                                              g_broadcast_submenu_items, BROADCAST_SUB_IDX_COUNT);
                        g_app_state = APP_STATE_BROADCAST_SUBMENU;
                        render_current_state(false);
                        break;
                    case REMOTE_BADGE_IDX_SETTINGS:
                        build_broadcast_settings_menu();
                        broadcast_settings_update_labels();
                        view_list_screen_init(&g_broadcast_settings_menu, i18n_str(STR_BROADCAST_SETTINGS),
                                              g_broadcast_settings_items, SETTINGS_SUB_IDX_COUNT);
                        g_app_state = APP_STATE_BROADCAST_SETTINGS;
                        render_current_state(false);
                        break;
                }
            } else if (key == 'N') {
                go_to_main_menu();
            }
            break;

        // vCards submenu (Edit, Exchange, List)
        case APP_STATE_VCARD_SUBMENU:
            if (key == '2') {
                view_list_screen_navigate(&g_vcard_submenu, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_vcard_submenu, true);
                render_current_state(true);
            } else if (key == 'Y' || key == '5') {
                uint8_t sel = view_list_screen_get_selection(&g_vcard_submenu);
                switch (sel) {
                    case VCARD_SUB_IDX_EDIT:
                        vcard_editor_reset();
                        if (vcard_store_has_own()) {
                            char existing[VCARD_MAX_LEN + 1];
                            if (vcard_store_get_own(existing, sizeof(existing)) > 0) {
                                vcard_editor_load_from_vcard(existing);
                            }
                        }
                        view_t9_input_init(&g_t9_input, i18n_str(STR_FIRST_NAME), g_vcard_editor.first);
                        g_app_state = APP_STATE_VCARD_ADD_FIRST;
                        render_current_state(false);
                        break;
                    case VCARD_SUB_IDX_EXCHANGE:
                        if (wifi_manager_is_init()) {
                            view_toast_error(i18n_str(STR_BLUETOOTH_DISABLE_WIFI), 1500);
                            break;
                        }
#if FEATURE_BLE_UART
                        if (bluetooth_is_active()) {
                            ble_uart_set_power_mode(BLE_POWER_OFF);
                            ble_uart_deinit();
                            g_ble_enabled = false;
                        }
#endif
                        ble_badge_init();
                        ble_badge_set_exchange_enabled(true);
                        view_info_screen_init(&g_info_view, i18n_str(STR_VCARD_EXCHANGE),
                                              i18n_str(STR_VCARD_EXCHANGE_MSG));
                        g_app_state = APP_STATE_VCARD_EXCHANGE;
                        render_current_state(false);
                        break;
                    case VCARD_SUB_IDX_LIST:
                        vcard_refresh_list();
                        view_list_screen_init(&g_vcard_list, i18n_str(STR_VCARD_LIST),
                                              g_vcard_list_items, g_vcard_list_count);
                        g_app_state = APP_STATE_VCARD_LIST;
                        render_current_state(false);
                        break;
                }
            } else if (key == 'N') {
                build_remote_badge_menu();
                view_list_screen_init(&g_remote_badge_menu, i18n_str(STR_REMOTE_BADGE),
                                      g_remote_badge_items, REMOTE_BADGE_IDX_COUNT);
                g_app_state = APP_STATE_REMOTE_BADGE_MENU;
                render_current_state(false);
            }
            break;

        // Exchange mode - scanning for other badges and advertising own vCard
        case APP_STATE_VCARD_EXCHANGE:
            if (key == 'N') {
                ble_badge_set_exchange_enabled(false);
                go_to_vcard_submenu();
            }
            break;

        // Broadcast submenu (Send, Receive toggles)
        case APP_STATE_BROADCAST_SUBMENU:
            if (key == '2') {
                view_list_screen_navigate(&g_broadcast_submenu, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_broadcast_submenu, true);
                render_current_state(true);
            } else if (key == 'Y' || key == '5') {
                uint8_t sel = view_list_screen_get_selection(&g_broadcast_submenu);
                if (wifi_manager_is_init()) {
                    view_toast_error(i18n_str(STR_BLUETOOTH_DISABLE_WIFI), 1500);
                    break;
                }
#if FEATURE_BLE_UART
                if (bluetooth_is_active()) {
                    ble_uart_set_power_mode(BLE_POWER_OFF);
                    ble_uart_deinit();
                    g_ble_enabled = false;
                }
#endif
                ble_badge_init();
                switch (sel) {
                    case BROADCAST_SUB_IDX_SEND:
                        ble_badge_set_adv_enabled(!ble_badge_is_adv_enabled());
                        break;
                    case BROADCAST_SUB_IDX_RECEIVE:
                        ble_badge_set_scan_enabled(!ble_badge_is_scan_enabled());
                        break;
                }
                broadcast_submenu_update_labels();
                render_current_state(true);
            } else if (key == 'N') {
                build_remote_badge_menu();
                view_list_screen_init(&g_remote_badge_menu, i18n_str(STR_REMOTE_BADGE),
                                      g_remote_badge_items, REMOTE_BADGE_IDX_COUNT);
                g_app_state = APP_STATE_REMOTE_BADGE_MENU;
                render_current_state(false);
            }
            break;

        // Broadcast settings submenu (Send interval, Scan interval)
        case APP_STATE_BROADCAST_SETTINGS:
            if (key == '2') {
                view_list_screen_navigate(&g_broadcast_settings_menu, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_broadcast_settings_menu, true);
                render_current_state(true);
            } else if (key == 'Y' || key == '5') {
                uint8_t sel = view_list_screen_get_selection(&g_broadcast_settings_menu);
                switch (sel) {
                    case SETTINGS_SUB_IDX_SEND_INTERVAL: {
                        uint32_t current = ble_badge_get_adv_interval() / 1000;
                        view_slider_init(&g_slider, i18n_str(STR_VCARD_ADV_INTERVAL),
                                         10, 600, current, 10, "%d", "s",
                                         i18n_str(STR_HINT_BRIGHTNESS));
                        g_app_state = APP_STATE_VCARD_ADV_INTERVAL;
                        render_current_state(false);
                        break;
                    }
                    case SETTINGS_SUB_IDX_SCAN_INTERVAL: {
                        uint32_t current = ble_badge_get_scan_interval() / 1000;
                        view_slider_init(&g_slider, i18n_str(STR_VCARD_SCAN_INTERVAL),
                                         10, 120, current, 10, "%d", "s",
                                         i18n_str(STR_HINT_BRIGHTNESS));
                        g_app_state = APP_STATE_VCARD_SCAN_INTERVAL;
                        render_current_state(false);
                        break;
                    }
                }
            } else if (key == 'N') {
                build_remote_badge_menu();
                view_list_screen_init(&g_remote_badge_menu, i18n_str(STR_REMOTE_BADGE),
                                      g_remote_badge_items, REMOTE_BADGE_IDX_COUNT);
                g_app_state = APP_STATE_REMOTE_BADGE_MENU;
                render_current_state(false);
            }
            break;

        case APP_STATE_VCARD_ADD_FIRST:
            if (key >= '0' && key <= '9') {
                view_t9_input_key(&g_t9_input, key);
                render_current_state(true);
            } else if (key == 'N') {
                if (g_t9_input.len > 0) {
                    view_t9_input_backspace(&g_t9_input);
                    render_current_state(true);
                } else {
                    vcard_editor_reset();
                    go_to_vcard_submenu();
                }
            } else if (key == 'Y') {
                strncpy(g_vcard_editor.first, view_t9_input_get_text(&g_t9_input), sizeof(g_vcard_editor.first) - 1);
                g_vcard_editor.first[sizeof(g_vcard_editor.first) - 1] = '\0';
                view_t9_input_init(&g_t9_input, i18n_str(STR_LAST_NAME), g_vcard_editor.last);
                g_app_state = APP_STATE_VCARD_ADD_LAST;
                render_current_state(false);
            }
            break;

        case APP_STATE_VCARD_ADD_LAST:
            if (key >= '0' && key <= '9') {
                view_t9_input_key(&g_t9_input, key);
                render_current_state(true);
            } else if (key == 'N') {
                if (g_t9_input.len > 0) {
                    view_t9_input_backspace(&g_t9_input);
                    render_current_state(true);
                } else {
                    view_t9_input_init(&g_t9_input, i18n_str(STR_FIRST_NAME), g_vcard_editor.first);
                    g_app_state = APP_STATE_VCARD_ADD_FIRST;
                    render_current_state(false);
                }
            } else if (key == 'Y') {
                strncpy(g_vcard_editor.last, view_t9_input_get_text(&g_t9_input), sizeof(g_vcard_editor.last) - 1);
                g_vcard_editor.last[sizeof(g_vcard_editor.last) - 1] = '\0';
                view_t9_input_init(&g_t9_input, i18n_str(STR_DESCRIPTION), g_vcard_editor.note);
                g_app_state = APP_STATE_VCARD_ADD_NOTE;
                render_current_state(false);
            }
            break;

        case APP_STATE_VCARD_ADD_NOTE:
            if (key >= '0' && key <= '9') {
                view_t9_input_key(&g_t9_input, key);
                render_current_state(true);
            } else if (key == 'N') {
                if (g_t9_input.len > 0) {
                    view_t9_input_backspace(&g_t9_input);
                    render_current_state(true);
                } else {
                    view_t9_input_init(&g_t9_input, i18n_str(STR_LAST_NAME), g_vcard_editor.last);
                    g_app_state = APP_STATE_VCARD_ADD_LAST;
                    render_current_state(false);
                }
            } else if (key == 'Y') {
                strncpy(g_vcard_editor.note, view_t9_input_get_text(&g_t9_input), sizeof(g_vcard_editor.note) - 1);
                g_vcard_editor.note[sizeof(g_vcard_editor.note) - 1] = '\0';
                vcard_fields_init_menu();
                view_list_screen_init(&g_vcard_field_menu, i18n_str(STR_VCARD_ADD),
                                      g_vcard_field_items, VCARD_FIELD_CAT_COUNT + 1);
                g_app_state = APP_STATE_VCARD_FIELD_MENU;
                render_current_state(false);
            }
            break;

        case APP_STATE_VCARD_FIELD_MENU:
            if (key == '2') {
                view_list_screen_navigate(&g_vcard_field_menu, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_vcard_field_menu, true);
                render_current_state(true);
            } else if (key == 'Y' || key == '5') {
                uint8_t sel = view_list_screen_get_selection(&g_vcard_field_menu);
                if (sel == VCARD_FIELD_CAT_COUNT) {
                    // Save selected
                    char vcard_out[VCARD_MAX_LEN + 1];
                    char err[64] = {0};
                    if (vcard_build_from_editor(vcard_out, sizeof(vcard_out), err, sizeof(err)) &&
                        vcard_store_set_own(vcard_out, strlen(vcard_out), err, sizeof(err))) {
                        view_toast_success(i18n_str(STR_SAVED), 1000);
                    } else {
                        view_toast_error(err[0] ? err : i18n_str(STR_SAVE_FAILED), 1500);
                    }
                    go_to_vcard_submenu();
                } else if (sel == VCARD_FIELD_CAT_PHONE) {
                    // Phone -> submenu
                    g_vcard_field_category = sel;
                    vcard_phone_type_init_menu();
                    view_list_screen_init(&g_phone_type_menu, i18n_str(STR_PHONE),
                                          g_phone_type_items, PHONE_TYPE_IDX_COUNT);
                    g_app_state = APP_STATE_VCARD_PHONE_TYPE;
                    render_current_state(false);
                } else if (sel == VCARD_FIELD_CAT_IMPP) {
                    // IMPP -> submenu
                    g_vcard_field_category = sel;
                    vcard_impp_type_init_menu();
                    view_list_screen_init(&g_impp_type_menu, i18n_str(STR_IMPP),
                                          g_impp_type_items, IMPP_TYPE_IDX_COUNT);
                    g_app_state = APP_STATE_VCARD_IMPP_TYPE;
                    render_current_state(false);
                } else if (sel == VCARD_FIELD_CAT_ADDRESS) {
                    // Address -> submenu
                    g_vcard_field_category = sel;
                    vcard_address_type_init_menu();
                    view_list_screen_init(&g_address_type_menu, i18n_str(STR_ADDRESS),
                                          g_address_type_items, ADDRESS_TYPE_IDX_COUNT);
                    g_app_state = APP_STATE_VCARD_ADDRESS_TYPE;
                    render_current_state(false);
                } else {
                    // Direct entry for Email, URL, Org, Title, Social
                    if (g_vcard_editor.extra_count >= VCARD_EDITOR_MAX_EXTRAS) {
                        view_toast_error(i18n_str(STR_VCARD_TOO_BIG), 1500);
                        break;
                    }
                    g_vcard_field_category = sel;
                    uint8_t field_type = VCARD_FIELD_TYPE_EMAIL;
                    switch (sel) {
                        case VCARD_FIELD_CAT_EMAIL: field_type = VCARD_FIELD_TYPE_EMAIL; break;
                        case VCARD_FIELD_CAT_URL:   field_type = VCARD_FIELD_TYPE_URL; break;
                        case VCARD_FIELD_CAT_ORG:   field_type = VCARD_FIELD_TYPE_ORG; break;
                        case VCARD_FIELD_CAT_TITLE: field_type = VCARD_FIELD_TYPE_TITLE; break;
                        case VCARD_FIELD_CAT_SOCIAL: field_type = VCARD_FIELD_TYPE_SOCIAL; break;
                    }
                    g_vcard_editor.extra_edit_index = g_vcard_editor.extra_count++;
                    g_vcard_editor.extra_type[g_vcard_editor.extra_edit_index] = field_type;
                    g_vcard_editor.extra_value[g_vcard_editor.extra_edit_index][0] = '\0';
                    const char *title = g_vcard_field_items[sel].label;
                    view_t9_input_init(&g_t9_input, title, "");
                    g_app_state = APP_STATE_VCARD_FIELD_VALUE;
                    render_current_state(false);
                }
            } else if (key == 'N') {
                go_to_vcard_submenu();
            }
            break;

        case APP_STATE_VCARD_PHONE_TYPE:
            if (key == '2') {
                view_list_screen_navigate(&g_phone_type_menu, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_phone_type_menu, true);
                render_current_state(true);
            } else if (key == 'Y' || key == '5') {
                uint8_t sel = view_list_screen_get_selection(&g_phone_type_menu);
                if (g_vcard_editor.extra_count >= VCARD_EDITOR_MAX_EXTRAS) {
                    view_toast_error(i18n_str(STR_VCARD_TOO_BIG), 1500);
                    break;
                }
                g_vcard_field_subtype = sel;
                g_vcard_editor.extra_edit_index = g_vcard_editor.extra_count++;
                g_vcard_editor.extra_type[g_vcard_editor.extra_edit_index] = phone_type_to_field_type(sel);
                g_vcard_editor.extra_value[g_vcard_editor.extra_edit_index][0] = '\0';
                view_t9_input_init(&g_t9_input, g_phone_type_items[sel].label, "");
                g_app_state = APP_STATE_VCARD_FIELD_VALUE;
                render_current_state(false);
            } else if (key == 'N') {
                vcard_fields_init_menu();
                view_list_screen_init(&g_vcard_field_menu, i18n_str(STR_VCARD_ADD),
                                      g_vcard_field_items, VCARD_FIELD_CAT_COUNT + 1);
                g_app_state = APP_STATE_VCARD_FIELD_MENU;
                render_current_state(false);
            }
            break;

        case APP_STATE_VCARD_IMPP_TYPE:
            if (key == '2') {
                view_list_screen_navigate(&g_impp_type_menu, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_impp_type_menu, true);
                render_current_state(true);
            } else if (key == 'Y' || key == '5') {
                uint8_t sel = view_list_screen_get_selection(&g_impp_type_menu);
                if (g_vcard_editor.extra_count >= VCARD_EDITOR_MAX_EXTRAS) {
                    view_toast_error(i18n_str(STR_VCARD_TOO_BIG), 1500);
                    break;
                }
                g_vcard_field_subtype = sel;
                g_vcard_editor.extra_edit_index = g_vcard_editor.extra_count++;
                g_vcard_editor.extra_type[g_vcard_editor.extra_edit_index] = impp_type_to_field_type(sel);
                g_vcard_editor.extra_value[g_vcard_editor.extra_edit_index][0] = '\0';
                view_t9_input_init(&g_t9_input, g_impp_type_items[sel].label, "");
                g_app_state = APP_STATE_VCARD_FIELD_VALUE;
                render_current_state(false);
            } else if (key == 'N') {
                vcard_fields_init_menu();
                view_list_screen_init(&g_vcard_field_menu, i18n_str(STR_VCARD_ADD),
                                      g_vcard_field_items, VCARD_FIELD_CAT_COUNT + 1);
                g_app_state = APP_STATE_VCARD_FIELD_MENU;
                render_current_state(false);
            }
            break;

        case APP_STATE_VCARD_ADDRESS_TYPE:
            if (key == '2') {
                view_list_screen_navigate(&g_address_type_menu, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_address_type_menu, true);
                render_current_state(true);
            } else if (key == 'Y' || key == '5') {
                uint8_t sel = view_list_screen_get_selection(&g_address_type_menu);
                if (g_vcard_editor.extra_count >= VCARD_EDITOR_MAX_EXTRAS) {
                    view_toast_error(i18n_str(STR_VCARD_TOO_BIG), 1500);
                    break;
                }
                g_vcard_field_subtype = sel;
                g_vcard_editor.extra_edit_index = g_vcard_editor.extra_count++;
                g_vcard_editor.extra_type[g_vcard_editor.extra_edit_index] = address_type_to_field_type(sel);
                g_vcard_editor.extra_value[g_vcard_editor.extra_edit_index][0] = '\0';
                view_t9_input_init(&g_t9_input, g_address_type_items[sel].label, "");
                g_app_state = APP_STATE_VCARD_FIELD_VALUE;
                render_current_state(false);
            } else if (key == 'N') {
                vcard_fields_init_menu();
                view_list_screen_init(&g_vcard_field_menu, i18n_str(STR_VCARD_ADD),
                                      g_vcard_field_items, VCARD_FIELD_CAT_COUNT + 1);
                g_app_state = APP_STATE_VCARD_FIELD_MENU;
                render_current_state(false);
            }
            break;

        case APP_STATE_VCARD_FIELD_VALUE:
            if (key >= '0' && key <= '9') {
                view_t9_input_key(&g_t9_input, key);
                render_current_state(true);
            } else if (key == 'N') {
                if (g_t9_input.len > 0) {
                    view_t9_input_backspace(&g_t9_input);
                    render_current_state(true);
                } else {
                    if (g_vcard_editor.extra_count > 0 &&
                        g_vcard_editor.extra_edit_index + 1 == g_vcard_editor.extra_count) {
                        g_vcard_editor.extra_count--;
                    }
                    vcard_fields_init_menu();
                    view_list_screen_init(&g_vcard_field_menu, i18n_str(STR_VCARD_ADD),
                                          g_vcard_field_items, VCARD_FIELD_CAT_COUNT + 1);
                    g_app_state = APP_STATE_VCARD_FIELD_MENU;
                    render_current_state(false);
                }
            } else if (key == 'Y') {
                uint8_t idx = g_vcard_editor.extra_edit_index;
                strncpy(g_vcard_editor.extra_value[idx], view_t9_input_get_text(&g_t9_input),
                        sizeof(g_vcard_editor.extra_value[idx]) - 1);
                g_vcard_editor.extra_value[idx][sizeof(g_vcard_editor.extra_value[idx]) - 1] = '\0';
                vcard_fields_init_menu();
                view_list_screen_init(&g_vcard_field_menu, i18n_str(STR_VCARD_ADD),
                                      g_vcard_field_items, VCARD_FIELD_CAT_COUNT + 1);
                g_app_state = APP_STATE_VCARD_FIELD_MENU;
                render_current_state(false);
            }
            break;

        case APP_STATE_VCARD_ADV_INTERVAL:
            if (key == '2' || key == '6') {
                view_slider_adjust(&g_slider, true);
                render_current_state(true);
            } else if (key == '4' || key == '8') {
                view_slider_adjust(&g_slider, false);
                render_current_state(true);
            } else if (key == 'Y') {
                uint16_t val = view_slider_get_value(&g_slider);
                ble_badge_set_adv_interval((uint32_t)val * 1000);
                build_broadcast_settings_menu();
                broadcast_settings_update_labels();
                view_list_screen_init(&g_broadcast_settings_menu, i18n_str(STR_BROADCAST_SETTINGS),
                                      g_broadcast_settings_items, SETTINGS_SUB_IDX_COUNT);
                g_app_state = APP_STATE_BROADCAST_SETTINGS;
                render_current_state(false);
            } else if (key == 'N') {
                build_broadcast_settings_menu();
                broadcast_settings_update_labels();
                view_list_screen_init(&g_broadcast_settings_menu, i18n_str(STR_BROADCAST_SETTINGS),
                                      g_broadcast_settings_items, SETTINGS_SUB_IDX_COUNT);
                g_app_state = APP_STATE_BROADCAST_SETTINGS;
                render_current_state(false);
            }
            break;

        case APP_STATE_VCARD_SCAN_INTERVAL:
            if (key == '2' || key == '6') {
                view_slider_adjust(&g_slider, true);
                render_current_state(true);
            } else if (key == '4' || key == '8') {
                view_slider_adjust(&g_slider, false);
                render_current_state(true);
            } else if (key == 'Y') {
                uint16_t val = view_slider_get_value(&g_slider);
                ble_badge_set_scan_interval(10000, (uint32_t)val * 1000);
                build_broadcast_settings_menu();
                broadcast_settings_update_labels();
                view_list_screen_init(&g_broadcast_settings_menu, i18n_str(STR_BROADCAST_SETTINGS),
                                      g_broadcast_settings_items, SETTINGS_SUB_IDX_COUNT);
                g_app_state = APP_STATE_BROADCAST_SETTINGS;
                render_current_state(false);
            } else if (key == 'N') {
                build_broadcast_settings_menu();
                broadcast_settings_update_labels();
                view_list_screen_init(&g_broadcast_settings_menu, i18n_str(STR_BROADCAST_SETTINGS),
                                      g_broadcast_settings_items, SETTINGS_SUB_IDX_COUNT);
                g_app_state = APP_STATE_BROADCAST_SETTINGS;
                render_current_state(false);
            }
            break;

        case APP_STATE_VCARD_LIST:
            if (key == '2') {
                view_list_screen_navigate(&g_vcard_list, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_vcard_list, true);
                render_current_state(true);
            } else if (key == '3') {
                uint16_t sel = view_list_screen_get_selection(&g_vcard_list);
                if (sel < g_vcard_list_count) {
                    g_vcard_selected_slot = g_vcard_list_slots[sel];
                    g_vcard_selected_is_own = (g_vcard_selected_slot == 0xFFFF);
                    g_vcard_context_items[0] = {i18n_str(STR_VIEW), 1};
                    g_vcard_context_items[1] = {i18n_str(STR_VCARD_SHOW_QR), 2};
                    g_vcard_context_items[2] = {i18n_str(STR_DELETE), 3};
                    view_context_menu_init(&g_vcard_context_menu, i18n_str(STR_SELECT),
                                           g_vcard_context_items, 3);
                    view_context_menu_show(&g_vcard_context_menu);
                    g_app_state = APP_STATE_VCARD_CONTEXT_MENU;
                    render_current_state(false);
                }
            } else if (key == 'Y') {
                uint16_t sel = view_list_screen_get_selection(&g_vcard_list);
                if (sel < g_vcard_list_count) {
                    g_vcard_selected_slot = g_vcard_list_slots[sel];
                    g_vcard_selected_is_own = (g_vcard_selected_slot == 0xFFFF);
                    if (vcard_load_selected(g_vcard_view_buf, sizeof(g_vcard_view_buf))) {
                        view_info_screen_init(&g_info_view, i18n_str(STR_VIEW), g_vcard_view_buf);
                        g_app_state = APP_STATE_VCARD_VIEW;
                        render_current_state(false);
                    }
                }
            } else if (key == 'N') {
                go_to_vcard_submenu();
            }
            break;

        case APP_STATE_VCARD_CONTEXT_MENU:
            if (key == '2') {
                view_context_menu_navigate(&g_vcard_context_menu, false);
                view_context_menu_render(&g_vcard_context_menu);
            } else if (key == '8') {
                view_context_menu_navigate(&g_vcard_context_menu, true);
                view_context_menu_render(&g_vcard_context_menu);
            } else if (key == 'Y') {
                uint8_t action = view_context_menu_get_action(&g_vcard_context_menu);
                view_context_menu_hide(&g_vcard_context_menu);
                if (action == 1) {
                    if (vcard_load_selected(g_vcard_view_buf, sizeof(g_vcard_view_buf))) {
                        view_info_screen_init(&g_info_view, i18n_str(STR_VIEW), g_vcard_view_buf);
                        g_app_state = APP_STATE_VCARD_VIEW;
                        render_current_state(false);
                    }
                } else if (action == 2) {
                    if (vcard_load_selected(g_vcard_view_buf, sizeof(g_vcard_view_buf))) {
                        char name_buf[64] = {0};
                        if (g_vcard_selected_is_own) {
                            vcard_store_get_display_own(name_buf, sizeof(name_buf));
                        } else {
                            vcard_store_get_display(g_vcard_selected_slot, name_buf, sizeof(name_buf));
                        }
                        vcard_show_qr(g_vcard_view_buf, name_buf, APP_STATE_VCARD_LIST);
                    }
                } else if (action == 3) {
                    bool deleted = false;
                    if (g_vcard_selected_is_own) {
                        deleted = vcard_store_clear_own();
                    } else {
                        deleted = vcard_store_delete(g_vcard_selected_slot);
                    }
                    if (deleted) {
                        view_toast_success(i18n_str(STR_DELETED), 1000);
                    } else {
                        view_toast_error(i18n_str(STR_DELETE_FAILED), 1000);
                    }
                    vcard_refresh_list();
                    view_list_screen_init(&g_vcard_list, i18n_str(STR_VCARD_LIST),
                                          g_vcard_list_items, g_vcard_list_count);
                    g_app_state = APP_STATE_VCARD_LIST;
                    render_current_state(false);
                }
            } else if (key == 'N') {
                view_context_menu_hide(&g_vcard_context_menu);
                g_app_state = APP_STATE_VCARD_LIST;
                render_current_state(false);
            }
            break;

        case APP_STATE_VCARD_VIEW:
            if (key == '2' || key == '8') {
                view_info_screen_scroll(&g_info_view, key == '8');
                render_current_state(true);
            } else if (key == 'N') {
                g_app_state = APP_STATE_VCARD_LIST;
                render_current_state(false);
            }
            break;

        case APP_STATE_VCARD_QR:
            if (key) {
                gui_backlight_off();  // Turn off backlight when leaving QR view
                g_app_state = g_vcard_qr_return_state;
                render_current_state(false);
            }
            break;

        case APP_STATE_VCARD_NEARBY_LIST:
            if (key == '2') {
                view_list_screen_navigate(&g_vcard_nearby_list, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_vcard_nearby_list, true);
                render_current_state(true);
            } else if (key == 'Y') {
                uint16_t sel = view_list_screen_get_selection(&g_vcard_nearby_list);
                if (sel < g_vcard_nearby_count) {
                    if (!ble_badge_exchange_with(g_vcard_nearby_peers[sel].addr)) {
                        view_toast_error(i18n_str(STR_VCARD_SEND_FAILED), 1000);
                        break;
                    }
                    view_info_screen_init(&g_info_view, i18n_str(STR_VCARD_SEND),
                                          i18n_str(STR_VCARD_SENDING));
                    g_app_state = APP_STATE_VCARD_SEND_PROGRESS;
                    render_current_state(false);
                }
            } else if (key == 'N') {
                go_to_vcard_submenu();
            }
            break;

        case APP_STATE_VCARD_SEND_PROGRESS:
            if (key == 'N') {
                ble_badge_exchange_cancel();
                g_app_state = APP_STATE_VCARD_NEARBY_LIST;
                render_current_state(false);
            }
            break;

        case APP_STATE_VCARD_NEARBY_ALERT:
            if (key == 'N' || key == 'Y') {
                g_vcard_nearby_alert_until = 0;
                g_app_state = g_vcard_nearby_return_state;
                render_current_state(false);
            }
            break;

        case APP_STATE_BLE_PAIRING_CONFIRM:
            if (key == 'Y') {
                ble_badge_confirm_pairing(true);
                g_app_state = g_ble_pairing_return_state;
                render_current_state(false);
            } else if (key == 'N') {
                ble_badge_confirm_pairing(false);
                g_app_state = g_ble_pairing_return_state;
                render_current_state(false);
            }
            break;

        case APP_STATE_BLE_PAIRING_PASSKEY:
            if (key >= '0' && key <= '9') {
                view_pin_entry_digit(&g_pin_entry, key);
                render_current_state(true);
            } else if (key == 'N') {
                if (g_pin_entry.len == 0) {
                    ble_badge_reply_passkey(false, 0);
                    g_app_state = g_ble_pairing_return_state;
                    render_current_state(false);
                } else {
                    view_pin_entry_backspace(&g_pin_entry);
                    render_current_state(true);
                }
            } else if (key == 'Y') {
                const char *pin = view_pin_entry_get_pin(&g_pin_entry);
                uint32_t passkey = (uint32_t)strtoul(pin, NULL, 10);
                ble_badge_reply_passkey(true, passkey);
                g_app_state = g_ble_pairing_return_state;
                render_current_state(false);
            }
            break;

        case APP_STATE_BLE_PAIRING_DISPLAY:
            if (key == 'N' || key == 'Y') {
                g_app_state = g_ble_pairing_return_state;
                render_current_state(false);
            }
            break;
#endif

        case APP_STATE_TOOLS_NTP_SYNC:
            if (key == 'N') {
                // Cancel NTP sync - cleanup
                ntp_sync_stop();
                // Release session (will auto-deinit if we acquired it)
                if (g_ntp_wifi_session > 0) {
                    wifi_manager_session_release(g_ntp_wifi_session, true);
                    g_ntp_wifi_session = 0;
                }
                g_ntp_sync_phase = 0;
                build_tools_menu();
                view_list_screen_init(&g_tools_menu, i18n_str(STR_TOOLS), g_tools_items, g_tools_item_count);
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
#if FEATURE_BLE_UART || FEATURE_BLE_BADGE
                        if (bluetooth_is_active()) {
                            view_toast_error(i18n_str(STR_WIFI_DISABLE_BLUETOOTH), 1500);
                            render_current_state(false);
                            break;
                        }
#endif
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
                    // Setup - start WiFi scan
#if FEATURE_BLE_UART || FEATURE_BLE_BADGE
                    if (bluetooth_is_active()) {
                        view_toast_error(i18n_str(STR_WIFI_DISABLE_BLUETOOTH), 1500);
                        render_current_state(false);
                        break;
                    }
#endif
                    wifi_manager_init();
                    wifi_manager_start_scan();
                    g_wifi_scan_start = millis();
                    view_info_screen_init(&g_info_view, i18n_str(STR_WIFI_MENU), i18n_str(STR_WIFI_SCANNING));
                    g_app_state = APP_STATE_WIFI_SCAN;
                    render_current_state(false);
                } else if (sel == 2) {
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
                } else if (sel == 3) {
                    // Disconnect
                    wifi_manager_disconnect();
                    wifi_manager_deinit();
                    view_toast_success(i18n_str(STR_WIFI_DISCONNECTED), 1500);
                    render_current_state(false);
                }
            } else if (key == 'N') {
                build_tools_menu();
                view_list_screen_init(&g_tools_menu, i18n_str(STR_TOOLS), g_tools_items, g_tools_item_count);
                g_app_state = APP_STATE_TOOLS_MENU;
                render_current_state(false);
            }
            break;

#if FEATURE_CA
        case APP_STATE_CA_MENU:
            if (key == '2') {
                view_list_screen_navigate(&g_ca_menu, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_ca_menu, true);
                render_current_state(true);
            } else if (key == 'Y' || key == '5') {
                uint8_t sel = view_list_screen_get_selection(&g_ca_menu);
                switch (sel) {
                    case CA_IDX_STATUS: {
                        // Show CA status details
                        ca_status_t status;
                        if (ca_get_status(&status) && status.initialized) {
                            snprintf(g_ca_detail_text, sizeof(g_ca_detail_text),
                                "%s: %s\n\n"
                                "%s: %s\n\n"
                                "%s: %lu\n\n"
                                "%s: %lu",
                                i18n_str(STR_CA_COMMON_NAME), status.common_name,
                                i18n_str(STR_CA_INITIALIZED), i18n_str(STR_OK),
                                i18n_str(STR_CA_ISSUED_CERTS), (unsigned long)status.issued_count,
                                i18n_str(STR_CA_SERIAL), (unsigned long)status.serial_counter);
                        } else {
                            snprintf(g_ca_detail_text, sizeof(g_ca_detail_text),
                                "%s", i18n_str(STR_CA_NOT_INIT));
                        }
                        view_info_screen_init(&g_info_view, i18n_str(STR_CA_STATUS), g_ca_detail_text);
                        g_app_state = APP_STATE_CA_DETAILS;
                        render_current_state(false);
                        break;
                    }
                    case CA_IDX_GENERATE:
                        if (ca_is_initialized()) {
                            // CA exists - show reset confirmation
                            view_info_screen_init(&g_info_view, i18n_str(STR_CA_RESET),
                                i18n_str(STR_CA_RESET_CONFIRM));
                            g_app_state = APP_STATE_CA_RESET_CONFIRM;
                            render_current_state(false);
                        } else {
                            // No CA - start wizard with Common Name
                            memset(&g_ca_wizard, 0, sizeof(g_ca_wizard));
                            g_ca_wizard.validity_years = 10;  // Default 10 years
                            view_t9_input_init(&g_t9_input, i18n_str(STR_CA_ENTER_CN), "");
                            g_app_state = APP_STATE_CA_WIZARD_CN;
                            render_current_state(false);
                        }
                        break;
                    case CA_IDX_EXPORT:
                        if (!ca_is_initialized()) {
                            view_toast_error(i18n_str(STR_CA_NOT_INIT), 1500);
                        } else {
                            size_t out_len;
                            if (ca_export_root_cert_pem(g_ca_detail_text, sizeof(g_ca_detail_text), &out_len)) {
                                view_info_screen_init(&g_info_view, i18n_str(STR_CA_EXPORT), g_ca_detail_text);
                                g_app_state = APP_STATE_CA_DETAILS;
                                render_current_state(false);
                            } else {
                                view_toast_error("Export failed", 1500);
                            }
                        }
                        break;
                    case CA_IDX_QR_CODE:
                        if (!ca_is_initialized()) {
                            view_toast_error(i18n_str(STR_CA_NOT_INIT), 1500);
                        } else {
                            size_t out_len = 0;
                            if (ca_export_pubkey_base64(g_ca_detail_text, sizeof(g_ca_detail_text), &out_len)) {
                                view_qr_code_init(&g_qr_view, i18n_str(STR_CA_QR_PUBKEY), NULL, g_ca_detail_text);
                                g_app_state = APP_STATE_CA_QR_CODE;
                                render_current_state(false);
                            } else {
                                view_toast_error("Export failed", 1500);
                            }
                        }
                        break;
                }
            } else if (key == 'N') {
                go_to_main_menu();
            }
            break;

        case APP_STATE_CA_DETAILS:
            if (key == '2') {
                view_info_screen_scroll(&g_info_view, false);  // Scroll up
                render_current_state(true);
            } else if (key == '8') {
                view_info_screen_scroll(&g_info_view, true);   // Scroll down
                render_current_state(true);
            } else if (key == 'N') {
                g_app_state = APP_STATE_CA_MENU;
                render_current_state(false);
            }
            break;

        case APP_STATE_CA_GENERATING:
            // Wait for generation to complete, no key handling
            break;

        case APP_STATE_CA_QR_CODE:
            if (key) {
                gui_backlight_off();  // Turn off backlight when leaving QR view
                g_app_state = APP_STATE_CA_MENU;
                render_current_state(false);
            }
            break;

        case APP_STATE_CA_RESET_CONFIRM:
            if (key == 'Y') {
                // User confirmed reset - delete CA
                if (ca_factory_reset()) {
                    view_toast_success(i18n_str(STR_CA_RESET_SUCCESS), 1500);
                } else {
                    view_toast_error("Reset failed", 1500);
                }
                build_ca_menu();
                view_list_screen_init(&g_ca_menu, i18n_str(STR_CA_MENU), g_ca_items, CA_IDX_COUNT);
                g_app_state = APP_STATE_CA_MENU;
                render_current_state(false);
            } else if (key == 'N') {
                // User cancelled
                g_app_state = APP_STATE_CA_MENU;
                render_current_state(false);
            }
            break;

        // CA Wizard: Common Name (required)
        case APP_STATE_CA_WIZARD_CN:
            if (key >= '0' && key <= '9') {
                view_t9_input_key(&g_t9_input, key);
                render_current_state(true);
            } else if (key == 'N') {
                if (g_t9_input.len > 0) {
                    view_t9_input_backspace(&g_t9_input);
                    render_current_state(true);
                } else {
                    // Cancel wizard - go back to menu
                    g_app_state = APP_STATE_CA_MENU;
                    render_current_state(false);
                }
            } else if (key == 'Y') {
                const char *text = view_t9_input_get_text(&g_t9_input);
                if (strlen(text) == 0) {
                    view_toast_error("CN required", 1500);
                } else {
                    strncpy(g_ca_wizard.cn, text, CA_FIELD_MAX_LEN - 1);
                    // Next: Organization
                    view_t9_input_init(&g_t9_input, i18n_str(STR_CA_ENTER_ORG), g_ca_wizard.org);
                    g_app_state = APP_STATE_CA_WIZARD_ORG;
                    render_current_state(false);
                }
            }
            break;

        // CA Wizard: Organization (optional)
        case APP_STATE_CA_WIZARD_ORG:
            if (key >= '0' && key <= '9') {
                view_t9_input_key(&g_t9_input, key);
                render_current_state(true);
            } else if (key == 'N') {
                if (g_t9_input.len > 0) {
                    view_t9_input_backspace(&g_t9_input);
                    render_current_state(true);
                } else {
                    // Back to CN
                    view_t9_input_init(&g_t9_input, i18n_str(STR_CA_ENTER_CN), g_ca_wizard.cn);
                    g_app_state = APP_STATE_CA_WIZARD_CN;
                    render_current_state(false);
                }
            } else if (key == 'Y') {
                strncpy(g_ca_wizard.org, view_t9_input_get_text(&g_t9_input), CA_FIELD_MAX_LEN - 1);
                // Next: Org Unit
                view_t9_input_init(&g_t9_input, i18n_str(STR_CA_ENTER_OU), g_ca_wizard.ou);
                g_app_state = APP_STATE_CA_WIZARD_OU;
                render_current_state(false);
            }
            break;

        // CA Wizard: Organizational Unit (optional)
        case APP_STATE_CA_WIZARD_OU:
            if (key >= '0' && key <= '9') {
                view_t9_input_key(&g_t9_input, key);
                render_current_state(true);
            } else if (key == 'N') {
                if (g_t9_input.len > 0) {
                    view_t9_input_backspace(&g_t9_input);
                    render_current_state(true);
                } else {
                    // Back to Org
                    view_t9_input_init(&g_t9_input, i18n_str(STR_CA_ENTER_ORG), g_ca_wizard.org);
                    g_app_state = APP_STATE_CA_WIZARD_ORG;
                    render_current_state(false);
                }
            } else if (key == 'Y') {
                strncpy(g_ca_wizard.ou, view_t9_input_get_text(&g_t9_input), CA_FIELD_MAX_LEN - 1);
                // Next: Country
                view_t9_input_init(&g_t9_input, i18n_str(STR_CA_ENTER_COUNTRY), g_ca_wizard.country);
                g_app_state = APP_STATE_CA_WIZARD_COUNTRY;
                render_current_state(false);
            }
            break;

        // CA Wizard: Country (optional, 2 letters)
        case APP_STATE_CA_WIZARD_COUNTRY:
            if (key >= '0' && key <= '9') {
                view_t9_input_key(&g_t9_input, key);
                render_current_state(true);
            } else if (key == 'N') {
                if (g_t9_input.len > 0) {
                    view_t9_input_backspace(&g_t9_input);
                    render_current_state(true);
                } else {
                    // Back to OU
                    view_t9_input_init(&g_t9_input, i18n_str(STR_CA_ENTER_OU), g_ca_wizard.ou);
                    g_app_state = APP_STATE_CA_WIZARD_OU;
                    render_current_state(false);
                }
            } else if (key == 'Y') {
                strncpy(g_ca_wizard.country, view_t9_input_get_text(&g_t9_input), 2);
                // Next: Locality
                view_t9_input_init(&g_t9_input, i18n_str(STR_CA_ENTER_LOCALITY), g_ca_wizard.locality);
                g_app_state = APP_STATE_CA_WIZARD_LOCALITY;
                render_current_state(false);
            }
            break;

        // CA Wizard: Locality/City (optional)
        case APP_STATE_CA_WIZARD_LOCALITY:
            if (key >= '0' && key <= '9') {
                view_t9_input_key(&g_t9_input, key);
                render_current_state(true);
            } else if (key == 'N') {
                if (g_t9_input.len > 0) {
                    view_t9_input_backspace(&g_t9_input);
                    render_current_state(true);
                } else {
                    // Back to Country
                    view_t9_input_init(&g_t9_input, i18n_str(STR_CA_ENTER_COUNTRY), g_ca_wizard.country);
                    g_app_state = APP_STATE_CA_WIZARD_COUNTRY;
                    render_current_state(false);
                }
            } else if (key == 'Y') {
                strncpy(g_ca_wizard.locality, view_t9_input_get_text(&g_t9_input), CA_FIELD_MAX_LEN - 1);
                // Next: State/Province
                view_t9_input_init(&g_t9_input, i18n_str(STR_CA_ENTER_STATE), g_ca_wizard.state);
                g_app_state = APP_STATE_CA_WIZARD_STATE;
                render_current_state(false);
            }
            break;

        // CA Wizard: State/Province (optional)
        case APP_STATE_CA_WIZARD_STATE:
            if (key >= '0' && key <= '9') {
                view_t9_input_key(&g_t9_input, key);
                render_current_state(true);
            } else if (key == 'N') {
                if (g_t9_input.len > 0) {
                    view_t9_input_backspace(&g_t9_input);
                    render_current_state(true);
                } else {
                    // Back to Locality
                    view_t9_input_init(&g_t9_input, i18n_str(STR_CA_ENTER_LOCALITY), g_ca_wizard.locality);
                    g_app_state = APP_STATE_CA_WIZARD_LOCALITY;
                    render_current_state(false);
                }
            } else if (key == 'Y') {
                strncpy(g_ca_wizard.state, view_t9_input_get_text(&g_t9_input), CA_FIELD_MAX_LEN - 1);
                // Next: Validity selection
                view_list_screen_init(&g_ca_validity_menu, i18n_str(STR_CA_VALIDITY_YEARS),
                    g_ca_validity_items, 6);
                // Pre-select 10 years (index 2)
                g_ca_validity_menu.selection = 2;
                g_app_state = APP_STATE_CA_WIZARD_VALIDITY;
                render_current_state(false);
            }
            break;

        // CA Wizard: Validity Years selection
        case APP_STATE_CA_WIZARD_VALIDITY:
            if (key == '2') {
                view_list_screen_navigate(&g_ca_validity_menu, false);
                render_current_state(true);
            } else if (key == '8') {
                view_list_screen_navigate(&g_ca_validity_menu, true);
                render_current_state(true);
            } else if (key == 'Y' || key == '5') {
                // Get selected validity
                uint8_t sel = view_list_screen_get_selection(&g_ca_validity_menu);
                static const uint8_t validity_values[] = {1, 5, 10, 15, 20, 25};
                g_ca_wizard.validity_years = validity_values[sel];

                // Generate CA with wizard data
                view_info_screen_init(&g_info_view, i18n_str(STR_CA_GENERATE),
                    i18n_str(STR_CA_GENERATING));
                g_app_state = APP_STATE_CA_GENERATING;
                render_current_state(false);

                // Build DN string from wizard data
                char dn[256];
                int pos = 0;
                pos += snprintf(dn + pos, sizeof(dn) - pos, "CN=%s", g_ca_wizard.cn);
                if (g_ca_wizard.org[0]) {
                    pos += snprintf(dn + pos, sizeof(dn) - pos, ",O=%s", g_ca_wizard.org);
                }
                if (g_ca_wizard.ou[0]) {
                    pos += snprintf(dn + pos, sizeof(dn) - pos, ",OU=%s", g_ca_wizard.ou);
                }
                if (g_ca_wizard.country[0]) {
                    pos += snprintf(dn + pos, sizeof(dn) - pos, ",C=%s", g_ca_wizard.country);
                }
                if (g_ca_wizard.locality[0]) {
                    pos += snprintf(dn + pos, sizeof(dn) - pos, ",L=%s", g_ca_wizard.locality);
                }
                if (g_ca_wizard.state[0]) {
                    pos += snprintf(dn + pos, sizeof(dn) - pos, ",ST=%s", g_ca_wizard.state);
                }

                // Generate CA
                if (ca_setup(dn)) {
                    view_toast_success(i18n_str(STR_CA_GENERATED), 1500);
                } else {
                    view_toast_error(i18n_str(STR_CA_GENERATE_FAILED), 1500);
                }
                build_ca_menu();
                view_list_screen_init(&g_ca_menu, i18n_str(STR_CA_MENU), g_ca_items, CA_IDX_COUNT);
                g_app_state = APP_STATE_CA_MENU;
                render_current_state(false);
            } else if (key == 'N') {
                // Back to State
                view_t9_input_init(&g_t9_input, i18n_str(STR_CA_ENTER_STATE), g_ca_wizard.state);
                g_app_state = APP_STATE_CA_WIZARD_STATE;
                render_current_state(false);
            }
            break;
#endif

    }
}
