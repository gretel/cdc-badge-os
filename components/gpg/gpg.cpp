#include "feature_flags.h"

#if FEATURE_GPG

#include "gpg.h"
#include "tropic01.h"
#include "tropic01_cache.h"
#include "cdc_rtc.h"

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <esp_log.h>
#include <mbedtls/version.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha1.h>
#include <mbedtls/pk.h>
#include <mbedtls/pem.h>
#include <mbedtls/ecp.h>
#include <mbedtls/bignum.h>

// Helper to get unix timestamp
static uint32_t get_unix_time(void) {
    struct tm timeinfo;
    cdc_rtc_get_time(&timeinfo);
    return (uint32_t)mktime(&timeinfo);
}

static const char *TAG = "GPG";

// ============================================================================
// Static state
// ============================================================================

static bool s_initialized = false;
static gpg_metadata_t s_metadata;
static char s_pending_user_id[GPG_USER_ID_MAX] = {0};  // User ID before key generation

// ============================================================================
// Internal helpers
// ============================================================================

static bool load_metadata(void) {
    uint16_t read_size = 0;
    uint8_t buf[sizeof(gpg_metadata_t)];

    if (!tropic01_rmem_read(TR01_RMEM_SLOT_GPG, buf, sizeof(buf), &read_size)) {
        ESP_LOGD(TAG, "No GPG metadata in R-Memory");
        return false;
    }

    if (read_size < sizeof(gpg_metadata_t)) {
        ESP_LOGD(TAG, "GPG metadata too short: %u bytes", read_size);
        return false;
    }

    memcpy(&s_metadata, buf, sizeof(gpg_metadata_t));

    if (s_metadata.magic != GPG_METADATA_MAGIC) {
        ESP_LOGD(TAG, "Invalid GPG metadata magic: 0x%04X", s_metadata.magic);
        memset(&s_metadata, 0, sizeof(s_metadata));
        return false;
    }

    if (s_metadata.version != GPG_METADATA_VERSION) {
        ESP_LOGW(TAG, "GPG metadata version mismatch: %u vs %u",
                 s_metadata.version, GPG_METADATA_VERSION);
        memset(&s_metadata, 0, sizeof(s_metadata));
        return false;
    }

    ESP_LOGI(TAG, "GPG key loaded: %s (curve=%u)", s_metadata.user_id, s_metadata.curve);
    return true;
}

static bool save_metadata(void) {
    s_metadata.magic = GPG_METADATA_MAGIC;
    s_metadata.version = GPG_METADATA_VERSION;

    if (!tropic01_rmem_write(TR01_RMEM_SLOT_GPG, (const uint8_t *)&s_metadata,
                              sizeof(gpg_metadata_t))) {
        ESP_LOGE(TAG, "Failed to save GPG metadata");
        return false;
    }

    ESP_LOGI(TAG, "GPG metadata saved");
    return true;
}

// Calculate GPG fingerprint: SHA-1(0x99 || len || timestamp || algo || pubkey)
static bool calculate_fingerprint(const uint8_t *pubkey, size_t pubkey_len,
                                   uint8_t curve, uint32_t created_at,
                                   uint8_t *fp_out) {
    // GPG v4 fingerprint format
    // Algorithm IDs: 19=ECDSA, 22=EdDSA
    uint8_t algo = (curve == CDC_CURVE_ED25519) ? 22 : 19;

    // Build the packet to hash
    // 0x99 = public key packet tag for fingerprint calculation
    // Length includes: version(1) + timestamp(4) + algo(1) + OID + pubkey
    // Simplified: we hash timestamp + algo + raw pubkey

    mbedtls_sha1_context sha1_ctx;
    mbedtls_sha1_init(&sha1_ctx);
    mbedtls_sha1_starts(&sha1_ctx);

    // Version 4 packet header
    uint8_t header[6];
    header[0] = 0x99;  // Public key packet marker

    // Calculate total length (simplified)
    uint16_t payload_len = 1 + 4 + 1 + pubkey_len;  // version + ts + algo + key
    header[1] = (payload_len >> 8) & 0xFF;
    header[2] = payload_len & 0xFF;
    header[3] = 4;  // Version 4
    header[4] = (created_at >> 24) & 0xFF;
    header[5] = (created_at >> 16) & 0xFF;

    mbedtls_sha1_update(&sha1_ctx, header, 6);

    uint8_t ts_rest[2];
    ts_rest[0] = (created_at >> 8) & 0xFF;
    ts_rest[1] = created_at & 0xFF;
    mbedtls_sha1_update(&sha1_ctx, ts_rest, 2);

    mbedtls_sha1_update(&sha1_ctx, &algo, 1);
    mbedtls_sha1_update(&sha1_ctx, pubkey, pubkey_len);

    mbedtls_sha1_finish(&sha1_ctx, fp_out);
    mbedtls_sha1_free(&sha1_ctx);

    return true;
}

