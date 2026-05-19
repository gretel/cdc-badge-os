#pragma once

#include "mod_homeassistant/HaFavorite.h"
#include "cdc_core/IModule.h"
#include <cstddef>
#include <vector>

namespace cdc::mod_homeassistant {

/**
 * \brief NVS-backed storage for favorites and URL settings.
 *
 * Module NVS namespace is `mod_homeassistant` (via `ModuleRegistry::NVS_PREFIX`).
 * Keys:
 *  - `url` (string, ≤ 256 B)
 *  - `ssl_skip` (u8)
 *  - `fav_count` (u8)
 *  - `fav_<i>` (blob `HaFavorite`)
 */
namespace HaFavoriteStorage {

static constexpr const char* NVS_NAMESPACE = "mod_ha";
static constexpr const char* KEY_URL       = "url";
static constexpr const char* KEY_FAV_COUNT = "fav_count";

bool loadAll(std::vector<HaFavorite>& out);
bool saveAll(const std::vector<HaFavorite>& favorites);

bool readUrl(char* out, size_t maxLen);
bool writeUrl(const char* url);

/**
 * \brief Wipes the NVS namespace (URL, favorites).
 */
void wipe();

} // namespace HaFavoriteStorage

/**
 * \brief TROPIC01 R-Memory backed storage for the access token.
 *
 * The token occupies one R-Memory slot mapped via the module's `SlotRange`
 * (`SlotRequest { .mapName = "HA_TOKEN", .minRmemSlots = 1 }`). Layout in the
 * slot: a `\0`-terminated UTF-8 string (max 254 bytes payload, capped at
 * HaTokenStorage::MAX_LEN). Empty slot = no token configured.
 */
namespace HaTokenStorage {

static constexpr size_t MAX_LEN = 250;

bool isPresent();
bool read(char* out, size_t maxLen);
bool write(const char* token);
bool clear();

/**
 * \brief Returns a short (8 hex-byte) SHA256 fingerprint of the token.
 *        Useful for status output without leaking the token itself.
 */
bool fingerprint(char* outHex16, size_t outSize);

} // namespace HaTokenStorage

} // namespace cdc::mod_homeassistant
