#include "mod_vcard/vcard_store.h"

#include "cdc_core/Hash.h"
#include "cdc_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <cctype>
#include <cstdio>
#include <cstring>

static const char* TAG = "VCARD";

static constexpr const char* VCARD_NAMESPACE = "mod_vcard";
static constexpr const char* VCARD_KEY_OWN = "own";

static char g_own_vcard[VCARD_MAX_LEN + 1];
static bool g_own_loaded = false;
static bool g_own_present = false;

#ifdef __DOXYGEN__
namespace cdc::mod_vcard {
#endif

typedef struct {
    bool used;
    uint32_t hash;
    char last_name[32];
    char display[64];
} vcard_meta_t;

#ifdef __DOXYGEN__
} // namespace cdc::mod_vcard
#endif

static vcard_meta_t g_cards[VCARD_MAX_CARDS];
static bool g_cards_loaded = false;
static uint16_t g_card_count = 0;

/**
 * \brief Computes FNV-1a hash for vCard duplicate tracking.
 * \param data Input buffer.
 * \param len Input length.
 * \return 32-bit hash value.
 */
static uint32_t fnv1a_hash(const char* data, size_t len) {
    return cdc::core::hash::fnv1a_32(reinterpret_cast<const uint8_t*>(data), len);
}

/**
 * \brief Formats NVS key name for a card slot.
 * \param out Output key buffer.
 * \param out_len Output buffer size.
 * \param slot Slot index.
 */
static void vcard_key_for_slot(char* out, size_t out_len, uint16_t slot) {
    snprintf(out, out_len, "c%02u", static_cast<unsigned>(slot));
}

/**
 * \brief Trims trailing CR/LF characters from one line.
 * \param line Mutable line buffer.
 */
static void vcard_trim_cr(char* line) {
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) {
        line[len - 1] = '\0';
        len--;
    }
}

/**
 * \brief Checks whether a vCard line contains meaningful field content.
 * \param line Line pointer.
 * \param len Line length.
 * \return `true` if line should be retained.
 */
static bool vcard_line_has_content(const char* line, size_t len) {
    const char* colon = static_cast<const char*>(memchr(line, ':', len));
    if (!colon) return true;

    const char* val = colon + 1;
    size_t val_len = len - static_cast<size_t>(val - line);

    while (val_len > 0 && (val[val_len - 1] == '\r' || val[val_len - 1] == '\n' ||
                           val[val_len - 1] == ' ' || val[val_len - 1] == '\t')) {
        val_len--;
    }

    if (val_len == 0) return false;

    if (len > 2 && (line[0] == 'N' && (line[1] == ':' || line[1] == ';'))) {
        bool has_real_content = false;
        for (size_t i = 0; i < val_len; i++) {
            if (val[i] != ';' && val[i] != ' ' && val[i] != '\t') {
                has_real_content = true;
                break;
            }
        }
        if (!has_real_content) return false;
    }

    if (len > 5 && strncasecmp(line, "IMPP:", 5) == 0) {
        const char* second_colon = strchr(val, ':');
        if (second_colon) {
            const char* actual_val = second_colon + 1;
            size_t actual_len = val_len - static_cast<size_t>(actual_val - val);
            while (actual_len > 0 && (actual_val[actual_len - 1] == '\r' ||
                   actual_val[actual_len - 1] == '\n' || actual_val[actual_len - 1] == ' ')) {
                actual_len--;
            }
            if (actual_len == 0) return false;
        }
    }

    return true;
}

/**
 * \brief Removes empty optional fields from vCard text in-place.
 * \param vcard Mutable vCard buffer.
 * \param len Input length.
 * \return New filtered length.
 */
size_t vcard_filter_empty_fields(char* vcard, size_t len) {
    char result[VCARD_MAX_LEN + 1];
    size_t out_pos = 0;

    const char* p = vcard;
    const char* end = vcard + len;

    while (p < end) {
        const char* line_start = p;
        const char* line_end = p;
        while (line_end < end && *line_end != '\n') {
            line_end++;
        }
        size_t line_len = static_cast<size_t>(line_end - line_start);

        bool keep = false;
        if (line_len >= 6 && strncasecmp(line_start, "BEGIN:", 6) == 0) keep = true;
        else if (line_len >= 8 && strncasecmp(line_start, "VERSION:", 8) == 0) keep = true;
        else if (line_len >= 4 && strncasecmp(line_start, "END:", 4) == 0) keep = true;
        else keep = vcard_line_has_content(line_start, line_len);

        if (keep && out_pos + line_len + 1 < sizeof(result)) {
            memcpy(result + out_pos, line_start, line_len);
            out_pos += line_len;
            result[out_pos++] = '\n';
        }

        p = line_end;
        if (p < end && *p == '\n') p++;
    }

    result[out_pos] = '\0';
    memcpy(vcard, result, out_pos + 1);
    return out_pos;
}