// ============================================================================
// Public API
// ============================================================================

bool gpg_init(void) {
    if (s_initialized) {
        return true;
    }

    memset(&s_metadata, 0, sizeof(s_metadata));

    // Try to load existing metadata
    if (load_metadata()) {
        s_initialized = true;
        ESP_LOGI(TAG, "GPG initialized with existing key");
    } else {
        s_initialized = true;
        ESP_LOGI(TAG, "GPG initialized (no key configured)");
    }

    return true;
}

bool gpg_is_initialized(void) {
    return s_metadata.magic == GPG_METADATA_MAGIC;
}

bool gpg_get_status(gpg_status_t *status) {
    if (!status) {
        return false;
    }

    memset(status, 0, sizeof(gpg_status_t));

    if (s_metadata.magic != GPG_METADATA_MAGIC) {
        status->initialized = false;
        return true;
    }

    status->initialized = true;
    status->curve = s_metadata.curve;
    strncpy(status->user_id, s_metadata.user_id, GPG_USER_ID_MAX - 1);
    memcpy(status->fingerprint, s_metadata.fingerprint, GPG_FINGERPRINT_LEN);
    status->created_at = s_metadata.created_at;
    status->sign_count = s_metadata.sign_count;

    return true;
}

bool gpg_set_pending_user_id(const char *user_id) {
    if (!user_id || strlen(user_id) == 0) {
        ESP_LOGE(TAG, "Empty user ID");
        return false;
    }

    strncpy(s_pending_user_id, user_id, GPG_USER_ID_MAX - 1);
    s_pending_user_id[GPG_USER_ID_MAX - 1] = '\0';

    ESP_LOGI(TAG, "Pending user ID set: %s", s_pending_user_id);
    return true;
}

bool gpg_has_pending_user_id(void) {
    return strlen(s_pending_user_id) > 0;
}

bool gpg_generate_key(uint8_t curve) {
    if (curve != CDC_CURVE_ED25519 && curve != CDC_CURVE_P256) {
        ESP_LOGE(TAG, "Invalid curve: %u", curve);
        return false;
    }

    // Check if user ID is set (either pending or we're overwriting existing key)
    if (strlen(s_pending_user_id) == 0 && s_metadata.magic != GPG_METADATA_MAGIC) {
        ESP_LOGE(TAG, "User ID must be set before key generation!");
        ESP_LOGE(TAG, "Use GPG_SET_UID first, then GPG_GENERATE");
        return false;
    }

    ESP_LOGI(TAG, "Generating GPG key (curve=%u)", curve);

    // Generate key in TROPIC01
    if (!tropic01_ecc_key_generate(TR01_ECC_SLOT_GPG, curve)) {
        ESP_LOGE(TAG, "Failed to generate ECC key");
        return false;
    }

    // Read back public key
    uint8_t pubkey[64];
    uint8_t read_curve, origin;
    if (!tropic01_ecc_key_read(TR01_ECC_SLOT_GPG, pubkey, sizeof(pubkey),
                                &read_curve, &origin)) {
        ESP_LOGE(TAG, "Failed to read generated public key");
        return false;
    }

    // Initialize metadata
    memset(&s_metadata, 0, sizeof(s_metadata));
    s_metadata.magic = GPG_METADATA_MAGIC;
    s_metadata.version = GPG_METADATA_VERSION;
    s_metadata.curve = curve;
    s_metadata.created_at = get_unix_time();
    s_metadata.sign_count = 0;

    // Copy user ID from pending (or keep existing if overwriting)
    if (strlen(s_pending_user_id) > 0) {
        strncpy(s_metadata.user_id, s_pending_user_id, GPG_USER_ID_MAX - 1);
        s_metadata.user_id[GPG_USER_ID_MAX - 1] = '\0';
        // Clear pending user ID after use
        memset(s_pending_user_id, 0, sizeof(s_pending_user_id));
    }

    // Store public key
    size_t pk_len = (curve == CDC_CURVE_ED25519) ? 32 : 64;
    memcpy(s_metadata.pubkey, pubkey, pk_len);
    s_metadata.pubkey_len = pk_len;

    // Calculate fingerprint
    calculate_fingerprint(pubkey, pk_len, curve, s_metadata.created_at,
                          s_metadata.fingerprint);

    // Save metadata
    if (!save_metadata()) {
        return false;
    }

    // Invalidate cache
    tropic01_cache_ecc_invalidate(TR01_ECC_SLOT_GPG);

    ESP_LOGI(TAG, "GPG key generated successfully");
    return true;
}

