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

// ============================================================================
// OpenPGP PIN Support (PW1 = User PIN, PW3 = Admin PIN)
// ============================================================================

// OpenPGP PIN defaults (per OpenPGP 3.4.1 spec)
#define OPENPGP_PW1_DEFAULT "123456"    // User PIN (6+ chars)
#define OPENPGP_PW3_DEFAULT "12345678"  // Admin PIN (8+ chars)
#define OPENPGP_PW1_MIN_LEN 6
#define OPENPGP_PW3_MIN_LEN 8
#define OPENPGP_PIN_MAX_LEN 32          // Practical limit for hardware keypad
#define OPENPGP_PIN_MAX_RETRIES 3

// Initialize OpenPGP PINs (call during openpgp_init)
// Loads existing PINs or sets defaults
void pin_storage_openpgp_init(void);

// Verify OpenPGP PW1 (User PIN)
// Returns true if PIN matches, decrements retry counter on failure
bool pin_storage_openpgp_verify_pw1(const char *pin);

// Verify OpenPGP PW3 (Admin PIN)
// Returns true if PIN matches, decrements retry counter on failure
bool pin_storage_openpgp_verify_pw3(const char *pin);

// Change OpenPGP PW1 (requires old PIN verification first)
bool pin_storage_openpgp_change_pw1(const char *new_pin);

// Change OpenPGP PW3 (requires old PIN verification first)
bool pin_storage_openpgp_change_pw3(const char *new_pin);

// Get remaining retry counts
uint8_t pin_storage_openpgp_pw1_retries(void);
uint8_t pin_storage_openpgp_pw3_retries(void);

// Reset retry counter (called after successful verification)
void pin_storage_openpgp_reset_pw1_retries(void);
void pin_storage_openpgp_reset_pw3_retries(void);

// Check if PINs are blocked
bool pin_storage_openpgp_pw1_blocked(void);
bool pin_storage_openpgp_pw3_blocked(void);

// Reset PINs to defaults (for factory reset)
bool pin_storage_openpgp_reset(void);

#ifdef __cplusplus
}
#endif
