#pragma once

#include "mod_homeassistant/HaDomain.h"
#include <cstdint>

namespace cdc::mod_homeassistant {

/**
 * \brief Persistent favorite entry (NVS blob, packed for storage stability).
 *
 * Stored as opaque blob in the module's NVS namespace under key
 * `fav_<index>`. The layout must remain compatible across builds. See
 * `HaFavoriteStorage` for read/write helpers.
 */
struct HaFavorite {
    char    entity_id[64];      // Full entity id, e.g. "light.kitchen"
    char    display_name[32];   // User-editable label, defaults to friendly_name
    uint8_t domain;             // HaDomain value
    uint8_t flags;              // Reserved for future use
};

static_assert(sizeof(HaFavorite) == 64 + 32 + 1 + 1,
              "HaFavorite size must stay stable for NVS compatibility");

/**
 * \brief RAM-only cached entity state, populated from `GET /api/states`.
 *
 * Lives in PSRAM as part of a `std::vector<HaEntityState>` owned by the
 * `HaHomeView` while the module is active. Not persisted.
 */
struct HaEntityState {
    char    entity_id[64];
    char    friendly_name[48];
    uint8_t domain;             // HaDomain value
    uint8_t state;              // 0=off, 1=on, 2=unknown, 3=unavailable
    uint8_t brightness;         // 0-100, only valid for HaDomain::LIGHT
};

/**
 * \brief Encoded state values used in HaEntityState::state.
 */
enum class HaState : uint8_t {
    OFF          = 0,
    ON           = 1,
    UNKNOWN      = 2,
    UNAVAILABLE  = 3,
};

constexpr uint8_t HA_MAX_FAVORITES = 32;

} // namespace cdc::mod_homeassistant