bool gpg_import_key_pem(const char *pem_data) {
    if (!pem_data || strlen(pem_data) == 0) {
        ESP_LOGE(TAG, "Empty PEM data");
        return false;
    }

    // Check if user ID is set
    if (strlen(s_pending_user_id) == 0 && s_metadata.magic != GPG_METADATA_MAGIC) {
        ESP_LOGE(TAG, "User ID must be set before key import!");
        ESP_LOGE(TAG, "Use GPG_SET_UID first, then GPG_IMPORT");
        return false;
    }

    ESP_LOGI(TAG, "Importing GPG key from PEM");

    // Parse PEM using mbedtls
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);

    int ret = mbedtls_pk_parse_key(&pk, (const unsigned char *)pem_data,
                                    strlen(pem_data) + 1, NULL, 0,
                                    NULL, NULL);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to parse PEM: -0x%04X", -ret);
        mbedtls_pk_free(&pk);
        return false;
    }

    // Determine curve type and validate compatibility
    uint8_t curve;
    uint8_t privkey[32];
    size_t privkey_len = 32;

    mbedtls_pk_type_t pk_type = mbedtls_pk_get_type(&pk);

    // Reject RSA keys explicitly
    if (pk_type == MBEDTLS_PK_RSA || pk_type == MBEDTLS_PK_RSA_ALT ||
        pk_type == MBEDTLS_PK_RSASSA_PSS) {
        ESP_LOGE(TAG, "RSA keys are NOT supported! Only Ed25519 and P-256.");
        mbedtls_pk_free(&pk);
        return false;
    }

    if (pk_type == MBEDTLS_PK_ECKEY || pk_type == MBEDTLS_PK_ECDSA) {
        // Get EC keypair - mbedtls_pk_ec takes the pk_context by value
        mbedtls_ecp_keypair *ec = mbedtls_pk_ec(pk);
        if (!ec) {
            ESP_LOGE(TAG, "Failed to get EC keypair");
            mbedtls_pk_free(&pk);
            return false;
        }

        // Validate curve - only P-256 (secp256r1) allowed
        mbedtls_ecp_group_id grp_id = mbedtls_ecp_keypair_get_group_id(ec);
        if (grp_id != MBEDTLS_ECP_DP_SECP256R1) {
            ESP_LOGE(TAG, "Unsupported EC curve! Only P-256 (secp256r1) is supported.");
            ESP_LOGE(TAG, "Detected curve ID: %d", grp_id);
            ESP_LOGE(TAG, "Other curves (secp384r1, secp521r1, brainpool, etc.) are NOT supported.");
            mbedtls_pk_free(&pk);
            return false;
        }

        curve = CDC_CURVE_P256;

        // Extract private key (d) using mbedtls_ecp_export
        mbedtls_mpi d_mpi;
        mbedtls_mpi_init(&d_mpi);
        ret = mbedtls_ecp_export(ec, NULL, &d_mpi, NULL);
        if (ret == 0) {
            ret = mbedtls_mpi_write_binary(&d_mpi, privkey, privkey_len);
        }
        mbedtls_mpi_free(&d_mpi);
        if (ret != 0) {
            ESP_LOGE(TAG, "Failed to extract private key: -0x%04X", -ret);
            mbedtls_pk_free(&pk);
            return false;
        }

        ESP_LOGI(TAG, "Detected P-256 (secp256r1) key - COMPATIBLE");
    } else {
        // Try to detect Ed25519 from raw key
        // Ed25519 keys in PKCS#8 have specific OID 1.3.101.112
        curve = CDC_CURVE_ED25519;

        // For Ed25519, we need to extract from the PKCS#8 structure
        // This is simplified - in production, proper ASN.1 parsing needed
        ESP_LOGW(TAG, "Ed25519 import via PEM not fully implemented");
        ESP_LOGI(TAG, "Note: Only Ed25519 and P-256 keys are supported by TROPIC01");
        mbedtls_pk_free(&pk);
        return false;
    }

    mbedtls_pk_free(&pk);

    // Write key to TROPIC01
    if (!tropic01_ecc_key_write(TR01_ECC_SLOT_GPG, privkey, privkey_len, curve)) {
        ESP_LOGE(TAG, "Failed to write key to TROPIC01");
        // Clear sensitive data
        memset(privkey, 0, sizeof(privkey));
        return false;
    }

    // Clear sensitive data
    memset(privkey, 0, sizeof(privkey));

    // Read back public key
    uint8_t pubkey[64];
    uint8_t read_curve, origin;
    if (!tropic01_ecc_key_read(TR01_ECC_SLOT_GPG, pubkey, sizeof(pubkey),
                                &read_curve, &origin)) {
        ESP_LOGE(TAG, "Failed to read imported public key");
        return false;
    }

    // Initialize metadata
    memset(&s_metadata, 0, sizeof(s_metadata));
    s_metadata.magic = GPG_METADATA_MAGIC;
    s_metadata.version = GPG_METADATA_VERSION;
    s_metadata.curve = curve;
    s_metadata.created_at = get_unix_time();
    s_metadata.sign_count = 0;

    // Copy user ID from pending
    if (strlen(s_pending_user_id) > 0) {
        strncpy(s_metadata.user_id, s_pending_user_id, GPG_USER_ID_MAX - 1);
        s_metadata.user_id[GPG_USER_ID_MAX - 1] = '\0';
        // Clear pending user ID after use
        memset(s_pending_user_id, 0, sizeof(s_pending_user_id));
    }

    // Store public key
    size_t pk_len = (curve == CDC_CURVE_ED25519) ? 32 : 64;
    memcpy(s_metadata.pubkey, pubkey, pk_len);
    s_metadata.pubkey_len = pk_len;

    // Calculate fingerprint
    calculate_fingerprint(pubkey, pk_len, curve, s_metadata.created_at,
                          s_metadata.fingerprint);

    // Save metadata
    if (!save_metadata()) {
        return false;
    }

    // Invalidate cache
    tropic01_cache_ecc_invalidate(TR01_ECC_SLOT_GPG);

    ESP_LOGI(TAG, "GPG key imported successfully");
    return true;
}

