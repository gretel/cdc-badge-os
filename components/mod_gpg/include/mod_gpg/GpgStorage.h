#pragma once

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// === Slot Range Management ===

void gpg_storage_set_slot_range(uint16_t eccStart, uint16_t eccEnd);
void gpg_storage_set_rmem_range(uint16_t rmemStart, uint16_t rmemEnd);
bool gpg_storage_ready(void);
uint8_t gpg_storage_sig_slot(void);
uint8_t gpg_storage_dec_slot(void);
uint8_t gpg_storage_aut_slot(void);

// === DEC Private Key Storage (Encrypted in R-Memory) ===
//
// SECURITY NOTE:
// The TROPIC01 secure element does NOT support native ECDH operations.
// For GPG decryption (PSO:DECIPHER), the DEC private key is stored
// encrypted in R-Memory using AES-256-GCM with a PIN-derived key.
//
// See docs/GPG_ECDH_SECURITY.md for security analysis.

/**
 * Save DEC private key encrypted to R-Memory
 * @param privkey    32-byte P-256 private key scalar
 * @param pin        User PIN (PW1) for key derivation
 * @return true on success
 *
 * Storage format in R-Memory:
 * [4 bytes]  Magic ("ECDH")
 * [12 bytes] AES-GCM Nonce
 * [32 bytes] Encrypted Private Key
 * [16 bytes] GCM Authentication Tag
 */
bool gpg_storage_save_dec_privkey(const uint8_t* privkey, const char* pin);

/**
 * Load and decrypt DEC private key from R-Memory
 * @param privkey_out 32-byte output buffer for private key
 * @param pin         User PIN (PW1) for key derivation
 * @return true on success
 *
 * SECURITY: Caller MUST clear privkey_out buffer after use!
 */
bool gpg_storage_load_dec_privkey(uint8_t* privkey_out, const char* pin);

/**
 * Check if DEC private key exists in R-Memory
 */
bool gpg_storage_has_dec_privkey(void);

/**
 * Delete DEC private key from R-Memory
 */
bool gpg_storage_delete_dec_privkey(void);

/**
 * Get current PW1 PIN hash for key derivation
 * Used internally for session-based decryption
 * @param hash_out  32-byte output buffer
 * @return true if PIN is set and verified in current session
 */
bool gpg_storage_get_session_key(uint8_t* key_out);

/**
 * Set session key after successful PIN verification
 * Called by VERIFY command handler
 * @param pin  Verified PIN string
 */
void gpg_storage_set_session_pin(const char* pin);

/**
 * Clear session key (on deselect or timeout)
 */
void gpg_storage_clear_session(void);

#ifdef __cplusplus
}
#endif
