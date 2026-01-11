#pragma once

#include "feature_flags.h"

#if FEATURE_GPG

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Constants
// ============================================================================

#define GPG_USER_ID_MAX         64      // "Name <email@example.com>"
#define GPG_FINGERPRINT_LEN     20      // SHA-1 fingerprint for GPG compatibility
#define GPG_PUBKEY_MAX_LEN      64      // 32 for Ed25519, 64 for P-256
#define GPG_SIGNATURE_MAX_LEN   64      // R+S for ECDSA/EdDSA

// Magic bytes for metadata validation
#define GPG_METADATA_MAGIC      0x4750  // "GP"
#define GPG_METADATA_VERSION    1

// ============================================================================
// Types
// ============================================================================

// GPG key status
typedef struct {
    bool initialized;                       // Key exists
    uint8_t curve;                          // CDC_CURVE_ED25519 or CDC_CURVE_P256
    char user_id[GPG_USER_ID_MAX];          // User ID string
    uint8_t fingerprint[GPG_FINGERPRINT_LEN]; // GPG fingerprint
    uint32_t created_at;                    // Unix timestamp
    uint32_t sign_count;                    // Number of signatures
} gpg_status_t;

// Metadata stored in R-Memory
typedef struct __attribute__((packed)) {
    uint16_t magic;                         // GPG_METADATA_MAGIC
    uint8_t version;                        // GPG_METADATA_VERSION
    uint8_t curve;                          // CDC_CURVE_ED25519 or CDC_CURVE_P256
    char user_id[GPG_USER_ID_MAX];          // User ID
    uint32_t created_at;                    // Unix timestamp
    uint8_t fingerprint[GPG_FINGERPRINT_LEN]; // GPG fingerprint
    uint8_t pubkey[GPG_PUBKEY_MAX_LEN];     // Public key
    uint8_t pubkey_len;                     // Actual pubkey length
    uint32_t sign_count;                    // Signature counter
    uint8_t reserved[32];                   // Future use
} gpg_metadata_t;

// ============================================================================
// Initialization
// ============================================================================

/**
 * Initialize GPG module.
 * Loads metadata from R-Memory if present.
 *
 * @return true on success
 */
bool gpg_init(void);

/**
 * Check if GPG key is configured.
 */
bool gpg_is_initialized(void);

/**
 * Get GPG key status.
 *
 * @param status Output structure
 * @return true if key exists
 */
bool gpg_get_status(gpg_status_t *status);

// ============================================================================
// Key Management
// ============================================================================

/**
 * Set pending user ID (BEFORE key generation).
 * The user ID becomes permanent once the key is generated.
 * Must be called before gpg_generate_key().
 *
 * @param user_id User ID string (max 63 chars), format: "Name <email>"
 * @return true on success
 */
bool gpg_set_pending_user_id(const char *user_id);

/**
 * Check if pending user ID is set.
 */
bool gpg_has_pending_user_id(void);

/**
 * Generate new GPG key pair.
 * Overwrites existing key if present.
 * IMPORTANT: User ID must be set via gpg_set_pending_user_id() first!
 * The fingerprint is calculated from the public key + creation time,
 * so user ID should be set before generation for proper workflow.
 *
 * @param curve CDC_CURVE_ED25519 or CDC_CURVE_P256
 * @return true on success, false if no user ID set or generation fails
 */
bool gpg_generate_key(uint8_t curve);

/**
 * Import GPG key from PEM format.
 * Expects PKCS#8 private key PEM.
 *
 * @param pem_data PEM-encoded private key (null-terminated)
 * @return true on success
 */
bool gpg_import_key_pem(const char *pem_data);

/**
 * Set user ID for the key.
 *
 * @param user_id User ID string (max 63 chars)
 * @return true on success
 */
bool gpg_set_user_id(const char *user_id);

/**
 * Delete GPG key and metadata.
 *
 * @return true on success
 */
bool gpg_reset(void);

// ============================================================================
// Export
// ============================================================================

/**
 * Export public key in PEM format.
 *
 * @param buf Output buffer
 * @param size Buffer size
 * @param out_len Actual output length
 * @return true on success
 */
bool gpg_export_pubkey_pem(char *buf, size_t size, size_t *out_len);

/**
 * Export public key as raw bytes.
 *
 * @param pubkey Output buffer (64 bytes max)
 * @param pubkey_len Output: actual length
 * @param curve Output: curve type
 * @return true on success
 */
bool gpg_export_pubkey_raw(uint8_t *pubkey, size_t *pubkey_len, uint8_t *curve);

/**
 * Get GPG fingerprint.
 *
 * @param fp_out Output buffer (20 bytes)
 * @return true on success
 */
bool gpg_get_fingerprint(uint8_t *fp_out);

// ============================================================================
// Signing
// ============================================================================

/**
 * Sign a hash.
 *
 * @param hash Hash to sign (32 bytes for SHA-256)
 * @param hash_len Hash length
 * @param sig_out Signature output (64 bytes)
 * @param sig_len Output: actual signature length
 * @return true on success
 */
bool gpg_sign_hash(const uint8_t *hash, size_t hash_len,
                   uint8_t *sig_out, size_t *sig_len);

// ============================================================================
// Cross-Signing (Phase 3)
// ============================================================================

/**
 * Receive a public key from another badge.
 * Stores in R-Memory slots 140-149.
 *
 * @param pubkey Public key bytes
 * @param pubkey_len Public key length
 * @param curve Curve type
 * @param user_id User ID string
 * @param fingerprint GPG fingerprint (20 bytes)
 * @return true on success
 */
bool gpg_receive_pubkey(const uint8_t *pubkey, size_t pubkey_len, uint8_t curve,
                        const char *user_id, const uint8_t *fingerprint);

/**
 * Get count of received public keys.
 */
uint8_t gpg_received_count(void);

/**
 * Cross-sign a received public key.
 *
 * @param index Index of received key (0-9)
 * @return true on success
 */
bool gpg_cross_sign(uint8_t index);

#ifdef __cplusplus
}
#endif

#endif // FEATURE_GPG
