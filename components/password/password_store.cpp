// Password Storage (TROPIC01 R-Memory + NVS metadata)
// Secrets (password + notes) live in TROPIC01, metadata lives in NVS.

#include "password_store.h"

#include "cdc_log.h"
#include "tropic01.h"
#include "tropic01_cache.h"  // For TR01_CACHE_DATA_SIZE
#include "keyboard_typing.h"

#include "nvs.h"
#include "nvs_flash.h"
#include <esp_attr.h>

#include <string.h>
#include <stdio.h>

#define PASS_NAMESPACE "pass"
#define PASS_KEY_PREFIX "p"

#define PASS_MAGIC 0x53534150u  // 'PASS'
#define PASS_VERSION 1

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t password_len;
    uint16_t notes_len;
} pass_header_t;
#pragma pack(pop)

EXT_RAM_BSS_ATTR static uint16_t s_slots[PASSWORD_MAX_ENTRIES];
static uint16_t s_count = 0;
static bool s_initialized = false;

static void pass_key_for_slot(char *out, size_t out_len, uint16_t slot) {
    snprintf(out, out_len, "%s%03u", PASS_KEY_PREFIX, (unsigned)slot);
}

static void sanitize_meta(password_meta_t *meta) {
    if (!meta) return;
    meta->name[PASSWORD_NAME_LEN - 1] = '\0';
    meta->username[PASSWORD_USERNAME_LEN - 1] = '\0';
    meta->url[PASSWORD_URL_LEN - 1] = '\0';
}

static bool meta_exists(nvs_handle_t nvs, uint16_t slot) {
    char key[8];
    pass_key_for_slot(key, sizeof(key), slot);
    size_t len = 0;
    return (nvs_get_blob(nvs, key, NULL, &len) == ESP_OK) && (len > 0);
}

static bool meta_read(nvs_handle_t nvs, uint16_t slot, password_meta_t *out) {
    if (!out) return false;
    char key[8];
    pass_key_for_slot(key, sizeof(key), slot);
    size_t len = sizeof(password_meta_t);
    esp_err_t err = nvs_get_blob(nvs, key, out, &len);
    if (err != ESP_OK || len == 0) {
        return false;
    }
    if (len < sizeof(password_meta_t)) {
        // Zero any missing tail if stored struct was smaller (legacy)
        memset(((uint8_t *)out) + len, 0, sizeof(password_meta_t) - len);
    }
    sanitize_meta(out);
    return true;
}

static bool meta_write(nvs_handle_t nvs, uint16_t slot, const password_meta_t *meta) {
    if (!meta) return false;
    char key[8];
    pass_key_for_slot(key, sizeof(key), slot);
    esp_err_t err = nvs_set_blob(nvs, key, meta, sizeof(password_meta_t));
    return err == ESP_OK;
}