bool gpg_set_user_id(const char *user_id) {
    if (!user_id || strlen(user_id) == 0) {
        return false;
    }

    // If no key exists yet, set pending user ID for future generation/import
    if (s_metadata.magic != GPG_METADATA_MAGIC) {
        ESP_LOGI(TAG, "No key yet - setting pending user ID for generation/import");
        return gpg_set_pending_user_id(user_id);
    }

    // Key exists - update metadata (but note: this doesn't change the fingerprint!)
    ESP_LOGW(TAG, "Updating user ID on existing key (fingerprint unchanged)");
    strncpy(s_metadata.user_id, user_id, GPG_USER_ID_MAX - 1);
    s_metadata.user_id[GPG_USER_ID_MAX - 1] = '\0';

    return save_metadata();
}

bool gpg_reset(void) {
    ESP_LOGI(TAG, "Resetting GPG key");

    // Erase ECC slot
    if (!tropic01_ecc_key_erase(TR01_ECC_SLOT_GPG)) {
        ESP_LOGW(TAG, "Failed to erase ECC slot (may already be empty)");
    }

    // Erase metadata
    if (!tropic01_rmem_erase(TR01_RMEM_SLOT_GPG)) {
        ESP_LOGW(TAG, "Failed to erase R-Memory slot (may already be empty)");
    }

    // Clear local state
    memset(&s_metadata, 0, sizeof(s_metadata));

    // Invalidate cache
    tropic01_cache_ecc_invalidate(TR01_ECC_SLOT_GPG);

    ESP_LOGI(TAG, "GPG key reset complete");
    return true;
}

