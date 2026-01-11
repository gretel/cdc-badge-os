#include "vcard_store.h"

#if FEATURE_BLE_BADGE

#include "cdc_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define VCARD_NAMESPACE "vcard"
#define VCARD_KEY_OWN   "own"

static char g_own_vcard[VCARD_MAX_LEN + 1];
static bool g_own_loaded = false;
static bool g_own_present = false;

typedef struct {
    bool used;
    uint32_t hash;
    char last_name[32];
    char display[64];
} vcard_meta_t;

static vcard_meta_t g_cards[VCARD_MAX_CARDS];
static bool g_cards_loaded = false;
static uint16_t g_card_count = 0;

static uint32_t fnv1a_hash(const char *data, size_t len) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint8_t)data[i];
        hash *= 16777619u;
    }
    return hash;
}

static void vcard_key_for_slot(char *out, size_t out_len, uint16_t slot) {
    snprintf(out, out_len, "c%02u", (unsigned)slot);
}

static void vcard_trim_cr(char *line) {
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) {
        line[len - 1] = '\0';
        len--;
    }
}

// Check if a vCard line has actual content after the colon
// Returns true if line has data (should be kept), false if empty
static bool vcard_line_has_content(const char *line, size_t len) {
    const char *colon = (const char *)memchr(line, ':', len);
    if (!colon) return true;  // No colon = keep (invalid line, but keep for safety)

    // Special case: N: field with just semicolons is empty (N:;;;; or N:;; etc)
    // Check what comes after the colon
    const char *val = colon + 1;
    size_t val_len = len - (size_t)(val - line);

    // Trim trailing whitespace/CR/LF from value
    while (val_len > 0 && (val[val_len - 1] == '\r' || val[val_len - 1] == '\n' ||
                          val[val_len - 1] == ' ' || val[val_len - 1] == '\t')) {
        val_len--;
    }

    // Empty value?
    if (val_len == 0) return false;

    // Check for N: field with only semicolons (e.g., N:;;;; or N:;;)
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

    // Check for IMPP with only protocol prefix (e.g., IMPP:telegram:)
    if (len > 5 && strncasecmp(line, "IMPP:", 5) == 0) {
        // Find the second colon (protocol:value)
        const char *second_colon = strchr(val, ':');
        if (second_colon) {
            const char *actual_val = second_colon + 1;
            size_t actual_len = val_len - (size_t)(actual_val - val);
            while (actual_len > 0 && (actual_val[actual_len - 1] == '\r' ||
                   actual_val[actual_len - 1] == '\n' || actual_val[actual_len - 1] == ' ')) {
                actual_len--;
            }
            if (actual_len == 0) return false;
        }
    }

    return true;
}

// Filter empty lines from vCard, keeping only lines with actual content
// Returns new length - exported as vcard_filter_empty_fields()
size_t vcard_filter_empty_fields(char *vcard, size_t len) {
    char result[VCARD_MAX_LEN + 1];
    size_t out_pos = 0;

    const char *p = vcard;
    const char *end = vcard + len;

    while (p < end) {
        // Find end of line
        const char *line_start = p;
        const char *line_end = p;
        while (line_end < end && *line_end != '\n') {
            line_end++;
        }
        size_t line_len = (size_t)(line_end - line_start);

        // Always keep BEGIN:VCARD, VERSION, END:VCARD
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

        // Move past newline
        p = line_end;
        if (p < end && *p == '\n') p++;
    }

    if (out_pos > 0 && result[out_pos - 1] == '\n') {
        // Keep the trailing newline but null-terminate
    }
    result[out_pos] = '\0';

    memcpy(vcard, result, out_pos + 1);
    return out_pos;
}

