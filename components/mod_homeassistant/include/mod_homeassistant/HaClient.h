#pragma once

#include "mod_homeassistant/HaFavorite.h"
#include <cstdint>
#include <vector>

namespace cdc::mod_homeassistant {

/**
 * \brief HTTP result codes returned by HaClient operations.
 */
enum class HaResult : uint8_t {
    OK              = 0,
    NO_CONFIG       = 1,    // URL or token missing
    NO_WIFI         = 2,    // WiFi not connected
    TCP_FAIL        = 3,    // Cannot reach host
    UNAUTHORIZED    = 4,    // HTTP 401 (token invalid)
    SERVER_ERROR    = 5,    // HTTP 5xx
    TIMEOUT         = 6,
    PARSE_ERROR     = 7,    // Malformed JSON
    OUT_OF_MEMORY   = 8,
};

/**
 * \brief REST client for a Home Assistant instance.
 *
 * Uses esp_http_client + mbedtls + cJSON. All calls are synchronous and must
 * be invoked from a dedicated FreeRTOS task (`HaNetTask`), never from the UI
 * thread.
 *
 * Configuration (URL, token, SSL behaviour) is loaded from persistent storage
 * via `loadConfig()` before any request can succeed.
 */
class HaClient {
public:
    static constexpr uint32_t GET_TIMEOUT_MS    = 5000;
    static constexpr uint32_t POST_TIMEOUT_MS   = 3000;
    static constexpr size_t   MAX_URL_LEN       = 256;
    static constexpr size_t   MAX_TOKEN_LEN     = 256;

    /**
     * \brief Loads URL and token from NVS / TROPIC01 R-Memory.
     * \return `true` if both URL and token are present and look valid.
     */
    bool loadConfig();

    /**
     * \brief Returns `true` if URL is set in NVS.
     */
    static bool isUrlConfigured();

    /**
     * \brief Returns `true` if a token is present in the R-Memory slot.
     */
    static bool isTokenConfigured();

    /**
     * \brief Wipes URL (NVS) and token (R-Memory).
     */
    static void resetAll();

    /**
     * \brief GET /api/ to verify reachability and token validity.
     */
    HaResult ping();

    /**
     * \brief GET /api/states, filtered to supported domains, into `out`.
     *
     * Appends to `out` (does not clear). Entities that don't match a supported
     * `HaDomain` are skipped.
     */
    HaResult getStates(std::vector<HaEntityState>& out);

    /**
     * \brief POST /api/services/<domain>/<service>.
     * \param domain Domain string (e.g. "light", "scene").
     * \param service Service name (e.g. "toggle", "turn_on").
     * \param entityId Target entity id.
     * \param extraParamsJson Optional JSON fragment merged into the payload
     *        body. Must be a valid JSON object body without surrounding
     *        braces, e.g. `"brightness_pct": 50`. May be nullptr.
     */
    HaResult callService(const char* domain,
                         const char* service,
                         const char* entityId,
                         const char* extraParamsJson = nullptr);

    /**
     * \brief Returns the last HTTP status code (0 if no request was made).
     */
    int getLastHttpStatus() const { return lastHttpStatus_; }

private:
    char url_[MAX_URL_LEN]      = {};
    char token_[MAX_TOKEN_LEN]  = {};
    int  lastHttpStatus_        = 0;

    HaResult performRequest(const char* method,
                            const char* path,
                            const char* body,
                            char** responseOut,
                            size_t* responseLenOut,
                            uint32_t timeoutMs);
};

} // namespace cdc::mod_homeassistant