/**
 * \brief Extracts value from first vCard line matching prefix.
 * \param vcard vCard text.
 * \param prefix Line prefix (for example `FN:`).
 * \param out Output value buffer.
 * \param out_len Output buffer size.
 * \return `true` if matching line was found.
 */
static bool vcard_extract_line(const char* vcard, const char* prefix, char* out, size_t out_len) {
    if (!vcard || !prefix || !out || out_len == 0) return false;
    size_t prefix_len = strlen(prefix);
    const char* p = vcard;
    while (*p) {
        const char* line_start = p;
        const char* line_end = strpbrk(p, "\r\n");
        size_t line_len = line_end ? static_cast<size_t>(line_end - line_start) : strlen(line_start);

        if (line_len >= prefix_len && strncmp(line_start, prefix, prefix_len) == 0) {
            size_t copy_len = line_len - prefix_len;
            if (copy_len >= out_len) copy_len = out_len - 1;
            memcpy(out, line_start + prefix_len, copy_len);
            out[copy_len] = '\0';
            vcard_trim_cr(out);
            return true;
        }

        if (!line_end) break;
        p = line_end + 1;
        if (*p == '\n') p++;
    }
    return false;
}

/**
 * \brief Derives sortable last-name and display-name fields from vCard.
 * \param vcard vCard text.
 * \param last Output last-name buffer.
 * \param last_len Last-name buffer size.
 * \param display Output display-name buffer.
 * \param display_len Display-name buffer size.
 */
static void vcard_parse_names(const char* vcard, char* last, size_t last_len,
                              char* display, size_t display_len) {
    if (!last || last_len == 0 || !display || display_len == 0) return;
    last[0] = '\0';
    display[0] = '\0';

    char fn[64] = {0};
    vcard_extract_line(vcard, "FN:", fn, sizeof(fn));

    char n_line[128] = {0};
    if (!vcard_extract_line(vcard, "N:", n_line, sizeof(n_line))) {
        const char* n_tag = strstr(vcard, "\nN;");
        if (n_tag) {
            const char* val = strchr(n_tag, ':');
            if (val) {
                val++;
                size_t copy_len = 0;
                while (val[copy_len] && val[copy_len] != '\r' && val[copy_len] != '\n') {
                    copy_len++;
                }
                if (copy_len >= sizeof(n_line)) copy_len = sizeof(n_line) - 1;
                memcpy(n_line, val, copy_len);
                n_line[copy_len] = '\0';
            }
        }
    }

    if (n_line[0] != '\0') {
        char* family = n_line;
        char* given = strchr(n_line, ';');
        if (given) {
            *given = '\0';
            given++;
            char* next_semi = strchr(given, ';');
            if (next_semi) *next_semi = '\0';
        }
        if (family && *family) {
            strncpy(last, family, last_len - 1);
            last[last_len - 1] = '\0';
        }
        if (display[0] == '\0') {
            if (given && *given && family && *family) {
                snprintf(display, display_len, "%s %s", given, family);
            } else if (given && *given) {
                snprintf(display, display_len, "%s", given);
            } else if (family && *family) {
                snprintf(display, display_len, "%s", family);
            }
        }
    }

    if (display[0] == '\0' && fn[0] != '\0') {
        strncpy(display, fn, display_len - 1);
        display[display_len - 1] = '\0';
    }

    if (display[0] == '\0') {
        strncpy(display, "vCard", display_len - 1);
        display[display_len - 1] = '\0';
    }
}

/**
 * \brief Writes error text into bounded output buffer.
 * \param err Output error buffer.
 * \param err_len Output buffer size.
 * \param msg Error message text.
 */
static void set_err(char* err, size_t err_len, const char* msg) {
    if (!err || err_len == 0) return;
    strncpy(err, msg ? msg : "", err_len - 1);
    err[err_len - 1] = '\0';
}

