// Password Storage (TROPIC01 R-Memory + NVS metadata)

#ifndef PASSWORD_STORE_H
#define PASSWORD_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Metadata sizes (stored in NVS)
#define PASSWORD_NAME_LEN      32
#define PASSWORD_USERNAME_LEN  32
#define PASSWORD_URL_LEN       64

// Secret sizes (stored in TROPIC01)
#define PASSWORD_MAX_LEN       96
#define PASSWORD_NOTES_LEN     256

// UI list capacity (not necessarily storage capacity)
#define PASSWORD_MAX_ENTRIES   500

typedef struct {
    char name[PASSWORD_NAME_LEN];
    char username[PASSWORD_USERNAME_LEN];
    char url[PASSWORD_URL_LEN];
} password_meta_t;

// Initialize password store (loads metadata index from NVS).
uint16_t password_store_init(void);

// Get number of stored password entries.
uint16_t password_store_count(void);

// Get list of used TROPIC01 slots (unsorted).
uint16_t password_store_list_slots(uint16_t *out_slots, uint16_t max_slots);

// Get metadata for a slot (from NVS).
bool password_store_get_meta(uint16_t slot, password_meta_t *out);

// Read password + notes for a slot (from TROPIC01).
bool password_store_get_secret(uint16_t slot,
                               char *password_out, size_t password_len,
                               char *notes_out, size_t notes_len);

// Add a new entry. Returns true on success and outputs assigned slot.
bool password_store_add(const password_meta_t *meta, const char *password,
                        const char *notes, uint16_t *out_slot);

// Update an existing entry at slot.
bool password_store_update(uint16_t slot, const password_meta_t *meta,
                           const char *password, const char *notes);

// Delete entry at slot (TROPIC01 + NVS metadata).
bool password_store_delete(uint16_t slot);

// Clear all metadata entries (NVS only).
void password_store_clear_all(void);

// Type password via USB keyboard (optional Enter).
bool password_store_type(uint16_t slot, bool press_enter);

#ifdef __cplusplus
}
#endif

#endif // PASSWORD_STORE_H