uint16_t password_store_init(void) {
    s_count = 0;
    memset(s_slots, 0, sizeof(s_slots));

    nvs_handle_t nvs;
    if (nvs_open(PASS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        s_initialized = true;
        return 0;
    }

    for (uint16_t slot = TR01_RMEM_SLOT_PASS_START; slot <= TR01_RMEM_SLOT_PASS_END; slot++) {
        if (meta_exists(nvs, slot)) {
            if (s_count < PASSWORD_MAX_ENTRIES) {
                s_slots[s_count++] = slot;
            } else {
                break;
            }
        }
    }

    nvs_close(nvs);
    s_initialized = true;
    return s_count;
}

uint16_t password_store_count(void) {
    if (!s_initialized) {
        password_store_init();
    }
    return s_count;
}

uint16_t password_store_list_slots(uint16_t *out_slots, uint16_t max_slots) {
    if (!out_slots || max_slots == 0) return 0;
    if (!s_initialized) {
        password_store_init();
    }

    uint16_t count = (s_count < max_slots) ? s_count : max_slots;
    memcpy(out_slots, s_slots, count * sizeof(uint16_t));
    return count;
}

bool password_store_get_meta(uint16_t slot, password_meta_t *out) {
    if (!out) return false;
    nvs_handle_t nvs;
    if (nvs_open(PASS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return false;
    }
    bool ok = meta_read(nvs, slot, out);
    nvs_close(nvs);
    return ok;
}

bool password_store_get_secret(uint16_t slot,
                               char *password_out, size_t password_len,
                               char *notes_out, size_t notes_len) {
    if (!password_out || password_len == 0 || !notes_out || notes_len == 0) return false;

    uint8_t buffer[TR01_CACHE_DATA_SIZE];
    uint16_t read_size = 0;
    if (!tropic01_rmem_read(slot, buffer, sizeof(buffer), &read_size)) {
        LOG_E("PASS", "Failed to read slot %d", slot);
        return false;
    }
    if (read_size < sizeof(pass_header_t)) {
        LOG_E("PASS", "Slot %d: too small (%d)", slot, read_size);
        return false;
    }

    const pass_header_t *hdr = (const pass_header_t *)buffer;
    if (hdr->magic != PASS_MAGIC || hdr->version != PASS_VERSION) {
        LOG_E("PASS", "Slot %d: invalid header", slot);
        return false;
    }

    uint32_t total = sizeof(pass_header_t) + hdr->password_len + hdr->notes_len;
    if (total > read_size) {
        LOG_E("PASS", "Slot %d: size mismatch", slot);
        return false;
    }
    if (hdr->password_len >= password_len || hdr->notes_len >= notes_len) {
        LOG_E("PASS", "Slot %d: output buffer too small", slot);
        return false;
    }

    const uint8_t *p = buffer + sizeof(pass_header_t);
    memcpy(password_out, p, hdr->password_len);
    password_out[hdr->password_len] = '\0';
    p += hdr->password_len;
    memcpy(notes_out, p, hdr->notes_len);
    notes_out[hdr->notes_len] = '\0';

    // Clear buffer containing secrets
    memset(buffer, 0, sizeof(buffer));

    return true;
}

static bool write_secret_to_slot(uint16_t slot, const char *password, const char *notes) {
    if (!password || !notes) return false;

    size_t pass_len = strlen(password);
    size_t notes_len = strlen(notes);

    if (pass_len > PASSWORD_MAX_LEN || notes_len > PASSWORD_NOTES_LEN) {
        LOG_E("PASS", "Secret too large (pass=%d, notes=%d)", (int)pass_len, (int)notes_len);
        return false;
    }

    size_t total = sizeof(pass_header_t) + pass_len + notes_len;
    if (total > TR01_CACHE_DATA_SIZE) {
        LOG_E("PASS", "Secret exceeds slot size (%d > %d)", (int)total, TR01_CACHE_DATA_SIZE);
        return false;
    }

    uint8_t buffer[TR01_CACHE_DATA_SIZE];
    memset(buffer, 0, sizeof(buffer));

    pass_header_t *hdr = (pass_header_t *)buffer;
    hdr->magic = PASS_MAGIC;
    hdr->version = PASS_VERSION;
    hdr->password_len = (uint16_t)pass_len;
    hdr->notes_len = (uint16_t)notes_len;

    uint8_t *p = buffer + sizeof(pass_header_t);
    memcpy(p, password, pass_len);
    p += pass_len;
    memcpy(p, notes, notes_len);

    // Erase then write
    tropic01_rmem_erase(slot);
    bool ok = tropic01_rmem_write(slot, buffer, (uint16_t)total);

    // Clear sensitive buffer
    memset(buffer, 0, sizeof(buffer));

    if (!ok) {
        LOG_E("PASS", "Failed to write slot %d", slot);
    }
    return ok;
}

bool password_store_add(const password_meta_t *meta, const char *password,
                        const char *notes, uint16_t *out_slot) {
    if (!meta || !password || !notes) return false;
    if (!s_initialized) {
        password_store_init();
    }

    nvs_handle_t nvs;
    if (nvs_open(PASS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        return false;
    }

    // Find free slot (scan backwards)
    int free_slot = -1;
    for (int slot = TR01_RMEM_SLOT_PASS_END; slot >= TR01_RMEM_SLOT_PASS_START; slot--) {
        if (!meta_exists(nvs, (uint16_t)slot)) {
            free_slot = slot;
            break;
        }
    }

    if (free_slot < 0) {
        nvs_close(nvs);
        LOG_E("PASS", "No free slots");
        return false;
    }

    password_meta_t tmp = *meta;
    sanitize_meta(&tmp);

    if (!write_secret_to_slot((uint16_t)free_slot, password, notes)) {
        nvs_close(nvs);
        return false;
    }

    if (!meta_write(nvs, (uint16_t)free_slot, &tmp)) {
        nvs_close(nvs);
        LOG_E("PASS", "Failed to write metadata");
        return false;
    }
    nvs_commit(nvs);
    nvs_close(nvs);

    if (s_count < PASSWORD_MAX_ENTRIES) {
        s_slots[s_count++] = (uint16_t)free_slot;
    }

    if (out_slot) *out_slot = (uint16_t)free_slot;
    return true;
}

bool password_store_update(uint16_t slot, const password_meta_t *meta,
                           const char *password, const char *notes) {
    if (!meta || !password || !notes) return false;

    if (!write_secret_to_slot(slot, password, notes)) {
        return false;
    }

    nvs_handle_t nvs;
    if (nvs_open(PASS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        return false;
    }

    password_meta_t tmp = *meta;
    sanitize_meta(&tmp);

    bool ok = meta_write(nvs, slot, &tmp);
    if (ok) {
        nvs_commit(nvs);
    }
    nvs_close(nvs);
    return ok;
}

bool password_store_delete(uint16_t slot) {
    nvs_handle_t nvs;
    if (nvs_open(PASS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        return false;
    }

    char key[8];
    pass_key_for_slot(key, sizeof(key), slot);
    esp_err_t err = nvs_erase_key(nvs, key);
    if (err == ESP_OK) {
        nvs_commit(nvs);
    }
    nvs_close(nvs);

    if (!tropic01_rmem_erase(slot)) {
        LOG_E("PASS", "Failed to erase slot %d", slot);
        return false;
    }

    // Update in-memory list
    for (uint16_t i = 0; i < s_count; i++) {
        if (s_slots[i] == slot) {
            for (uint16_t j = i + 1; j < s_count; j++) {
                s_slots[j - 1] = s_slots[j];
            }
            s_count--;
            break;
        }
    }

    return true;
}

void password_store_clear_all(void) {
    nvs_handle_t nvs;
    if (nvs_open(PASS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        return;
    }

    for (uint16_t slot = TR01_RMEM_SLOT_PASS_START; slot <= TR01_RMEM_SLOT_PASS_END; slot++) {
        char key[8];
        pass_key_for_slot(key, sizeof(key), slot);
        nvs_erase_key(nvs, key);
    }
    nvs_commit(nvs);
    nvs_close(nvs);

    s_count = 0;
    memset(s_slots, 0, sizeof(s_slots));
}

bool password_store_type(uint16_t slot, bool press_enter) {
    char password[PASSWORD_MAX_LEN + 1];
    char notes[PASSWORD_NOTES_LEN + 1];
    notes[0] = '\0';

    if (!password_store_get_secret(slot, password, sizeof(password), notes, sizeof(notes))) {
        return false;
    }

    bool success = keyboard_type(password, press_enter);
    memset(password, 0, sizeof(password));
    memset(notes, 0, sizeof(notes));
    if (!success) {
        LOG_W("PASS", "No keyboard available (USB/BLE)");
        return false;
    }

    return true;
}
