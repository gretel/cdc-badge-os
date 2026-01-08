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

// ============================================================================
// FIDO2 ClientPIN Support
// ============================================================================

#define PIN_FIDO2_HASH_LEN 16  // LEFT(SHA256(PIN), 16)

// Check if FIDO2 hash is available (requires PIN v2 format)
bool pin_storage_fido2_available(void);

// Get FIDO2 PIN hash for ClientPIN protocol
// hash_out must be at least PIN_FIDO2_HASH_LEN bytes
bool pin_storage_get_fido2_hash(uint8_t *hash_out);

// Verify FIDO2 PIN hash (constant-time comparison)
// hash_in must be PIN_FIDO2_HASH_LEN bytes
bool pin_storage_verify_fido2_hash(const uint8_t *hash_in);

#ifdef __cplusplus
}
#endif