/**
 * \brief Validates basic vCard format constraints.
 * \param vcard vCard text buffer.
 * \param len Input length.
 * \param err Output error buffer.
 * \param err_len Error buffer size.
 * \return `true` if vCard is acceptable.
 */
static bool vcard_validate(const char* vcard, size_t len, char* err, size_t err_len) {
    if (!vcard || len == 0) {
        set_err(err, err_len, "Empty vCard");
        return false;
    }
    if (len > VCARD_MAX_LEN) {
        set_err(err, err_len, "vCard too large");
        return false;
    }
    if (memchr(vcard, '\0', len) != nullptr) {
        set_err(err, err_len, "vCard contains NUL");
        return false;
    }
    if (!strstr(vcard, "BEGIN:VCARD")) {
        set_err(err, err_len, "Missing BEGIN:VCARD");
        return false;
    }
    if (!strstr(vcard, "VERSION:4.0")) {
        set_err(err, err_len, "Missing VERSION:4.0");
        return false;
    }
    if (!strstr(vcard, "END:VCARD")) {
        set_err(err, err_len, "Missing END:VCARD");
        return false;
    }
    return true;
}

/**
 * \brief Lazily loads local own-vCard from NVS cache.
 */
static void vcard_load_own(void) {
    if (g_own_loaded) return;
    g_own_vcard[0] = '\0';
    g_own_present = false;

    nvs_handle_t nvs;
    if (nvs_open(VCARD_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        g_own_loaded = true;
        return;
    }

    size_t len = sizeof(g_own_vcard);
    if (nvs_get_str(nvs, VCARD_KEY_OWN, g_own_vcard, &len) == ESP_OK) {
        g_own_present = true;
    }
    nvs_close(nvs);
    g_own_loaded = true;
}

/**
 * \brief Stores local own-vCard after validation and field filtering.
 * \param vcard vCard text.
 * \param len Input length.
 * \param err Output error buffer.
 * \param err_len Error buffer size.
 * \return `true` on successful persistence.
 */
bool vcard_store_set_own(const char* vcard, size_t len, char* err, size_t err_len) {
    if (!vcard_validate(vcard, len, err, err_len)) {
        return false;
    }

    char tmp[VCARD_MAX_LEN + 1];
    if (len > VCARD_MAX_LEN) {
        set_err(err, err_len, "vCard too large");
        return false;
    }
    memcpy(tmp, vcard, len);
    tmp[len] = '\0';

    len = vcard_filter_empty_fields(tmp, len);

    nvs_handle_t nvs;
    if (nvs_open(VCARD_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        set_err(err, err_len, "NVS open failed");
        return false;
    }
    esp_err_t ret = nvs_set_str(nvs, VCARD_KEY_OWN, tmp);
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs);
    }
    nvs_close(nvs);
    if (ret != ESP_OK) {
        set_err(err, err_len, "NVS write failed");
        return false;
    }

    strncpy(g_own_vcard, tmp, sizeof(g_own_vcard) - 1);
    g_own_vcard[sizeof(g_own_vcard) - 1] = '\0';
    g_own_loaded = true;
    g_own_present = true;
    LOG_I(TAG, "Own vCard stored (%d bytes)", (int)len);
    return true;
}

/**
 * \brief Retrieves local own-vCard text.
 * \param out Output buffer.
 * \param max_len Output buffer size.
 * \return Number of copied bytes.
 */
size_t vcard_store_get_own(char* out, size_t max_len) {
    if (!out || max_len == 0) return 0;
    vcard_load_own();
    if (!g_own_present) return 0;
    size_t len = strnlen(g_own_vcard, sizeof(g_own_vcard));
    if (len >= max_len) len = max_len - 1;
    memcpy(out, g_own_vcard, len);
    out[len] = '\0';
    return len;
}

/**
 * \brief Returns whether local own-vCard exists.
 * \return `true` if own-vCard is present.
 */
bool vcard_store_has_own(void) {
    vcard_load_own();
    return g_own_present;
}

/**
 * \brief Retrieves display name derived from local own-vCard.
 * \param out Output buffer.
 * \param max_len Output buffer size.
 * \return `true` on success.
 */
bool vcard_store_get_display_own(char* out, size_t max_len) {
    if (!out || max_len == 0) return false;
    vcard_load_own();
    if (!g_own_present) return false;
    char last[32];
    char display[64];
    vcard_parse_names(g_own_vcard, last, sizeof(last), display, sizeof(display));
    strncpy(out, display, max_len - 1);
    out[max_len - 1] = '\0';
    return true;
}

