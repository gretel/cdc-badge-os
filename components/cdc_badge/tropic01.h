#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// R-Memory slot definitions
#define TR01_RMEM_SLOT_PIN       30    // PIN hash storage
#define TR01_RMEM_SLOT_CONFIG    31    // Device config
#define TR01_RMEM_SLOT_CA        32    // CA metadata
#define TR01_RMEM_SLOT_FIDO_START 0    // FIDO2/SSH credentials: 0-28
#define TR01_RMEM_SLOT_FIDO_END   28   // Reduced from 29 to free slot 29 for GPG
#define TR01_RMEM_SLOT_TOTP_START 33   // TOTP accounts: 33-132
#define TR01_RMEM_SLOT_TOTP_END   132
#define TR01_RMEM_SLOT_GPG       29    // GPG metadata (User-ID, fingerprint, etc.)
// Password vault slots (store password + notes)
#define TR01_RMEM_SLOT_PASS_START 150  // Password entries: 150-511
#define TR01_RMEM_SLOT_PASS_END   511

// ECC slot definitions
#define TR01_ECC_SLOT_FIDO_START  0    // FIDO2/SSH keys: 0-28 (P-256 or Ed25519)
#define TR01_ECC_SLOT_FIDO_END    28   // Reduced from 29 to free slot 29 for GPG
#define TR01_ECC_SLOT_GPG         29   // GPG master key
#define TR01_ECC_SLOT_ATTEST      30   // FIDO2 attestation key
#define TR01_ECC_SLOT_CA          31   // CA root key
#define TR01_ECC_SLOT_COUNT       32

// ECC curve types (use CDC_ prefix to avoid collision with libtropic enums)
#define CDC_CURVE_ED25519  0
#define CDC_CURVE_P256     1

// Initialize TROPIC01 secure element
bool tropic01_init(void);

// Start secure session (required before operations)
bool tropic01_session_start(void);

// Check if session is active
bool tropic01_session_active(void);

// Abort session (use before deep sleep)
void tropic01_session_abort(void);

// Put TROPIC01 to sleep (manual - normally auto-sleep via R-Config)
void tropic01_sleep(void);

// R-Memory operations
bool tropic01_rmem_read(uint16_t slot, uint8_t *data, uint16_t max_size, uint16_t *read_size);
bool tropic01_rmem_write(uint16_t slot, const uint8_t *data, uint16_t size);
bool tropic01_rmem_erase(uint16_t slot);

// ECC key operations
bool tropic01_ecc_key_generate(uint8_t slot, uint8_t curve);
bool tropic01_ecc_key_read(uint8_t slot, uint8_t *pubkey, uint8_t pubkey_size,
                           uint8_t *curve, uint8_t *origin);
bool tropic01_ecc_key_erase(uint8_t slot);

// Store (import) external private key into TROPIC01
// WARNING: Private key must be valid for the specified curve
// For P-256: 32 bytes, for Ed25519: 32 bytes
bool tropic01_ecc_key_write(uint8_t slot, const uint8_t *privkey,
                            uint8_t privkey_size, uint8_t curve);

// Signing operations
bool tropic01_ecdsa_sign(uint8_t slot, const uint8_t *hash, uint32_t hash_len,
                         uint8_t *signature);
bool tropic01_eddsa_sign(uint8_t slot, const uint8_t *msg, uint16_t msg_len,
                         uint8_t *signature);

// Random number generator (hardware TRNG)
bool tropic01_get_random(uint8_t *buffer, uint16_t size);

// Secure random: TROPIC01 TRNG with ESP32 fallback
// Use this instead of esp_random() for security-critical random data
// Returns true if TROPIC01 was used, false if ESP32 fallback was used
bool secure_random(uint8_t *buffer, uint16_t size);

// Fill buffer with random bytes (esp_random style compatibility)
// Prefer secure_random() for new code
void secure_random_fill(uint8_t *buffer, size_t size);

// Diagnostics API
bool tropic01_get_chip_id(uint8_t *serial_num, uint8_t serial_size);
bool tropic01_get_fw_version(uint8_t *riscv_ver, uint8_t *spect_ver);

// ECC status structure
typedef struct {
    bool slot_used[TR01_ECC_SLOT_COUNT];
    uint8_t slot_curve[TR01_ECC_SLOT_COUNT];
} tropic01_ecc_status_t;

bool tropic01_get_ecc_status(tropic01_ecc_status_t *status);

// R-Config operations (reversible chip configuration)
bool tropic01_rconfig_read(uint8_t addr, uint32_t *value);
bool tropic01_rconfig_write(uint8_t addr, uint32_t value);
bool tropic01_rconfig_erase(void);

#ifdef __cplusplus
}
#endif
