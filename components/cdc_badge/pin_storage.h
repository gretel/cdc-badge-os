#pragma once

// PIN Storage Module for CDC Badge
// Stores PIN securely on TROPIC01 R-Memory
// Default PIN: 1234

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PIN_DEFAULT "1234"
#define PIN_MIN_LEN 4
#define PIN_MAX_LEN 6

// Load PIN from TROPIC01 (or use default if not set)
// Returns pointer to internal buffer (valid until next call)
const char* pin_storage_load(void);

// Save PIN to TROPIC01
// Returns true on success
bool pin_storage_save(const char *pin);

// Verify PIN against stored PIN
// Returns true if PIN matches
bool pin_storage_verify(const char *pin);

// Check if PIN is set (not default)
bool pin_storage_is_set(void);

#ifdef __cplusplus
}
#endif