/**
 * \brief Deletes local own-vCard from storage.
 * \return `true` on successful deletion.
 */
bool vcard_store_clear_own(void) {
    nvs_handle_t nvs;
    if (nvs_open(VCARD_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        return false;
    }
    esp_err_t ret = nvs_erase_key(nvs, VCARD_KEY_OWN);
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs);
    }
    nvs_close(nvs);
    g_own_vcard[0] = '\0';
    g_own_present = false;
    g_own_loaded = true;
    return ret == ESP_OK;
}

/**
 * \brief Initializes metadata cache for stored peer vCards.
 */
void vcard_store_init(void) {
    if (g_cards_loaded) return;

    memset(g_cards, 0, sizeof(g_cards));
    g_card_count = 0;

    nvs_handle_t nvs;
    if (nvs_open(VCARD_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        g_cards_loaded = true;
        return;
    }

    for (uint16_t slot = 0; slot < VCARD_MAX_CARDS; slot++) {
        char key[8];
        vcard_key_for_slot(key, sizeof(key), slot);
        size_t len = 0;
        if (nvs_get_str(nvs, key, nullptr, &len) != ESP_OK || len == 0 || len > VCARD_MAX_LEN) {
            continue;
        }
        char tmp[VCARD_MAX_LEN + 1];
        if (nvs_get_str(nvs, key, tmp, &len) == ESP_OK) {
            tmp[VCARD_MAX_LEN] = '\0';
            vcard_parse_names(tmp, g_cards[slot].last_name, sizeof(g_cards[slot].last_name),
                              g_cards[slot].display, sizeof(g_cards[slot].display));
            g_cards[slot].used = true;
            g_cards[slot].hash = fnv1a_hash(tmp, strnlen(tmp, VCARD_MAX_LEN));
            g_card_count++;
        }
    }
    nvs_close(nvs);
    g_cards_loaded = true;
}

/**
 * \brief Returns number of stored peer vCards.
 * \return Peer vCard count.
 */
uint16_t vcard_store_count(void) {
    vcard_store_init();
    return g_card_count;
}

/**
 * \brief Checks whether candidate vCard is already stored.
 * \param nvs Open NVS handle.
 * \param vcard Candidate vCard text.
 * \param len Candidate length.
 * \param hash Candidate hash (currently unused optimization hint).
 * \return `true` if duplicate exists.
 */
static bool vcard_is_duplicate(nvs_handle_t nvs, const char* vcard, size_t len, uint32_t hash) {
    for (uint16_t slot = 0; slot < VCARD_MAX_CARDS; slot++) {
        char key[8];
        vcard_key_for_slot(key, sizeof(key), slot);
        size_t vlen = 0;
        if (nvs_get_str(nvs, key, nullptr, &vlen) != ESP_OK || vlen == 0 || vlen > VCARD_MAX_LEN) {
            continue;
        }
        char tmp[VCARD_MAX_LEN + 1];
        if (nvs_get_str(nvs, key, tmp, &vlen) == ESP_OK) {
            tmp[VCARD_MAX_LEN] = '\0';
            if (strlen(tmp) == len && memcmp(tmp, vcard, len) == 0) {
                return true;
            }
        }
        (void)hash;
    }
    return false;
}

/**
 * \brief Adds peer vCard to first free slot after validation and duplicate check.
 * \param vcard vCard text.
 * \param len Input length.
 * \param err Output error buffer.
 * \param err_len Error buffer size.
 * \return `true` on successful storage.
 */
bool vcard_store_add(const char* vcard, size_t len, char* err, size_t err_len) {
    if (!vcard_validate(vcard, len, err, err_len)) {
        return false;
    }
    vcard_store_init();
    if (g_card_count >= VCARD_MAX_CARDS) {
        set_err(err, err_len, "vCard list full");
        return false;
    }

    nvs_handle_t nvs;
    if (nvs_open(VCARD_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        set_err(err, err_len, "NVS open failed");
        return false;
    }

    uint32_t hash = fnv1a_hash(vcard, len);
    if (vcard_is_duplicate(nvs, vcard, len, hash)) {
        nvs_close(nvs);
        set_err(err, err_len, "Duplicate vCard");
        return false;
    }

    int free_slot = -1;
    for (uint16_t i = 0; i < VCARD_MAX_CARDS; i++) {
        if (!g_cards[i].used) {
            free_slot = static_cast<int>(i);
            break;
        }
    }

    if (free_slot < 0) {
        nvs_close(nvs);
        set_err(err, err_len, "vCard list full");
        return false;
    }

    char key[8];
    vcard_key_for_slot(key, sizeof(key), static_cast<uint16_t>(free_slot));
    char tmp[VCARD_MAX_LEN + 1];
    memcpy(tmp, vcard, len);
    tmp[len] = '\0';
    len = vcard_filter_empty_fields(tmp, len);

    esp_err_t ret = nvs_set_str(nvs, key, tmp);
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs);
    }
    nvs_close(nvs);
    if (ret != ESP_OK) {
        set_err(err, err_len, "NVS write failed");
        return false;
    }

    vcard_parse_names(tmp, g_cards[free_slot].last_name, sizeof(g_cards[free_slot].last_name),
                      g_cards[free_slot].display, sizeof(g_cards[free_slot].display));
    g_cards[free_slot].used = true;
    g_cards[free_slot].hash = hash;
    g_card_count++;
    return true;
}

