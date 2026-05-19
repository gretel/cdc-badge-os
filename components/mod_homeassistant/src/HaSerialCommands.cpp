#include "mod_homeassistant/HaClient.h"
#include "mod_homeassistant/HaStorage.h"
#include "serial_cmd/ICommandRegistry.h"
#include "serial_cmd/Console.h"
#include "cdc_core/StringUtils.h"
#include "cdc_log.h"
#include <cstring>
#include <cctype>
#include <cstdio>

static const char* TAG = "HA_CMD";

namespace cdc::mod_homeassistant {

static constexpr const char* CMD_MODULE = "homeassistant";
static bool s_registered = false;

using cdc::core::skipSpaces;
using cdc::core::nextToken;

/**
 * \brief Persists the Home Assistant base URL.
 * \param args URL string (e.g. `http://homeassistant.local:8123`).
 */
static void cmdHaUrl(const char* args) {
    if (!args || !*args) {
        cdc::serial::Console::printf("Usage: HA_URL <http(s)://host:port>\r\n");
        return;
    }
    const char* p = args;
    while (*p && isspace(static_cast<unsigned char>(*p))) p++;

    if (strncmp(p, "http://", 7) != 0 && strncmp(p, "https://", 8) != 0) {
        cdc::serial::Console::printf("ERROR: URL must start with http:// or https://\r\n");
        return;
    }
    bool ok = HaFavoriteStorage::writeUrl(p);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}

/**
 * \brief Stores the Home Assistant Long-Lived Access Token in the
 *        TROPIC01 R-Memory slot reserved for the module.
 */
static void cmdHaToken(const char* args) {
    if (!args || !*args) {
        cdc::serial::Console::printf("Usage: HA_TOKEN <token>\r\n");
        return;
    }
    const char* p = args;
    while (*p && isspace(static_cast<unsigned char>(*p))) p++;

    size_t len = strlen(p);
    if (len < 32 || len > HaTokenStorage::MAX_LEN) {
        cdc::serial::Console::printf("ERROR: token length %u out of range (32..%u)\r\n",
                                     static_cast<unsigned>(len),
                                     static_cast<unsigned>(HaTokenStorage::MAX_LEN));
        return;
    }
    bool ok = HaTokenStorage::write(p);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}

/**
 * \brief Prints current URL, token fingerprint and basic status.
 */
static void cmdHaStatus(const char* args) {
    (void)args;
    char url[256] = {};
    HaFavoriteStorage::readUrl(url, sizeof(url));
    char fp[17] = {};
    HaTokenStorage::fingerprint(fp, sizeof(fp));
    cdc::serial::Console::printf(
        "URL:        %s\r\n"
        "Token:      %s%s\r\n",
        url[0] ? url : "(not set)",
        fp[0] ? "sha256:" : "",
        fp[0] ? fp : "(not set)");
}

/**
 * \brief Wipes URL, token and favorites.
 */
static void cmdHaReset(const char* args) {
    (void)args;
    HaClient::resetAll();
    cdc::serial::Console::printf("OK\r\n");
}

void registerHaSerialCommands() {
    if (s_registered) return;
    auto& reg = cdc::serial::getCommandRegistry();
    reg.registerCommand({"HA_URL",    "Set Home Assistant base URL",            cmdHaUrl,    CMD_MODULE, true});
    reg.registerCommand({"HA_TOKEN",  "Set Home Assistant Long-Lived Token",    cmdHaToken,  CMD_MODULE, true});
    reg.registerCommand({"HA_STATUS", "Show Home Assistant configuration",      cmdHaStatus, CMD_MODULE, true});
    reg.registerCommand({"HA_RESET",  "Wipe HA URL, token and favorites",       cmdHaReset,  CMD_MODULE, true});
    s_registered = true;
    LOG_I(TAG, "Serial commands registered");
}

} // namespace cdc::mod_homeassistant
