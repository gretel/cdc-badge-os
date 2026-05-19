#include "mod_homeassistant/HaStorage.h"
#include "cdc_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <cstring>
#include <cstdio>

static const char* TAG = "HA_STORE";

namespace cdc::mod_homeassistant {
namespace HaFavoriteStorage {

/**
 * \brief Loads all favorites from NVS into `out`.
 * \return `true` on success (including empty store).
 */
bool loadAll(std::vector<HaFavorite>& out) {
    out.clear();
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return true;  // namespace not created yet, treat as empty
    }

    uint8_t count = 0;
    if (nvs_get_u8(nvs, KEY_FAV_COUNT, &count) != ESP_OK || count == 0) {
        nvs_close(nvs);
        return true;
    }
    if (count > HA_MAX_FAVORITES) {
        count = HA_MAX_FAVORITES;
    }

    out.reserve(count);
    char key[16];
    for (uint8_t i = 0; i < count; i++) {
        snprintf(key, sizeof(key), "fav_%u", static_cast<unsigned>(i));
        HaFavorite fav = {};
        size_t len = sizeof(fav);
        esp_err_t err = nvs_get_blob(nvs, key, &fav, &len);
        if (err != ESP_OK || len != sizeof(fav)) {
            LOG_W(TAG, "Skip favorite %u (err=%d, len=%u)",
                  static_cast<unsigned>(i), static_cast<int>(err),
                  static_cast<unsigned>(len));
            continue;
        }
        out.push_back(fav);
    }

    nvs_close(nvs);
    return true;
}

/**
 * \brief Saves the given favorites (overwrites existing).
 */
bool saveAll(const std::vector<HaFavorite>& favorites) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        LOG_E(TAG, "Failed to open NVS for write");
        return false;
    }

    // Determine previous count to erase any leftover slots
    uint8_t prevCount = 0;
    nvs_get_u8(nvs, KEY_FAV_COUNT, &prevCount);

    const uint8_t count = favorites.size() > HA_MAX_FAVORITES
                              ? HA_MAX_FAVORITES
                              : static_cast<uint8_t>(favorites.size());

    char key[16];
    for (uint8_t i = 0; i < count; i++) {
        snprintf(key, sizeof(key), "fav_%u", static_cast<unsigned>(i));
        if (nvs_set_blob(nvs, key, &favorites[i], sizeof(HaFavorite)) != ESP_OK) {
            LOG_E(TAG, "Failed to write favorite %u", static_cast<unsigned>(i));
            nvs_close(nvs);
            return false;
        }
    }
    // Remove leftover entries
    for (uint8_t i = count; i < prevCount; i++) {
        snprintf(key, sizeof(key), "fav_%u", static_cast<unsigned>(i));
        nvs_erase_key(nvs, key);
    }

    nvs_set_u8(nvs, KEY_FAV_COUNT, count);
    esp_err_t err = nvs_commit(nvs);
    nvs_close(nvs);
    return err == ESP_OK;
}

bool readUrl(char* out, size_t maxLen) {
    if (!out || maxLen == 0) return false;
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        out[0] = '\0';
        return false;
    }
    size_t len = maxLen;
    esp_err_t err = nvs_get_str(nvs, KEY_URL, out, &len);
    nvs_close(nvs);
    if (err != ESP_OK || len <= 1) {
        out[0] = '\0';
        return false;
    }
    return true;
}

bool writeUrl(const char* url) {
    if (!url) return false;
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) return false;
    esp_err_t err = nvs_set_str(nvs, KEY_URL, url);
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err == ESP_OK;
}

void wipe() {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) return;
    nvs_erase_all(nvs);
    nvs_commit(nvs);
    nvs_close(nvs);
}

} // namespace HaFavoriteStorage
} // namespace cdc::mod_homeassistant
