#include "mod_homeassistant/HaClient.h"
#include "mod_homeassistant/HaStorage.h"
#include "cdc_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

static const char* TAG = "HA_CLIENT";

namespace cdc::mod_homeassistant {

/**
 * \brief Per-request response buffer captured by the event handler.
 */
struct ResponseBuffer {
    char*  data;
    size_t len;
    size_t cap;
};

/**
 * \brief Appends a chunk to the response buffer, growing it geometrically.
 */
static esp_err_t httpEventHandler(esp_http_client_event_t* evt) {
    if (!evt) return ESP_OK;
    auto* buf = static_cast<ResponseBuffer*>(evt->user_data);

    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA: {
            if (!buf || evt->data_len <= 0) break;
            size_t needed = buf->len + evt->data_len + 1;
            if (needed > buf->cap) {
                size_t newCap = buf->cap == 0 ? 4096 : buf->cap;
                while (newCap < needed) newCap *= 2;
                char* grown = static_cast<char*>(realloc(buf->data, newCap));
                if (!grown) {
                    LOG_E(TAG, "OOM growing response buffer to %u", static_cast<unsigned>(newCap));
                    return ESP_FAIL;
                }
                buf->data = grown;
                buf->cap  = newCap;
            }
            memcpy(buf->data + buf->len, evt->data, evt->data_len);
            buf->len += evt->data_len;
            buf->data[buf->len] = '\0';
            break;
        }
        default:
            break;
    }
    return ESP_OK;
}

bool HaClient::isUrlConfigured() {
    char url[MAX_URL_LEN] = {};
    return HaFavoriteStorage::readUrl(url, sizeof(url)) && url[0] != '\0';
}

bool HaClient::isTokenConfigured() {
    return HaTokenStorage::isPresent();
}

void HaClient::resetAll() {
    HaTokenStorage::clear();
    HaFavoriteStorage::wipe();
}

bool HaClient::loadConfig() {
    if (!HaFavoriteStorage::readUrl(url_, sizeof(url_)) || url_[0] == '\0') {
        return false;
    }
    if (!HaTokenStorage::read(token_, sizeof(token_))) {
        return false;
    }
    return true;
}

HaResult HaClient::performRequest(const char* method,
                                  const char* path,
                                  const char* body,
                                  char** responseOut,
                                  size_t* responseLenOut,
                                  uint32_t timeoutMs) {
    lastHttpStatus_ = 0;
    if (url_[0] == '\0' || token_[0] == '\0') {
        return HaResult::NO_CONFIG;
    }

    char fullUrl[MAX_URL_LEN + 64] = {};
    // Avoid double slashes if the stored URL ends with '/' and path starts with '/'.
    size_t urlLen = strlen(url_);
    bool urlHasSlash  = urlLen > 0 && url_[urlLen - 1] == '/';
    bool pathHasSlash = path && path[0] == '/';
    const char* pathPart = (urlHasSlash && pathHasSlash && path[1] != '\0') ? path + 1 : path;
    snprintf(fullUrl, sizeof(fullUrl), "%s%s", url_, pathPart ? pathPart : "");

    ResponseBuffer rsp = {nullptr, 0, 0};

    esp_http_client_config_t cfg = {};
    cfg.url            = fullUrl;
    cfg.method         = HTTP_METHOD_GET;
    cfg.timeout_ms     = static_cast<int>(timeoutMs);
    cfg.event_handler  = httpEventHandler;
    cfg.user_data      = &rsp;
    // HTTPS works without certificate validation. The CA bundle would cost
    // both rodata and per-connection RAM that the platform doesn't have to
    // spare; HA is a private-LAN endpoint, so the trust model is the network.
    cfg.crt_bundle_attach           = nullptr;
    cfg.skip_cert_common_name_check = true;

    if (strcmp(method, "POST") == 0) {
        cfg.method = HTTP_METHOD_POST;
    }

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        return HaResult::OUT_OF_MEMORY;
    }

    char authHeader[MAX_TOKEN_LEN + 16] = {};
    snprintf(authHeader, sizeof(authHeader), "Bearer %s", token_);
    esp_http_client_set_header(client, "Authorization", authHeader);
    esp_http_client_set_header(client, "Content-Type", "application/json");

    if (body) {
        esp_http_client_set_post_field(client, body, strlen(body));
    }

    esp_err_t err = esp_http_client_perform(client);
    lastHttpStatus_ = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        free(rsp.data);
        LOG_W(TAG, "HTTP %s %s failed: %s", method, path, esp_err_to_name(err));
        return (err == ESP_ERR_TIMEOUT) ? HaResult::TIMEOUT : HaResult::TCP_FAIL;
    }
    if (lastHttpStatus_ == 401 || lastHttpStatus_ == 403) {
        free(rsp.data);
        return HaResult::UNAUTHORIZED;
    }
    if (lastHttpStatus_ >= 500) {
        free(rsp.data);
        return HaResult::SERVER_ERROR;
    }
    if (lastHttpStatus_ < 200 || lastHttpStatus_ >= 300) {
        free(rsp.data);
        return HaResult::SERVER_ERROR;
    }

    if (responseOut)    *responseOut    = rsp.data;
    else                free(rsp.data);
    if (responseLenOut) *responseLenOut = rsp.len;
    return HaResult::OK;
}

