#pragma once

// Badge Settings Module for CDC Badge
// Stores lock screen text in NVS

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BADGE_TEXT_MAX_LEN 63

// Load badge settings from NVS
void badge_settings_load(void);

// Save badge settings to NVS
void badge_settings_save(void);

// Get/Set name line (top text on lock screen)
const char* badge_settings_get_name(void);
void badge_settings_set_name(const char *name);

// Get/Set info line (middle text on lock screen)
const char* badge_settings_get_info(void);
void badge_settings_set_info(const char *info);

// Get/Set info2 line (bottom text on lock screen)
const char* badge_settings_get_info2(void);
void badge_settings_set_info2(const char *info2);

// Get/Set timezone offset (-12 to +14)
int8_t badge_settings_get_timezone(void);
void badge_settings_set_timezone(int8_t tz_offset);

#ifdef __cplusplus
}
#endif