bool gpg_export_pubkey_pem(char *buf, size_t size, size_t *out_len) {
    if (!buf || size < 256 || !out_len) {
        return false;
    }

    if (s_metadata.magic != GPG_METADATA_MAGIC) {
        ESP_LOGE(TAG, "No GPG key configured");
        return false;
    }

    // Build SubjectPublicKeyInfo structure
    // For Ed25519: OID 1.3.101.112
    // For P-256: OID 1.2.840.10045.3.1.7

    size_t pk_len = s_metadata.pubkey_len;
    const uint8_t *pubkey = s_metadata.pubkey;

    if (s_metadata.curve == CDC_CURVE_ED25519) {
        // Ed25519 SPKI structure
        // SEQUENCE { SEQUENCE { OID 1.3.101.112 }, BIT STRING { pubkey } }
        static const uint8_t ed25519_prefix[] = {
            0x30, 0x2a,  // SEQUENCE, length 42
            0x30, 0x05,  // SEQUENCE, length 5
            0x06, 0x03, 0x2b, 0x65, 0x70,  // OID 1.3.101.112
            0x03, 0x21, 0x00  // BIT STRING, length 33, no unused bits
        };

        uint8_t der[sizeof(ed25519_prefix) + 32];
        memcpy(der, ed25519_prefix, sizeof(ed25519_prefix));
        memcpy(der + sizeof(ed25519_prefix), pubkey, 32);

        // Base64 encode
        size_t b64_len = 0;
        int ret = mbedtls_base64_encode(NULL, 0, &b64_len, der, sizeof(der));
        if (ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) {
            return false;
        }

        char b64[128];
        ret = mbedtls_base64_encode((unsigned char *)b64, sizeof(b64), &b64_len,
                                     der, sizeof(der));
        if (ret != 0) {
            return false;
        }

        // Format as PEM
        int written = snprintf(buf, size,
            "-----BEGIN PUBLIC KEY-----\n%s\n-----END PUBLIC KEY-----\n", b64);

        if (written < 0 || (size_t)written >= size) {
            return false;
        }

        *out_len = written;
    } else {
        // P-256 - more complex, need full SPKI encoding
        // For now, return raw hex as placeholder
        ESP_LOGW(TAG, "P-256 PEM export not fully implemented");

        int written = snprintf(buf, size, "P-256 public key (hex): ");
        for (size_t i = 0; i < pk_len && (size_t)written < size - 3; i++) {
            written += snprintf(buf + written, size - written, "%02x", pubkey[i]);
        }
        written += snprintf(buf + written, size - written, "\n");
        *out_len = written;
    }

    return true;
}

bool gpg_export_pubkey_raw(uint8_t *pubkey, size_t *pubkey_len, uint8_t *curve) {
    if (!pubkey || !pubkey_len || !curve) {
        return false;
    }

    if (s_metadata.magic != GPG_METADATA_MAGIC) {
        return false;
    }

    memcpy(pubkey, s_metadata.pubkey, s_metadata.pubkey_len);
    *pubkey_len = s_metadata.pubkey_len;
    *curve = s_metadata.curve;

    return true;
}

bool gpg_get_fingerprint(uint8_t *fp_out) {
    if (!fp_out) {
        return false;
    }

    if (s_metadata.magic != GPG_METADATA_MAGIC) {
        return false;
    }

    memcpy(fp_out, s_metadata.fingerprint, GPG_FINGERPRINT_LEN);
    return true;
}

bool gpg_sign_hash(const uint8_t *hash, size_t hash_len,
                   uint8_t *sig_out, size_t *sig_len) {
    if (!hash || !sig_out || !sig_len) {
        return false;
    }

    if (s_metadata.magic != GPG_METADATA_MAGIC) {
        ESP_LOGE(TAG, "No GPG key configured");
        return false;
    }

    bool success = false;

    if (s_metadata.curve == CDC_CURVE_ED25519) {
        // EdDSA signs the message directly, not a hash
        // For GPG compatibility, we sign the provided hash as the "message"
        success = tropic01_eddsa_sign(TR01_ECC_SLOT_GPG, hash, hash_len, sig_out);
        *sig_len = 64;
    } else {
        // ECDSA signs the hash
        if (hash_len != 32) {
            ESP_LOGE(TAG, "ECDSA requires 32-byte hash");
            return false;
        }
        success = tropic01_ecdsa_sign(TR01_ECC_SLOT_GPG, hash, hash_len, sig_out);
        *sig_len = 64;
    }

    if (success) {
        s_metadata.sign_count++;
        save_metadata();
        ESP_LOGI(TAG, "Signature created (count=%lu)", s_metadata.sign_count);
    }

    return success;
}

// ============================================================================
// Cross-Signing (Phase 3 - Stubs)
// ============================================================================

bool gpg_receive_pubkey(const uint8_t *pubkey, size_t pubkey_len, uint8_t curve,
                        const char *user_id, const uint8_t *fingerprint) {
    (void)pubkey;
    (void)pubkey_len;
    (void)curve;
    (void)user_id;
    (void)fingerprint;
    ESP_LOGW(TAG, "gpg_receive_pubkey: Not implemented (Phase 3)");
    return false;
}

uint8_t gpg_received_count(void) {
    return 0;  // Phase 3
}

bool gpg_cross_sign(uint8_t index) {
    (void)index;
    ESP_LOGW(TAG, "gpg_cross_sign: Not implemented (Phase 3)");
    return false;
}

#endif // FEATURE_GPG
