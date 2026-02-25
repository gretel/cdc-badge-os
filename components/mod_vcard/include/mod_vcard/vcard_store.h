#pragma once

#include <cstddef>
#include <cstdint>

#define VCARD_MAX_LEN   768
#define VCARD_MAX_CARDS 100

bool vcard_store_set_own(const char* vcard, size_t len, char* err, size_t err_len);
size_t vcard_store_get_own(char* out, size_t max_len);
size_t vcard_filter_empty_fields(char* vcard, size_t len);
bool vcard_store_has_own(void);
bool vcard_store_clear_own(void);
bool vcard_store_get_display_own(char* out, size_t max_len);
void vcard_store_init(void);
uint16_t vcard_store_count(void);
bool vcard_store_add(const char* vcard, size_t len, char* err, size_t err_len);
bool vcard_store_delete(uint16_t slot);
size_t vcard_store_get(uint16_t slot, char* out, size_t max_len);
bool vcard_store_get_display(uint16_t slot, char* out, size_t max_len);
uint16_t vcard_store_get_sorted(uint16_t* out_slots, uint16_t max_slots);
