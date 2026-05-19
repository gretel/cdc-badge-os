#pragma once

#include <cstdint>

namespace cdc::mod_homeassistant {

/**
 * \brief Module-local string identifiers.
 *
 * Numbers are offsets into the registered i18n block; mapped to the global
 * `StringId` space by `mstr()` at runtime.
 */
enum HaStr : uint16_t {
    STR_TITLE              = 0,
    STR_SETUP_TITLE        = 1,
    STR_URL_OK             = 2,
    STR_URL_MISSING        = 3,
    STR_TOKEN_OK           = 4,
    STR_TOKEN_MISSING      = 5,
    STR_SERIAL_CMDS        = 6,
    STR_ENTER_URL_GUI      = 7,
    STR_ACTIONS            = 8,
    STR_HINT_MENU_BACK     = 9,
    STR_HINT_ADD_FILTER    = 10,
    STR_HINT_TOGGLE_MENU   = 11,
    STR_HOST               = 12,
    STR_PORT               = 13,
    STR_USE_HTTPS          = 14,
    STR_SKIP_CERT_CHECK    = 15,
    STR_YES                = 16,
    STR_NO                 = 17,
    STR_URL_SAVED          = 18,
    STR_SAVE_FAILED        = 19,
    STR_HOST_REQUIRED      = 20,
    STR_SEARCH             = 21,
    STR_ALL_DOMAINS        = 22,
    STR_LIGHTS             = 23,
    STR_SWITCHES           = 24,
    STR_SCENES             = 25,
    STR_FILTER             = 26,
    STR_BROWSE             = 27,
    STR_BROWSE_ALL         = 28,
    STR_BRIGHTNESS         = 29,
    STR_REMOVE             = 30,
    STR_RESET_MODULE       = 31,
    STR_RESET_CONFIRM      = 32,
    STR_RESET_DONE         = 33,
    STR_MAX_FAVS           = 34,
    STR_ADDED              = 35,
    STR_REMOVED            = 36,
    STR_TOAST_OK           = 37,
    STR_HA_ERROR           = 38,
    STR_HA_UNREACHABLE     = 39,
    STR_HA_CONFIG_MISSING  = 40,
    STR_NO_WIFI            = 41,
    STR_UNSUPPORTED        = 42,
    STR_COUNT
};

/**
 * \brief Registers all module translations. Called once during module init.
 */
void registerStrings();

/**
 * \brief Returns the translated string for the module-local id.
 *        Falls back to a debug placeholder when called before
 *        `registerStrings()` has run.
 */
const char* mstr(uint16_t id);

} // namespace cdc::mod_homeassistant
