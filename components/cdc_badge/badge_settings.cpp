// Badge Settings Module for CDC Badge
// Stores lock screen text in NVS

#include "badge_settings.h"
#include "cdc_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <cstring>

#define NVS_NAMESPACE "badge"
#define NVS_KEY_NAME "name"
#define NVS_KEY_INFO "info"
#define NVS_KEY_INFO2 "info2"

// Internal buffers
static char g_name[BADGE_TEXT_MAX_LEN + 1] = "CDC Badge";
static char g_info[BADGE_TEXT_MAX_LEN + 1] = "";
static char g_info2[BADGE_TEXT_MAX_LEN + 1] = "";
static bool g_loaded = false;

void badge_settings_load(void) {
    nvs_handle_t nvs;

    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) == ESP_OK) {
        size_t name_len = sizeof(g_name);
        if (nvs_get_str(nvs, NVS_KEY_NAME, g_name, &name_len) == ESP_OK) {
            LOG_I("BADGE", "Loaded name: %s", g_name);
        }

        size_t info_len = sizeof(g_info);
        if (nvs_get_str(nvs, NVS_KEY_INFO, g_info, &info_len) == ESP_OK) {
            LOG_I("BADGE", "Loaded info: %s", g_info);
        }

        size_t info2_len = sizeof(g_info2);
        if (nvs_get_str(nvs, NVS_KEY_INFO2, g_info2, &info2_len) == ESP_OK) {
            LOG_I("BADGE", "Loaded info2: %s", g_info2);
        }

        nvs_close(nvs);
    }

    g_loaded = true;
}

void badge_settings_save(void) {
    nvs_handle_t nvs;

    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_str(nvs, NVS_KEY_NAME, g_name);
        nvs_set_str(nvs, NVS_KEY_INFO, g_info);
        nvs_set_str(nvs, NVS_KEY_INFO2, g_info2);
        nvs_commit(nvs);
        nvs_close(nvs);
        LOG_I("BADGE", "Saved settings");
    }
}

const char* badge_settings_get_name(void) {
    if (!g_loaded) badge_settings_load();
    return g_name;
}

void badge_settings_set_name(const char *name) {
    if (!name) return;
    strncpy(g_name, name, BADGE_TEXT_MAX_LEN);
    g_name[BADGE_TEXT_MAX_LEN] = '\0';
}

const char* badge_settings_get_info(void) {
    if (!g_loaded) badge_settings_load();
    return g_info;
}

void badge_settings_set_info(const char *info) {
    if (!info) return;
    strncpy(g_info, info, BADGE_TEXT_MAX_LEN);
    g_info[BADGE_TEXT_MAX_LEN] = '\0';
}

const char* badge_settings_get_info2(void) {
    if (!g_loaded) badge_settings_load();
    return g_info2;
}

void badge_settings_set_info2(const char *info2) {
    if (!info2) return;
    strncpy(g_info2, info2, BADGE_TEXT_MAX_LEN);
    g_info2[BADGE_TEXT_MAX_LEN] = '\0';
}