HaResult HaClient::ping() {
    char* response = nullptr;
    size_t responseLen = 0;
    HaResult res = performRequest("GET", "/api/", nullptr,
                                  &response, &responseLen, GET_TIMEOUT_MS);
    free(response);
    return res;
}

HaResult HaClient::getStates(std::vector<HaEntityState>& out) {
    char* response = nullptr;
    size_t responseLen = 0;
    HaResult res = performRequest("GET", "/api/states", nullptr,
                                  &response, &responseLen, GET_TIMEOUT_MS);
    if (res != HaResult::OK) {
        free(response);
        return res;
    }

    cJSON* root = cJSON_ParseWithLength(response, responseLen);
    free(response);
    if (!root || !cJSON_IsArray(root)) {
        cJSON_Delete(root);
        return HaResult::PARSE_ERROR;
    }

    int arraySize = cJSON_GetArraySize(root);
    out.reserve(out.size() + arraySize);

    for (int i = 0; i < arraySize; i++) {
        cJSON* item = cJSON_GetArrayItem(root, i);
        if (!item) continue;
        cJSON* idJ    = cJSON_GetObjectItem(item, "entity_id");
        cJSON* stateJ = cJSON_GetObjectItem(item, "state");
        cJSON* attrJ  = cJSON_GetObjectItem(item, "attributes");
        if (!cJSON_IsString(idJ)) continue;

        HaDomain domain = parseDomain(idJ->valuestring);
        if (domain == HaDomain::UNKNOWN) continue;

        HaEntityState entry = {};
        strncpy(entry.entity_id, idJ->valuestring, sizeof(entry.entity_id) - 1);
        entry.domain = static_cast<uint8_t>(domain);

        if (attrJ) {
            cJSON* nameJ = cJSON_GetObjectItem(attrJ, "friendly_name");
            if (cJSON_IsString(nameJ)) {
                strncpy(entry.friendly_name, nameJ->valuestring,
                        sizeof(entry.friendly_name) - 1);
            }
            if (domain == HaDomain::LIGHT) {
                cJSON* brJ = cJSON_GetObjectItem(attrJ, "brightness");
                if (cJSON_IsNumber(brJ)) {
                    // HA returns 0-255, convert to 0-100
                    int pct = (brJ->valueint * 100 + 127) / 255;
                    if (pct < 0)   pct = 0;
                    if (pct > 100) pct = 100;
                    entry.brightness = static_cast<uint8_t>(pct);
                }
            }
        }
        if (entry.friendly_name[0] == '\0') {
            strncpy(entry.friendly_name, entry.entity_id,
                    sizeof(entry.friendly_name) - 1);
        }

        if (cJSON_IsString(stateJ)) {
            const char* s = stateJ->valuestring;
            if (strcmp(s, "on") == 0) {
                entry.state = static_cast<uint8_t>(HaState::ON);
            } else if (strcmp(s, "off") == 0) {
                entry.state = static_cast<uint8_t>(HaState::OFF);
            } else if (strcmp(s, "unavailable") == 0) {
                entry.state = static_cast<uint8_t>(HaState::UNAVAILABLE);
            } else {
                entry.state = static_cast<uint8_t>(HaState::UNKNOWN);
            }
        } else {
            entry.state = static_cast<uint8_t>(HaState::UNKNOWN);
        }
        out.push_back(entry);
    }

    cJSON_Delete(root);
    return HaResult::OK;
}

HaResult HaClient::callService(const char* domain,
                               const char* service,
                               const char* entityId,
                               const char* extraParamsJson) {
    if (!domain || !service || !entityId) {
        return HaResult::NO_CONFIG;
    }

    char path[96] = {};
    snprintf(path, sizeof(path), "/api/services/%s/%s", domain, service);

    char body[256] = {};
    if (extraParamsJson && extraParamsJson[0] != '\0') {
        snprintf(body, sizeof(body), "{\"entity_id\":\"%s\",%s}", entityId, extraParamsJson);
    } else {
        snprintf(body, sizeof(body), "{\"entity_id\":\"%s\"}", entityId);
    }

    char* response = nullptr;
    size_t responseLen = 0;
    HaResult res = performRequest("POST", path, body,
                                  &response, &responseLen, POST_TIMEOUT_MS);
    free(response);
    return res;
}

} // namespace cdc::mod_homeassistant
