#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "feature_flags.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VCARD_MAX_LEN 768
#define VCARD_MAX_CARDS 100

#if FEATURE_BLE_BADGE
bool vcard_store_set_own(const char *vcard, size_t len, char *err, size_t err_len);
size_t vcard_store_get_own(char *out, size_t max_len);
size_t vcard_filter_empty_fields(char *vcard, size_t len);  // Remove empty fields from vCard
bool vcard_store_has_own(void);
bool vcard_store_clear_own(void);
bool vcard_store_get_display_own(char *out, size_t max_len);
void vcard_store_init(void);
uint16_t vcard_store_count(void);
bool vcard_store_add(const char *vcard, size_t len, char *err, size_t err_len);
bool vcard_store_delete(uint16_t slot);
size_t vcard_store_get(uint16_t slot, char *out, size_t max_len);
bool vcard_store_get_display(uint16_t slot, char *out, size_t max_len);
uint16_t vcard_store_get_sorted(uint16_t *out_slots, uint16_t max_slots);
#else
static inline bool vcard_store_set_own(const char *vcard, size_t len, char *err, size_t err_len) {
    (void)vcard; (void)len; (void)err; (void)err_len; return false;
}
static inline size_t vcard_store_get_own(char *out, size_t max_len) {
    (void)out; (void)max_len; return 0;
}
static inline bool vcard_store_has_own(void) { return false; }
static inline bool vcard_store_clear_own(void) { return false; }
static inline bool vcard_store_get_display_own(char *out, size_t max_len) {
    (void)out; (void)max_len; return false;
}
static inline void vcard_store_init(void) {}
static inline uint16_t vcard_store_count(void) { return 0; }
static inline bool vcard_store_add(const char *vcard, size_t len, char *err, size_t err_len) {
    (void)vcard; (void)len; (void)err; (void)err_len; return false;
}
static inline bool vcard_store_delete(uint16_t slot) { (void)slot; return false; }
static inline size_t vcard_store_get(uint16_t slot, char *out, size_t max_len) {
    (void)slot; (void)out; (void)max_len; return 0;
}
static inline bool vcard_store_get_display(uint16_t slot, char *out, size_t max_len) {
    (void)slot; (void)out; (void)max_len; return false;
}
static inline uint16_t vcard_store_get_sorted(uint16_t *out_slots, uint16_t max_slots) {
    (void)out_slots; (void)max_slots; return 0;
}
static inline size_t vcard_filter_empty_fields(char *vcard, size_t len) {
    (void)vcard; return len;
}
#endif

#ifdef __cplusplus
}
#endif