static bool vcard_extract_line(const char *vcard, const char *prefix, char *out, size_t out_len) {
    if (!vcard || !prefix || !out || out_len == 0) return false;
    size_t prefix_len = strlen(prefix);
    const char *p = vcard;
    while (*p) {
        const char *line_start = p;
        const char *line_end = strpbrk(p, "\r\n");
        size_t line_len = line_end ? (size_t)(line_end - line_start) : strlen(line_start);

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

static void vcard_parse_names(const char *vcard, char *last, size_t last_len, char *display, size_t display_len) {
    if (!last || last_len == 0 || !display || display_len == 0) return;
    last[0] = '\0';
    display[0] = '\0';

    char fn[64] = {0};
    vcard_extract_line(vcard, "FN:", fn, sizeof(fn));

    // Handle N: (Family;Given;Additional;Prefix;Suffix)
    char n_line[128] = {0};
    if (!vcard_extract_line(vcard, "N:", n_line, sizeof(n_line))) {
        // Try parameterized N;...:
        const char *n_tag = strstr(vcard, "\nN;");
        if (n_tag) {
            const char *val = strchr(n_tag, ':');
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
        char *family = n_line;
        char *given = strchr(n_line, ';');
        if (given) {
            *given = '\0';
            given++;
            // Truncate at next semicolon (Additional;Prefix;Suffix)
            char *next_semi = strchr(given, ';');
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

    if (last[0] == '\0' && fn[0] != '\0') {
        // Fallback: use last word of FN
        const char *last_space = strrchr(fn, ' ');
        if (last_space && *(last_space + 1)) {
            strncpy(last, last_space + 1, last_len - 1);
            last[last_len - 1] = '\0';
        } else {
            strncpy(last, fn, last_len - 1);
            last[last_len - 1] = '\0';
        }
    }

    if (display[0] == '\0') {
        strncpy(display, "vCard", display_len - 1);
        display[display_len - 1] = '\0';
    }
}

static void set_err(char *err, size_t err_len, const char *msg) {
    if (!err || err_len == 0) return;
    strncpy(err, msg, err_len - 1);
    err[err_len - 1] = '\0';
}

static bool vcard_validate(const char *vcard, size_t len, char *err, size_t err_len) {
    if (!vcard || len == 0) {
        set_err(err, err_len, "Empty vCard");
        return false;
    }
    if (len > VCARD_MAX_LEN) {
        set_err(err, err_len, "vCard too large");
        return false;
    }
    // Ensure text is null-terminated within limit
    if (memchr(vcard, '\0', len) != NULL) {
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

static void vcard_load_own(void) {
    if (g_own_loaded) return;
    g_own_loaded = true;
    g_own_present = false;
    g_own_vcard[0] = '\0';

    nvs_handle_t nvs;
    if (nvs_open(VCARD_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return;
    }

    size_t len = sizeof(g_own_vcard);
    if (nvs_get_str(nvs, VCARD_KEY_OWN, g_own_vcard, &len) == ESP_OK) {
        g_own_present = true;
    }
    nvs_close(nvs);
}

bool vcard_store_set_own(const char *vcard, size_t len, char *err, size_t err_len) {
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

    // Filter out empty lines before storing
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

    LOG_I("VCARD", "Own vCard stored (%d bytes)", (int)len);
    return true;
}

size_t vcard_store_get_own(char *out, size_t max_len) {
    if (!out || max_len == 0) return 0;
    vcard_load_own();
    if (!g_own_present) return 0;

    size_t len = strnlen(g_own_vcard, sizeof(g_own_vcard));
    if (len + 1 > max_len) {
        len = max_len - 1;
    }
    memcpy(out, g_own_vcard, len);
    out[len] = '\0';
    return len;
}

bool vcard_store_has_own(void) {
    vcard_load_own();
    return g_own_present;
}

bool vcard_store_get_display_own(char *out, size_t max_len) {
    if (!out || max_len == 0) return false;
    vcard_load_own();
    if (!g_own_present) return false;
    char last[64] = {0};
    char display[64] = {0};
    vcard_parse_names(g_own_vcard, last, sizeof(last), display, sizeof(display));
    strncpy(out, display, max_len - 1);
    out[max_len - 1] = '\0';
    return true;
}

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
    if (ret != ESP_OK) {
        return false;
    }
    g_own_present = false;
    g_own_loaded = true;
    g_own_vcard[0] = '\0';
    return true;
}

void vcard_store_init(void) {
    if (g_cards_loaded) return;
    g_cards_loaded = true;
    g_card_count = 0;
    memset(g_cards, 0, sizeof(g_cards));

    nvs_handle_t nvs;
    if (nvs_open(VCARD_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return;
    }

    for (uint16_t slot = 0; slot < VCARD_MAX_CARDS; slot++) {
        char key[8];
        vcard_key_for_slot(key, sizeof(key), slot);
        size_t len = 0;
        if (nvs_get_str(nvs, key, NULL, &len) != ESP_OK || len == 0 || len > VCARD_MAX_LEN) {
            continue;
        }
        char tmp[VCARD_MAX_LEN + 1];
        if (nvs_get_str(nvs, key, tmp, &len) == ESP_OK) {
            tmp[VCARD_MAX_LEN] = '\0';
            g_cards[slot].used = true;
            g_cards[slot].hash = fnv1a_hash(tmp, strlen(tmp));
            vcard_parse_names(tmp, g_cards[slot].last_name, sizeof(g_cards[slot].last_name),
                              g_cards[slot].display, sizeof(g_cards[slot].display));
            g_card_count++;
        }
    }
    nvs_close(nvs);
}

uint16_t vcard_store_count(void) {
    vcard_store_init();
    return g_card_count;
}

static bool vcard_is_duplicate(nvs_handle_t nvs, const char *vcard, size_t len, uint32_t hash) {
    for (uint16_t slot = 0; slot < VCARD_MAX_CARDS; slot++) {
        if (!g_cards[slot].used || g_cards[slot].hash != hash) continue;
        char key[8];
        vcard_key_for_slot(key, sizeof(key), slot);
        size_t existing_len = 0;
        if (nvs_get_str(nvs, key, NULL, &existing_len) != ESP_OK || existing_len == 0) {
            continue;
        }
        char tmp[VCARD_MAX_LEN + 1];
        if (nvs_get_str(nvs, key, tmp, &existing_len) == ESP_OK) {
            tmp[VCARD_MAX_LEN] = '\0';
            if (strlen(tmp) == len && memcmp(tmp, vcard, len) == 0) {
                return true;
            }
        }
    }
    return false;
}

bool vcard_store_add(const char *vcard, size_t len, char *err, size_t err_len) {
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
            free_slot = i;
            break;
        }
    }
    if (free_slot < 0) {
        nvs_close(nvs);
        set_err(err, err_len, "vCard list full");
        return false;
    }

    char key[8];
    vcard_key_for_slot(key, sizeof(key), (uint16_t)free_slot);
    char tmp[VCARD_MAX_LEN + 1];
    memcpy(tmp, vcard, len);
    tmp[len] = '\0';

    esp_err_t ret = nvs_set_str(nvs, key, tmp);
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs);
    }
    nvs_close(nvs);
    if (ret != ESP_OK) {
        set_err(err, err_len, "NVS write failed");
        return false;
    }

    g_cards[free_slot].used = true;
    g_cards[free_slot].hash = hash;
    vcard_parse_names(tmp, g_cards[free_slot].last_name, sizeof(g_cards[free_slot].last_name),
                      g_cards[free_slot].display, sizeof(g_cards[free_slot].display));
    g_card_count++;
    return true;
}

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
    if (ret != ESP_OK) {
        return false;
    }

    g_cards[slot].used = false;
    g_cards[slot].hash = 0;
    g_cards[slot].last_name[0] = '\0';
    g_cards[slot].display[0] = '\0';
    if (g_card_count > 0) g_card_count--;
    return true;
}

size_t vcard_store_get(uint16_t slot, char *out, size_t max_len) {
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
    if (len > 0 && len < max_len) {
        out[len - 1] = '\0';
    } else {
        out[max_len - 1] = '\0';
    }
    return strlen(out);
}

bool vcard_store_get_display(uint16_t slot, char *out, size_t max_len) {
    if (!out || max_len == 0) return false;
    vcard_store_init();
    if (slot >= VCARD_MAX_CARDS || !g_cards[slot].used) {
        return false;
    }
    strncpy(out, g_cards[slot].display, max_len - 1);
    out[max_len - 1] = '\0';
    return true;
}

static int compare_slots(uint16_t a, uint16_t b) {
    const char *la = g_cards[a].last_name;
    const char *lb = g_cards[b].last_name;
    while (*la && *lb) {
        char ca = (char)tolower((unsigned char)*la++);
        char cb = (char)tolower((unsigned char)*lb++);
        if (ca != cb) return (int)(unsigned char)ca - (int)(unsigned char)cb;
    }
    return (int)(unsigned char)*la - (int)(unsigned char)*lb;
}

uint16_t vcard_store_get_sorted(uint16_t *out_slots, uint16_t max_slots) {
    vcard_store_init();
    if (!out_slots || max_slots == 0) return 0;

    uint16_t count = 0;
    for (uint16_t i = 0; i < VCARD_MAX_CARDS && count < max_slots; i++) {
        if (g_cards[i].used) {
            out_slots[count++] = i;
        }
    }

    // Simple insertion sort
    for (uint16_t i = 1; i < count; i++) {
        uint16_t key = out_slots[i];
        int j = (int)i - 1;
        while (j >= 0 && compare_slots(out_slots[j], key) > 0) {
            out_slots[j + 1] = out_slots[j];
            j--;
        }
        out_slots[j + 1] = key;
    }
    return count;
}

#endif // FEATURE_BLE_BADGE
