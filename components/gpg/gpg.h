#pragma once

#include "feature_flags.h"

#if FEATURE_GPG

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "tropic01.h"  // For CDC_CURVE_ED25519/P256

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Constants
// ============================================================================

#define GPG_USER_ID_MAX         64      // "Name <email@example.com>"
#define GPG_FINGERPRINT_LEN     20      // SHA-1 V4 fingerprint for GnuPG 2.x compatibility
#define GPG_FINGERPRINT_V5_LEN  32      // SHA-256 V5 fingerprint (RFC 9580)
#define GPG_PUBKEY_MAX_LEN      64      // 32 for Ed25519, 64 for P-256
#define GPG_SIGNATURE_MAX_LEN   64      // R+S for ECDSA/EdDSA

// Magic bytes for metadata validation
#define GPG_METADATA_MAGIC      0x4750  // "GP"
#define GPG_METADATA_VERSION    2       // Version 2: adds V5 fingerprint

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
    uint8_t fingerprint[GPG_FINGERPRINT_LEN]; // V4 fingerprint (SHA-1, 20 bytes)
    uint8_t pubkey[GPG_PUBKEY_MAX_LEN];     // Public key
    uint8_t pubkey_len;                     // Actual pubkey length
    uint32_t sign_count;                    // Signature counter
    uint8_t fingerprint_v5[GPG_FINGERPRINT_V5_LEN]; // V5 fingerprint (SHA-256, 32 bytes)
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
 * Get GPG V4 fingerprint (SHA-1, 20 bytes).
 * Used by GnuPG 2.x for key identification.
 *
 * @param fp_out Output buffer (20 bytes)
 * @return true on success
 */
bool gpg_get_fingerprint(uint8_t *fp_out);

/**
 * Get GPG V5 fingerprint (SHA-256, 32 bytes).
 * RFC 9580 compliant fingerprint for future GnuPG versions.
 *
 * @param fp_out Output buffer (32 bytes)
 * @return true on success
 */
bool gpg_get_fingerprint_v5(uint8_t *fp_out);

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

#define GPG_RECV_MAX_KEYS       16      // Max received keys in NVS
#define GPG_RECV_FLAG_VERIFIED  0x01    // Verified in person

// Received key info (for listing)
typedef struct {
    uint8_t curve;
    char user_id[GPG_USER_ID_MAX];
    uint8_t fingerprint[GPG_FINGERPRINT_LEN];
    uint32_t received_at;
    bool signed_by_me;                  // Has my cross-signature
    uint8_t flags;
} gpg_received_key_info_t;

/**
 * Receive a public key from another badge.
 * Stores in NVS (namespace: gpg_recv).
 *
 * @param pubkey Public key bytes
 * @param pubkey_len Public key length
 * @param curve Curve type (CDC_CURVE_ED25519 or CDC_CURVE_P256)
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
 * Get info about a received key.
 *
 * @param index Index (0 to gpg_received_count()-1)
 * @param info Output structure
 * @return true on success
 */
bool gpg_received_get_info(uint8_t index, gpg_received_key_info_t *info);

/**
 * Get full received key data by fingerprint.
 *
 * @param fingerprint 20-byte fingerprint
 * @param pubkey Output buffer for public key (64 bytes max)
 * @param pubkey_len Output: actual length
 * @param curve Output: curve type
 * @return true on success
 */
bool gpg_received_get_key(const uint8_t *fingerprint, uint8_t *pubkey,
                          size_t *pubkey_len, uint8_t *curve);

/**
 * Cross-sign a received public key.
 * Creates a signature over the key binding (fingerprint + user_id).
 *
 * @param index Index of received key
 * @return true on success
 */
bool gpg_cross_sign(uint8_t index);

/**
 * Delete a received key.
 *
 * @param index Index of key to delete
 * @return true on success
 */
bool gpg_received_delete(uint8_t index);

/**
 * Get cross-signature for a received key (raw signature).
 *
 * @param index Index of received key
 * @param sig_out Output buffer (64 bytes)
 * @param sig_len Output: actual signature length
 * @return true if key is signed, false otherwise
 */
bool gpg_received_get_signature(uint8_t index, uint8_t *sig_out, size_t *sig_len);

/**
 * Export a signed key as OpenPGP packet stream.
 * Creates RFC 4880 compliant output that can be imported into GnuPG.
 * Includes: Public Key Packet + User ID Packet + Signature Packet (Type 0x10)
 *
 * @param index Index of received key (must be signed)
 * @param buf Output buffer
 * @param buf_size Buffer size (recommended: 512+ bytes)
 * @param out_len Output: actual length
 * @return true on success
 */
bool gpg_export_signed_key(uint8_t index, uint8_t *buf, size_t buf_size, size_t *out_len);

/**
 * Export a signed key as ASCII-armored OpenPGP.
 * Same as gpg_export_signed_key but with Base64 + headers.
 *
 * @param index Index of received key (must be signed)
 * @param buf Output buffer (char*)
 * @param buf_size Buffer size (recommended: 1024+ bytes)
 * @param out_len Output: actual length
 * @return true on success
 */
bool gpg_export_signed_key_armored(uint8_t index, char *buf, size_t buf_size, size_t *out_len);

/**
 * Export own public key for BLE broadcast.
 * Returns data in format suitable for gpg_receive_pubkey().
 *
 * @param pubkey Output buffer (64 bytes)
 * @param pubkey_len Output: actual length
 * @param curve Output: curve type
 * @param user_id Output: user ID string (64 bytes)
 * @param fingerprint Output: fingerprint (20 bytes)
 * @return true on success
 */
bool gpg_export_for_broadcast(uint8_t *pubkey, size_t *pubkey_len, uint8_t *curve,
                              char *user_id, uint8_t *fingerprint);

#ifdef __cplusplus
}
#endif

#endif // FEATURE_GPG