/**
 * \brief Deletes peer vCard at slot index.
 * \param slot Slot index.
 * \return `true` on successful deletion.
 */
bool vcard_store_delete(uint16_t slot) {
    vcard_store_init();
    if (slot >= VCARD_MAX_CARDS || !g_cards[slot].used) {
        return false;
    }
    nvs_handle_t nvs;
    if (nvs_open(VCARD_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        return false;
    }
    char key[8];
    vcard_key_for_slot(key, sizeof(key), slot);
    esp_err_t ret = nvs_erase_key(nvs, key);
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs);
    }
    nvs_close(nvs);
    if (ret != ESP_OK) return false;
    g_cards[slot].used = false;
    g_card_count--;
    return true;
}

/**
 * \brief Retrieves raw vCard text from slot.
 * \param slot Slot index.
 * \param out Output buffer.
 * \param max_len Output buffer size.
 * \return Number of copied bytes.
 */
size_t vcard_store_get(uint16_t slot, char* out, size_t max_len) {
    if (!out || max_len == 0) return 0;
    vcard_store_init();
    if (slot >= VCARD_MAX_CARDS || !g_cards[slot].used) {
        return 0;
    }

    nvs_handle_t nvs;
    if (nvs_open(VCARD_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return 0;
    }
    char key[8];
    vcard_key_for_slot(key, sizeof(key), slot);
    size_t len = max_len;
    if (nvs_get_str(nvs, key, out, &len) != ESP_OK) {
        nvs_close(nvs);
        return 0;
    }
    nvs_close(nvs);
    if (len > 0 && len <= max_len) {
        out[len - 1] = '\0';
    } else {
        out[max_len - 1] = '\0';
    }
    return strnlen(out, max_len);
}

/**
 * \brief Retrieves cached display label for slot.
 * \param slot Slot index.
 * \param out Output label buffer.
 * \param max_len Output buffer size.
 * \return `true` on success.
 */
bool vcard_store_get_display(uint16_t slot, char* out, size_t max_len) {
    if (!out || max_len == 0) return false;
    vcard_store_init();
    if (slot >= VCARD_MAX_CARDS || !g_cards[slot].used) {
        return false;
    }
    strncpy(out, g_cards[slot].display, max_len - 1);
    out[max_len - 1] = '\0';
    return true;
}

/**
 * \brief Returns slot indices of stored cards sorted by last name.
 * \param out_slots Output slot-index array.
 * \param max_slots Maximum writable entries.
 * \return Number of returned slot indices.
 */
uint16_t vcard_store_get_sorted(uint16_t* out_slots, uint16_t max_slots) {
    if (!out_slots || max_slots == 0) return 0;
    vcard_store_init();

    uint16_t count = 0;
    for (uint16_t i = 0; i < VCARD_MAX_CARDS && count < max_slots; i++) {
        if (g_cards[i].used) {
            out_slots[count++] = i;
        }
    }

    for (uint16_t i = 0; i < count; i++) {
        for (uint16_t j = i + 1; j < count; j++) {
            uint16_t a = out_slots[i];
            uint16_t b = out_slots[j];
            if (strcasecmp(g_cards[a].last_name, g_cards[b].last_name) > 0) {
                uint16_t tmp = out_slots[i];
                out_slots[i] = out_slots[j];
                out_slots[j] = tmp;
            }
        }
    }
    return count;
}
