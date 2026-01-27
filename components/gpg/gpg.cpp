#include "feature_flags.h"

#if FEATURE_GPG

#include "gpg.h"
#include "tropic01.h"
#include "tropic01_cache.h"
#include "openpgp.h"
#include "cdc_rtc.h"

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <mbedtls/version.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>
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

// Forward declaration for V5 fingerprint (used in migration)
static bool calculate_fingerprint_v5(const uint8_t *pubkey, size_t pubkey_len,
                                      uint8_t curve, uint32_t created_at,
                                      uint8_t *fp_out);

// Old V1 metadata structure (for migration)
typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t version;
    uint8_t curve;
    char user_id[GPG_USER_ID_MAX];
    uint32_t created_at;
    uint8_t fingerprint[GPG_FINGERPRINT_LEN];
    uint8_t pubkey[GPG_PUBKEY_MAX_LEN];
    uint8_t pubkey_len;
    uint32_t sign_count;
    uint8_t reserved[32];
} gpg_metadata_v1_t;

static bool load_metadata(void) {
    uint16_t read_size = 0;
    uint8_t buf[sizeof(gpg_metadata_t)];

    if (!tropic01_rmem_read(TR01_RMEM_SLOT_GPG, buf, sizeof(buf), &read_size)) {
        ESP_LOGD(TAG, "No GPG metadata in R-Memory");
        return false;
    }

    // Check minimum size for V1 metadata
    if (read_size < sizeof(gpg_metadata_v1_t)) {
        ESP_LOGD(TAG, "GPG metadata too short: %u bytes", read_size);
        return false;
    }

    // Read header to check version
    gpg_metadata_v1_t *v1_meta = (gpg_metadata_v1_t *)buf;

    if (v1_meta->magic != GPG_METADATA_MAGIC) {
        ESP_LOGD(TAG, "Invalid GPG metadata magic: 0x%04X", v1_meta->magic);
        memset(&s_metadata, 0, sizeof(s_metadata));
        return false;
    }

    if (v1_meta->version == 1) {
        // Migrate V1 to V2: copy fields and calculate V5 fingerprint
        ESP_LOGI(TAG, "Migrating GPG metadata V1 -> V2");
        memset(&s_metadata, 0, sizeof(s_metadata));
        s_metadata.magic = GPG_METADATA_MAGIC;
        s_metadata.version = GPG_METADATA_VERSION;
        s_metadata.curve = v1_meta->curve;
        memcpy(s_metadata.user_id, v1_meta->user_id, GPG_USER_ID_MAX);
        s_metadata.created_at = v1_meta->created_at;
        memcpy(s_metadata.fingerprint, v1_meta->fingerprint, GPG_FINGERPRINT_LEN);
        memcpy(s_metadata.pubkey, v1_meta->pubkey, GPG_PUBKEY_MAX_LEN);
        s_metadata.pubkey_len = v1_meta->pubkey_len;
        s_metadata.sign_count = v1_meta->sign_count;

        // Calculate missing V5 fingerprint
        if (!calculate_fingerprint_v5(s_metadata.pubkey, s_metadata.pubkey_len,
                                       s_metadata.curve, s_metadata.created_at,
                                       s_metadata.fingerprint_v5)) {
            ESP_LOGW(TAG, "Failed to calculate V5 fingerprint during migration");
            memset(s_metadata.fingerprint_v5, 0, GPG_FINGERPRINT_V5_LEN);
        }

        // Save upgraded metadata
        if (!tropic01_rmem_write(TR01_RMEM_SLOT_GPG, (const uint8_t *)&s_metadata,
                                  sizeof(gpg_metadata_t))) {
            ESP_LOGW(TAG, "Failed to save migrated metadata");
        } else {
            ESP_LOGI(TAG, "GPG metadata migrated to V2");
        }
    } else if (v1_meta->version == GPG_METADATA_VERSION) {
        // Current version - direct copy
        memcpy(&s_metadata, buf, sizeof(gpg_metadata_t));
    } else {
        ESP_LOGW(TAG, "GPG metadata version mismatch: %u vs %u",
                 v1_meta->version, GPG_METADATA_VERSION);
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

// RFC 4880 V4 Fingerprint calculation
// V4 fingerprint = SHA-1(0x99 || 2-byte-length || public_key_packet_body)
// Public key packet body = version(1) || timestamp(4) || algo(1) || key_material
// For ECC: key_material = OID || public_point_MPI
static bool calculate_fingerprint(const uint8_t *pubkey, size_t pubkey_len,
                                   uint8_t curve, uint32_t created_at,
                                   uint8_t *fp_out) {
    // Algorithm IDs per RFC 6637/4880: 19=ECDSA, 22=EdDSA
    uint8_t algo = (curve == CDC_CURVE_ED25519) ? 22 : 19;

    // OIDs for curves (length-prefixed per RFC 6637 Section 9)
    // Format: length_byte || OID_bytes (ASN.1 BER encoding)
    //
    // Ed25519: 1.3.6.1.4.1.11591.15.1 (GnuPG/Libgcrypt EdDSA OID)
    //   09 = length (9 bytes follow)
    //   2B = 1.3 (1*40+3=43), 06=6, 01=1, 04=4, 01=1
    //   DA 47 = 11591 (BER: 90|0x80=0xDA, 71=0x47, decodes to 90*128+71=11591)
    //   0F = 15, 01 = 1
    // P-256: 1.2.840.10045.3.1.7 (NIST P-256, secp256r1)
    //   08 = length (8 bytes follow)
    //   2A = 1.2 (1*40+2=42), 86 48 = 840 (BER), CE 3D = 10045 (BER)
    //   03=3, 01=1, 07=7
    static const uint8_t oid_ed25519[] = {0x09, 0x2B, 0x06, 0x01, 0x04, 0x01, 0xDA, 0x47, 0x0F, 0x01};
    static const uint8_t oid_p256[] = {0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07};

    const uint8_t *oid;
    size_t oid_len;
    if (curve == CDC_CURVE_ED25519) {
        oid = oid_ed25519;
        oid_len = sizeof(oid_ed25519);
    } else {
        oid = oid_p256;
        oid_len = sizeof(oid_p256);
    }

    // Build MPI for public key per RFC 4880 Section 3.2
    // MPI format: 2-byte bit count (big endian) || raw bytes (no leading zeros)
    // Bit count = number of bits in the MPI, excluding leading zero bits
    uint8_t mpi[67];  // Max: 2 + 65 for P-256
    size_t mpi_len;

    if (curve == CDC_CURVE_ED25519) {
        // Ed25519: 32-byte point = 256 bits (or 255 if MSB is 0)
        // RFC 6637/4880: EdDSA native format, count actual bits
        uint16_t bits = 256;
        // Check for leading zeros to get actual bit count
        if ((pubkey[0] & 0x80) == 0) bits = 255;
        mpi[0] = (bits >> 8) & 0xFF;
        mpi[1] = bits & 0xFF;
        memcpy(mpi + 2, pubkey, 32);
        mpi_len = 2 + 32;
    } else {
        // P-256: 04 || X || Y = 65 bytes = 520 bits (0x04 marker + 64 bytes)
        // The 0x04 marker indicates uncompressed point
        uint16_t bits = 520;  // 65 bytes * 8 = 520 bits
        mpi[0] = (bits >> 8) & 0xFF;
        mpi[1] = bits & 0xFF;
        mpi[2] = 0x04;  // Uncompressed point marker
        memcpy(mpi + 3, pubkey, 64);  // X || Y
        mpi_len = 2 + 1 + 64;
    }

    // Total public key packet body length
    // version(1) + timestamp(4) + algo(1) + oid + mpi
    uint16_t payload_len = 1 + 4 + 1 + oid_len + mpi_len;

    // Start SHA-1 hash
    mbedtls_sha1_context sha1_ctx;
    mbedtls_sha1_init(&sha1_ctx);
    mbedtls_sha1_starts(&sha1_ctx);

    // Hash packet header: 0x99 || length (2 bytes, big endian)
    uint8_t header[3];
    header[0] = 0x99;  // Public key packet marker for fingerprint
    header[1] = (payload_len >> 8) & 0xFF;
    header[2] = payload_len & 0xFF;
    mbedtls_sha1_update(&sha1_ctx, header, 3);

    // Hash version (4)
    uint8_t version = 4;
    mbedtls_sha1_update(&sha1_ctx, &version, 1);

    // Hash timestamp (4 bytes, big endian)
    uint8_t ts[4];
    ts[0] = (created_at >> 24) & 0xFF;
    ts[1] = (created_at >> 16) & 0xFF;
    ts[2] = (created_at >> 8) & 0xFF;
    ts[3] = created_at & 0xFF;
    mbedtls_sha1_update(&sha1_ctx, ts, 4);

    // Hash algorithm
    mbedtls_sha1_update(&sha1_ctx, &algo, 1);

    // Hash OID
    mbedtls_sha1_update(&sha1_ctx, oid, oid_len);

    // Hash MPI
    mbedtls_sha1_update(&sha1_ctx, mpi, mpi_len);

    // Finalize
    mbedtls_sha1_finish(&sha1_ctx, fp_out);
    mbedtls_sha1_free(&sha1_ctx);

    return true;
}

// RFC 9580 V5 Fingerprint calculation (SHA-256, 32 bytes)
// V5 fingerprint = SHA-256(0x9A || 4-byte-length || V5_public_key_packet_body)
// V5 packet body = version(1) || timestamp(4) || algo(1) || key_material_length(4) || key_material
// For ECC: key_material = OID || public_point_MPI
static bool calculate_fingerprint_v5(const uint8_t *pubkey, size_t pubkey_len,
                                      uint8_t curve, uint32_t created_at,
                                      uint8_t *fp_out) {
    uint8_t algo = (curve == CDC_CURVE_ED25519) ? 22 : 19;

    // OIDs (same as V4)
    static const uint8_t oid_ed25519[] = {0x09, 0x2B, 0x06, 0x01, 0x04, 0x01, 0xDA, 0x47, 0x0F, 0x01};
    static const uint8_t oid_p256[] = {0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07};

    const uint8_t *oid;
    size_t oid_len;
    if (curve == CDC_CURVE_ED25519) {
        oid = oid_ed25519;
        oid_len = sizeof(oid_ed25519);
    } else {
        oid = oid_p256;
        oid_len = sizeof(oid_p256);
    }

    // Build MPI (same as V4)
    uint8_t mpi[67];
    size_t mpi_len;

    if (curve == CDC_CURVE_ED25519) {
        uint16_t bits = 256;
        if ((pubkey[0] & 0x80) == 0) bits = 255;
        mpi[0] = (bits >> 8) & 0xFF;
        mpi[1] = bits & 0xFF;
        memcpy(mpi + 2, pubkey, 32);
        mpi_len = 2 + 32;
    } else {
        uint16_t bits = 520;
        mpi[0] = (bits >> 8) & 0xFF;
        mpi[1] = bits & 0xFF;
        mpi[2] = 0x04;
        memcpy(mpi + 3, pubkey, 64);
        mpi_len = 2 + 1 + 64;
    }

    // V5: key_material_length = OID + MPI length (4 bytes)
    uint32_t key_material_len = oid_len + mpi_len;

    // V5 packet body length: version(1) + timestamp(4) + algo(1) + key_material_length(4) + key_material
    uint32_t payload_len = 1 + 4 + 1 + 4 + key_material_len;

    // Start SHA-256 hash
    mbedtls_sha256_context sha256_ctx;
    mbedtls_sha256_init(&sha256_ctx);
    mbedtls_sha256_starts(&sha256_ctx, 0);  // 0 = SHA-256

    // V5 header: 0x9A || 4-byte length (big endian)
    uint8_t header[5];
    header[0] = 0x9A;
    header[1] = (payload_len >> 24) & 0xFF;
    header[2] = (payload_len >> 16) & 0xFF;
    header[3] = (payload_len >> 8) & 0xFF;
    header[4] = payload_len & 0xFF;
    mbedtls_sha256_update(&sha256_ctx, header, 5);

    // Version 5
    uint8_t version = 5;
    mbedtls_sha256_update(&sha256_ctx, &version, 1);

    // Timestamp (4 bytes, big endian)
    uint8_t ts[4];
    ts[0] = (created_at >> 24) & 0xFF;
    ts[1] = (created_at >> 16) & 0xFF;
    ts[2] = (created_at >> 8) & 0xFF;
    ts[3] = created_at & 0xFF;
    mbedtls_sha256_update(&sha256_ctx, ts, 4);

    // Algorithm
    mbedtls_sha256_update(&sha256_ctx, &algo, 1);

    // Key material length (4 bytes, big endian)
    uint8_t km_len[4];
    km_len[0] = (key_material_len >> 24) & 0xFF;
    km_len[1] = (key_material_len >> 16) & 0xFF;
    km_len[2] = (key_material_len >> 8) & 0xFF;
    km_len[3] = key_material_len & 0xFF;
    mbedtls_sha256_update(&sha256_ctx, km_len, 4);

    // OID
    mbedtls_sha256_update(&sha256_ctx, oid, oid_len);

    // MPI
    mbedtls_sha256_update(&sha256_ctx, mpi, mpi_len);

    // Finalize
    mbedtls_sha256_finish(&sha256_ctx, fp_out);
    mbedtls_sha256_free(&sha256_ctx);

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

// Validate User ID format per RFC 4880 conventions
// Expected format: "Real Name <email@example.com>" or "Real Name (Comment) <email>"
// - Must be valid UTF-8 (we accept ASCII subset for simplicity)
// - Recommended to have email in angle brackets for GnuPG compatibility
// - No control characters (0x00-0x1F except space)
static bool validate_user_id(const char *user_id) {
    if (!user_id || strlen(user_id) == 0) {
        return false;
    }

    size_t len = strlen(user_id);
    if (len >= GPG_USER_ID_MAX) {
        ESP_LOGW(TAG, "User ID too long (%zu >= %d)", len, GPG_USER_ID_MAX);
        return false;
    }

    // Basic validation: no control characters
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)user_id[i];
        if (c < 0x20 && c != '\0') {  // Control char (except space)
            ESP_LOGW(TAG, "User ID contains control character at pos %zu", i);
            return false;
        }
        // UTF-8 continuation bytes (0x80-0xBF) without proper start byte is invalid
        // But we don't do full UTF-8 validation - trust user input for now
    }

    // Warn if no email format detected (but still accept)
    if (!strchr(user_id, '<') || !strchr(user_id, '>')) {
        ESP_LOGW(TAG, "User ID has no <email> - may cause GnuPG compatibility issues");
    }

    return true;
}

bool gpg_set_pending_user_id(const char *user_id) {
    if (!validate_user_id(user_id)) {
        ESP_LOGE(TAG, "Invalid user ID");
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

// Helper to generate a single key and calculate its fingerprint
static bool generate_single_key(uint8_t slot, uint8_t curve, uint32_t created_at,
                                 uint8_t *fingerprint_out) {
    ESP_LOGI(TAG, "Generating key in slot %d (curve=%d)", slot, curve);

    // Erase existing key first (TROPIC01 requires empty slot for generation)
    tropic01_ecc_key_erase(slot);

    if (!tropic01_ecc_key_generate(slot, curve)) {
        ESP_LOGE(TAG, "Failed to generate key in slot %d", slot);
        return false;
    }

    // Read back public key for fingerprint calculation
    uint8_t pubkey[64];
    uint8_t read_curve, origin;
    if (!tropic01_ecc_key_read(slot, pubkey, sizeof(pubkey), &read_curve, &origin)) {
        ESP_LOGE(TAG, "Failed to read key from slot %d", slot);
        return false;
    }

    // Calculate fingerprint
    size_t pk_len = (curve == CDC_CURVE_ED25519) ? 32 : 64;
    if (!calculate_fingerprint(pubkey, pk_len, curve, created_at, fingerprint_out)) {
        ESP_LOGW(TAG, "Failed to calculate fingerprint for slot %d", slot);
        memset(fingerprint_out, 0, GPG_FINGERPRINT_LEN);
    }

    // Invalidate cache for this slot
    tropic01_cache_ecc_invalidate(slot);

    return true;
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

    ESP_LOGI(TAG, "Generating GPG key set (curve=%u)", curve);

    uint32_t created_at = get_unix_time();

    // Determine curves for each key type:
    // - SIG and AUT: use requested curve (Ed25519 or P-256)
    // - DEC: must use P-256 for ECDH (TROPIC01 has no X25519)
    uint8_t curve_sig = curve;
    uint8_t curve_dec = CDC_CURVE_P256;  // Always P-256 for ECDH
    uint8_t curve_aut = curve;

    // Generate all three keys
    uint8_t fp_sig[GPG_FINGERPRINT_LEN] = {0};
    uint8_t fp_dec[GPG_FINGERPRINT_LEN] = {0};
    uint8_t fp_aut[GPG_FINGERPRINT_LEN] = {0};

    // SIG key (slot 27)
    if (!generate_single_key(TR01_ECC_SLOT_GPG_SIG, curve_sig, created_at, fp_sig)) {
        return false;
    }

    // DEC key (slot 28) - P-256 for ECDH
    if (!generate_single_key(TR01_ECC_SLOT_GPG_DEC, curve_dec, created_at, fp_dec)) {
        return false;
    }

    // AUT key (slot 29)
    if (!generate_single_key(TR01_ECC_SLOT_GPG_AUT, curve_aut, created_at, fp_aut)) {
        return false;
    }

    // Initialize metadata (stores SIG key info as primary)
    memset(&s_metadata, 0, sizeof(s_metadata));
    s_metadata.magic = GPG_METADATA_MAGIC;
    s_metadata.version = GPG_METADATA_VERSION;
    s_metadata.curve = curve;
    s_metadata.created_at = created_at;
    s_metadata.sign_count = 0;

    // Copy user ID from pending
    if (strlen(s_pending_user_id) > 0) {
        strncpy(s_metadata.user_id, s_pending_user_id, GPG_USER_ID_MAX - 1);
        s_metadata.user_id[GPG_USER_ID_MAX - 1] = '\0';
        memset(s_pending_user_id, 0, sizeof(s_pending_user_id));
    }

    // Store SIG public key in metadata
    uint8_t pubkey[64];
    uint8_t read_curve, origin;
    if (tropic01_ecc_key_read(TR01_ECC_SLOT_GPG_SIG, pubkey, sizeof(pubkey),
                               &read_curve, &origin)) {
        size_t pk_len = (curve_sig == CDC_CURVE_ED25519) ? 32 : 64;
        memcpy(s_metadata.pubkey, pubkey, pk_len);
        s_metadata.pubkey_len = pk_len;
    }

    // Store SIG fingerprint in metadata
    memcpy(s_metadata.fingerprint, fp_sig, GPG_FINGERPRINT_LEN);

    // Calculate V5 fingerprint for SIG key
    size_t pk_len = (curve_sig == CDC_CURVE_ED25519) ? 32 : 64;
    if (!calculate_fingerprint_v5(s_metadata.pubkey, pk_len, curve_sig, created_at,
                                   s_metadata.fingerprint_v5)) {
        memset(s_metadata.fingerprint_v5, 0, GPG_FINGERPRINT_V5_LEN);
    }

    // Log fingerprints
    char fp_hex[65];
    for (int i = 0; i < 20; i++) sprintf(fp_hex + i*2, "%02X", fp_sig[i]);
    ESP_LOGI(TAG, "SIG Fingerprint: %s", fp_hex);
    for (int i = 0; i < 20; i++) sprintf(fp_hex + i*2, "%02X", fp_dec[i]);
    ESP_LOGI(TAG, "DEC Fingerprint: %s", fp_hex);
    for (int i = 0; i < 20; i++) sprintf(fp_hex + i*2, "%02X", fp_aut[i]);
    ESP_LOGI(TAG, "AUT Fingerprint: %s", fp_hex);

    // Save metadata
    if (!save_metadata()) {
        return false;
    }

    // Update OpenPGP fingerprints for all three keys
    openpgp_set_key_fingerprint(KEY_SIG, fp_sig, created_at);
    openpgp_set_key_fingerprint(KEY_DEC, fp_dec, created_at);
    openpgp_set_key_fingerprint(KEY_AUT, fp_aut, created_at);

    ESP_LOGI(TAG, "GPG key set generated (SIG/AUT: %s, DEC: P-256)",
             curve == CDC_CURVE_ED25519 ? "Ed25519" : "P-256");
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
    } else if (pk_type == MBEDTLS_PK_OPAQUE) {
        // mbedTLS 3.x treats Ed25519 as OPAQUE - manual PKCS#8 parsing required
        // RFC 8410 - Algorithm Identifiers for Ed25519, Ed448, X25519, X448
        //
        // PKCS#8 PrivateKeyInfo structure (RFC 5958):
        // SEQUENCE {                          # 30 <len>
        //   INTEGER 0 (version)               # 02 01 00
        //   SEQUENCE {                        # 30 05
        //     OID 1.3.101.112 (Ed25519)       # 06 03 2B 65 70
        //   }
        //   OCTET STRING {                    # 04 22 (34 bytes = 04 20 + 32 key bytes)
        //     OCTET STRING { 32-byte key }    # 04 20 <32 bytes>
        //   }
        //   [optional attributes]
        // }
        //
        // Common variations we handle:
        // 1. Standard: 04 22 04 20 <32 bytes> - nested OCTET STRING
        // 2. Direct:   04 20 <32 bytes>       - some tools omit outer wrapper
        static const uint8_t ed25519_oid[] = { 0x06, 0x03, 0x2B, 0x65, 0x70 };

        // Search for Ed25519 OID in the raw PEM data
        const char *pem_start = strstr(pem_data, "-----BEGIN PRIVATE KEY-----");
        if (!pem_start) {
            ESP_LOGE(TAG, "Invalid PEM format - no BEGIN marker");
            ESP_LOGE(TAG, "Expected: -----BEGIN PRIVATE KEY-----");
            mbedtls_pk_free(&pk);
            return false;
        }

        // Decode Base64 to get raw DER
        pem_start = pem_start + strlen("-----BEGIN PRIVATE KEY-----");
        const char *pem_end = strstr(pem_start, "-----END PRIVATE KEY-----");
        if (!pem_end) {
            ESP_LOGE(TAG, "Invalid PEM format - no END marker");
            mbedtls_pk_free(&pk);
            return false;
        }

        // Remove whitespace and decode - robust against various line endings
        char b64_clean[512];
        size_t clean_len = 0;
        for (const char *p = pem_start; p < pem_end && clean_len < sizeof(b64_clean)-1; p++) {
            if (*p != '\n' && *p != '\r' && *p != ' ' && *p != '\t') {
                b64_clean[clean_len++] = *p;
            }
        }
        b64_clean[clean_len] = '\0';

        uint8_t der[256];
        size_t der_len = 0;
        ret = mbedtls_base64_decode(der, sizeof(der), &der_len,
                                     (const unsigned char *)b64_clean, clean_len);
        if (ret != 0) {
            ESP_LOGE(TAG, "Base64 decode failed: -0x%04X", -ret);
            mbedtls_pk_free(&pk);
            return false;
        }

        // Validate minimum DER structure
        if (der_len < 48) {  // Minimum Ed25519 PKCS#8 is ~48 bytes
            ESP_LOGE(TAG, "DER too short for Ed25519 PKCS#8: %zu bytes", der_len);
            mbedtls_pk_free(&pk);
            return false;
        }

        // Validate outer SEQUENCE tag
        if (der[0] != 0x30) {
            ESP_LOGE(TAG, "Invalid PKCS#8: expected SEQUENCE (0x30), got 0x%02X", der[0]);
            mbedtls_pk_free(&pk);
            return false;
        }

        // Search for Ed25519 OID
        bool found_ed25519 = false;
        size_t oid_pos = 0;
        for (size_t i = 0; i + sizeof(ed25519_oid) <= der_len; i++) {
            if (memcmp(der + i, ed25519_oid, sizeof(ed25519_oid)) == 0) {
                found_ed25519 = true;
                oid_pos = i;
                break;
            }
        }

        if (!found_ed25519) {
            ESP_LOGE(TAG, "Not an Ed25519 key (OID 1.3.101.112 not found)");
            ESP_LOGE(TAG, "Only Ed25519 and P-256 keys are supported");
            mbedtls_pk_free(&pk);
            return false;
        }

        // Find the private key after the OID
        // Pattern 1 (standard): 04 22 04 20 <32 bytes> - nested OCTET STRING
        // Pattern 2 (direct):   04 20 <32 bytes>       - direct OCTET STRING
        bool found_key = false;
        for (size_t i = oid_pos; i + 36 <= der_len; i++) {
            // Standard nested pattern: 04 22 04 20 <32 bytes>
            if (der[i] == 0x04 && der[i+1] == 0x22 &&
                der[i+2] == 0x04 && der[i+3] == 0x20) {
                memcpy(privkey, der + i + 4, 32);
                found_key = true;
                ESP_LOGI(TAG, "Ed25519 key extracted (nested OCTET STRING)");
                break;
            }
            // Direct pattern: 04 20 <32 bytes> (after SEQUENCE end)
            if (der[i] == 0x04 && der[i+1] == 0x20 && !found_key) {
                // Verify we're past the algorithm SEQUENCE
                if (i > oid_pos + sizeof(ed25519_oid)) {
                    memcpy(privkey, der + i + 2, 32);
                    found_key = true;
                    ESP_LOGI(TAG, "Ed25519 key extracted (direct OCTET STRING)");
                    break;
                }
            }
        }

        if (!found_key) {
            ESP_LOGE(TAG, "Could not extract Ed25519 private key from PKCS#8");
            ESP_LOGE(TAG, "DER structure may be non-standard");
            mbedtls_pk_free(&pk);
            return false;
        }

        curve = CDC_CURVE_ED25519;
        ESP_LOGI(TAG, "Detected Ed25519 key - COMPATIBLE");
    } else {
        ESP_LOGE(TAG, "Unknown key type: %d. Only Ed25519 and P-256 supported.", pk_type);
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

    // Calculate V4 fingerprint (SHA-1, for GnuPG 2.x compatibility)
    if (!calculate_fingerprint(pubkey, pk_len, curve, s_metadata.created_at, s_metadata.fingerprint)) {
        ESP_LOGW(TAG, "Failed to calculate V4 fingerprint");
        memset(s_metadata.fingerprint, 0, GPG_FINGERPRINT_LEN);
    }

    // Calculate V5 fingerprint (SHA-256, RFC 9580)
    if (!calculate_fingerprint_v5(pubkey, pk_len, curve, s_metadata.created_at, s_metadata.fingerprint_v5)) {
        ESP_LOGW(TAG, "Failed to calculate V5 fingerprint");
        memset(s_metadata.fingerprint_v5, 0, GPG_FINGERPRINT_V5_LEN);
    }

    // Log both fingerprints
    char fp_hex[65];
    for (int i = 0; i < 20; i++) {
        sprintf(fp_hex + i*2, "%02X", s_metadata.fingerprint[i]);
    }
    ESP_LOGI(TAG, "V4 Fingerprint: %s", fp_hex);
    for (int i = 0; i < 32; i++) {
        sprintf(fp_hex + i*2, "%02X", s_metadata.fingerprint_v5[i]);
    }
    ESP_LOGI(TAG, "V5 Fingerprint: %s", fp_hex);

    // Save metadata
    if (!save_metadata()) {
        return false;
    }

    // Invalidate cache
    tropic01_cache_ecc_invalidate(TR01_ECC_SLOT_GPG);

    ESP_LOGI(TAG, "GPG key imported");
    return true;
}

bool gpg_set_user_id(const char *user_id) {
    if (!validate_user_id(user_id)) {
        ESP_LOGE(TAG, "Invalid user ID");
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
    ESP_LOGI(TAG, "Resetting GPG keys (all 3 slots)");

    // Erase all three ECC slots (SIG, DEC, AUT)
    tropic01_ecc_key_erase(TR01_ECC_SLOT_GPG_SIG);
    tropic01_ecc_key_erase(TR01_ECC_SLOT_GPG_DEC);
    tropic01_ecc_key_erase(TR01_ECC_SLOT_GPG_AUT);

    // Erase metadata
    if (!tropic01_rmem_erase(TR01_RMEM_SLOT_GPG)) {
        ESP_LOGW(TAG, "Failed to erase R-Memory slot (may already be empty)");
    }

    // Clear local state
    memset(&s_metadata, 0, sizeof(s_metadata));

    // Invalidate cache for all slots
    tropic01_cache_ecc_invalidate(TR01_ECC_SLOT_GPG_SIG);
    tropic01_cache_ecc_invalidate(TR01_ECC_SLOT_GPG_DEC);
    tropic01_cache_ecc_invalidate(TR01_ECC_SLOT_GPG_AUT);

    // Clear OpenPGP fingerprints
    uint8_t zero_fp[20] = {0};
    openpgp_set_key_fingerprint(KEY_SIG, zero_fp, 0);
    openpgp_set_key_fingerprint(KEY_DEC, zero_fp, 0);
    openpgp_set_key_fingerprint(KEY_AUT, zero_fp, 0);

    ESP_LOGI(TAG, "GPG key reset complete (all 3 keys deleted)");
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

    const uint8_t *pubkey = s_metadata.pubkey;

    if (s_metadata.curve == CDC_CURVE_ED25519) {
        // Ed25519 SPKI structure (RFC 8410)
        // SEQUENCE {
        //   SEQUENCE { OID 1.3.101.112 }  = 7 bytes (30 05 06 03 2b 65 70)
        //   BIT STRING { 0x00 || pubkey } = 35 bytes (03 21 00 + 32 bytes)
        // }
        // Total inner: 7 + 35 = 42 bytes, outer SEQUENCE: 30 2a
        static const uint8_t ed25519_prefix[] = {
            0x30, 0x2a,  // SEQUENCE, length 42 (7 + 35)
            0x30, 0x05,  // SEQUENCE, length 5 (OID header + OID)
            0x06, 0x03, 0x2b, 0x65, 0x70,  // OID 1.3.101.112 (Ed25519)
            0x03, 0x21, 0x00  // BIT STRING, length 33 (1 unused-bits byte + 32 key bytes), 0 unused bits
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
        // P-256 SPKI format (RFC 5480):
        // SEQUENCE {                                    = 2 + 89 = 91 bytes total
        //   SEQUENCE {                                  = 2 + 19 = 21 bytes
        //     OID 1.2.840.10045.2.1 (ecPublicKey)       = 9 bytes (06 07 + 7 bytes)
        //     OID 1.2.840.10045.3.1.7 (secp256r1)       = 10 bytes (06 08 + 8 bytes)
        //   }
        //   BIT STRING { 0x00 || 04 || X || Y }         = 2 + 66 = 68 bytes (03 42 00 + 65 bytes)
        // }
        // Inner: 21 + 68 = 89 bytes, outer SEQUENCE: 30 59
        static const uint8_t p256_prefix[] = {
            0x30, 0x59,  // SEQUENCE, length 89 (21 + 68)
            0x30, 0x13,  // SEQUENCE, length 19 (9 + 10)
            0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01,  // OID ecPublicKey (9 bytes)
            0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07,  // OID secp256r1 (10 bytes)
            0x03, 0x42, 0x00  // BIT STRING, length 66 (1 unused-bits byte + 65 point bytes), 0 unused
        };

        // P-256 public key is 64 bytes (X || Y), we add 0x04 uncompressed marker
        uint8_t der[sizeof(p256_prefix) + 65];
        memcpy(der, p256_prefix, sizeof(p256_prefix));
        der[sizeof(p256_prefix)] = 0x04;  // Uncompressed point marker
        memcpy(der + sizeof(p256_prefix) + 1, pubkey, 64);

        // Base64 encode
        size_t b64_len = 0;
        int ret = mbedtls_base64_encode(NULL, 0, &b64_len, der, sizeof(der));
        if (ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) {
            return false;
        }

        char b64[256];
        ret = mbedtls_base64_encode((unsigned char *)b64, sizeof(b64), &b64_len,
                                     der, sizeof(der));
        if (ret != 0) {
            ESP_LOGE(TAG, "Base64 encode failed: -0x%04X", -ret);
            return false;
        }

        // Format as PEM
        int written = snprintf(buf, size,
            "-----BEGIN PUBLIC KEY-----\n%s\n-----END PUBLIC KEY-----\n", b64);

        if (written < 0 || (size_t)written >= size) {
            return false;
        }

        *out_len = written;
        ESP_LOGI(TAG, "P-256 public key exported as PEM (%d bytes)", written);
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

bool gpg_get_fingerprint_v5(uint8_t *fp_out) {
    if (!fp_out) {
        return false;
    }

    if (s_metadata.magic != GPG_METADATA_MAGIC) {
        return false;
    }

    memcpy(fp_out, s_metadata.fingerprint_v5, GPG_FINGERPRINT_V5_LEN);
    return true;
}

// Sign a hash with the GPG key.
//
// IMPORTANT: EdDSA vs ECDSA Signature Semantics (RFC 8032, RFC 6979)
//
// EdDSA (Ed25519):
//   - Per RFC 8032, EdDSA normally signs the MESSAGE directly, not a pre-hash.
//   - The hash is computed internally: SHA-512(prefix || message)
//   - OpenPGP (RFC 4880bis/6637) handles this by using the EdDSA algorithm
//     with SHA-256 as the "hash algorithm" in the signature packet.
//   - For OpenPGP V4 signatures, we pass the 32-byte SHA-256 hash as the
//     "message" to EdDSA. This is conformant with how GnuPG handles EdDSA.
//   - Output: 64 bytes (R || S), each 32 bytes in little-endian per RFC 8032.
//
// ECDSA (P-256):
//   - Per RFC 6979, ECDSA signs a hash of the message.
//   - We pass the 32-byte SHA-256 hash directly to the ECDSA sign function.
//   - Output: 64 bytes (R || S), each 32 bytes in big-endian.
//
// Both produce 64-byte signatures formatted as MPI pairs in OpenPGP packets.
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
        // EdDSA: pass hash as "message" - TROPIC01 handles internal hashing
        // This matches OpenPGP EdDSA behavior where the signature packet's
        // hash (SHA-256) becomes the input to Ed25519
        success = tropic01_eddsa_sign(TR01_ECC_SLOT_GPG, hash, hash_len, sig_out);
        *sig_len = 64;  // R (32 bytes) || S (32 bytes)
    } else {
        // ECDSA: sign the hash directly
        if (hash_len != 32) {
            ESP_LOGE(TAG, "ECDSA requires 32-byte SHA-256 hash");
            return false;
        }
        success = tropic01_ecdsa_sign(TR01_ECC_SLOT_GPG, hash, hash_len, sig_out);
        *sig_len = 64;  // R (32 bytes) || S (32 bytes)
    }

    if (success) {
        s_metadata.sign_count++;
        if (!save_metadata()) {
            ESP_LOGW(TAG, "Failed to save sign count");
        }
        ESP_LOGI(TAG, "Signature created (count=%lu)", s_metadata.sign_count);
    }

    return success;
}

// ============================================================================
// Cross-Signing (Phase 3)
// ============================================================================

// NVS namespace for received keys
#define NVS_GPG_RECV "gpg_recv"

// Internal structure for NVS storage
typedef struct __attribute__((packed)) {
    uint8_t curve;
    char user_id[GPG_USER_ID_MAX];
    uint8_t pubkey[GPG_PUBKEY_MAX_LEN];
    uint8_t pubkey_len;
    uint8_t fingerprint[GPG_FINGERPRINT_LEN];
    uint32_t received_at;
    uint8_t signature[GPG_SIGNATURE_MAX_LEN];  // My cross-signature
    uint8_t sig_len;
    uint8_t flags;
} gpg_received_key_nvs_t;

// Cache of fingerprints for quick lookup (loaded on first access)
static uint8_t s_recv_fingerprints[GPG_RECV_MAX_KEYS][GPG_FINGERPRINT_LEN];
static uint8_t s_recv_count = 0;
static bool s_recv_cache_valid = false;

// Build NVS key from fingerprint
static void build_nvs_key(const uint8_t *fingerprint, char *key_out) {
    // Use first 8 bytes of fingerprint as hex = 16 chars
    sprintf(key_out, "pk_%02x%02x%02x%02x%02x%02x%02x%02x",
            fingerprint[0], fingerprint[1], fingerprint[2], fingerprint[3],
            fingerprint[4], fingerprint[5], fingerprint[6], fingerprint[7]);
}

// Refresh cache of received keys
static void refresh_recv_cache(void) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_GPG_RECV, NVS_READONLY, &nvs) != ESP_OK) {
        s_recv_count = 0;
        s_recv_cache_valid = true;
        return;
    }

    s_recv_count = 0;

    // Iterate through NVS entries
    nvs_iterator_t it = NULL;
    esp_err_t err = nvs_entry_find_in_handle(nvs, NVS_TYPE_BLOB, &it);

    while (err == ESP_OK && s_recv_count < GPG_RECV_MAX_KEYS) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);

        // Read key to get fingerprint
        gpg_received_key_nvs_t data;
        size_t len = sizeof(data);
        if (nvs_get_blob(nvs, info.key, &data, &len) == ESP_OK) {
            memcpy(s_recv_fingerprints[s_recv_count], data.fingerprint, GPG_FINGERPRINT_LEN);
            s_recv_count++;
        }

        err = nvs_entry_next(&it);
    }
    nvs_release_iterator(it);
    nvs_close(nvs);

    s_recv_cache_valid = true;
    ESP_LOGI(TAG, "Received keys cache: %u keys", s_recv_count);
}

bool gpg_receive_pubkey(const uint8_t *pubkey, size_t pubkey_len, uint8_t curve,
                        const char *user_id, const uint8_t *fingerprint) {
    if (!pubkey || pubkey_len == 0 || pubkey_len > GPG_PUBKEY_MAX_LEN) {
        ESP_LOGE(TAG, "Invalid pubkey");
        return false;
    }
    if (!user_id || strlen(user_id) == 0) {
        ESP_LOGE(TAG, "Empty user_id");
        return false;
    }
    if (!fingerprint) {
        ESP_LOGE(TAG, "Missing fingerprint");
        return false;
    }

    // Check if we already have this key
    if (!s_recv_cache_valid) {
        refresh_recv_cache();
    }

    for (uint8_t i = 0; i < s_recv_count; i++) {
        if (memcmp(s_recv_fingerprints[i], fingerprint, GPG_FINGERPRINT_LEN) == 0) {
            ESP_LOGI(TAG, "Key already received, updating");
            // Fall through to update
        }
    }

    // Check limit
    if (s_recv_count >= GPG_RECV_MAX_KEYS) {
        ESP_LOGW(TAG, "Max received keys reached (%u)", GPG_RECV_MAX_KEYS);
        return false;
    }

    // Build NVS key
    char nvs_key[24];
    build_nvs_key(fingerprint, nvs_key);

    // Prepare data
    gpg_received_key_nvs_t data;
    memset(&data, 0, sizeof(data));
    data.curve = curve;
    strncpy(data.user_id, user_id, GPG_USER_ID_MAX - 1);
    memcpy(data.pubkey, pubkey, pubkey_len);
    data.pubkey_len = pubkey_len;
    memcpy(data.fingerprint, fingerprint, GPG_FINGERPRINT_LEN);
    data.received_at = get_unix_time();
    data.sig_len = 0;  // No signature yet
    data.flags = 0;

    // Store in NVS
    nvs_handle_t nvs;
    if (nvs_open(NVS_GPG_RECV, NVS_READWRITE, &nvs) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS");
        return false;
    }

    esp_err_t err = nvs_set_blob(nvs, nvs_key, &data, sizeof(data));
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store key: %s", esp_err_to_name(err));
        return false;
    }

    // Invalidate cache
    s_recv_cache_valid = false;

    char fp_hex[41];
    for (int i = 0; i < 20; i++) {
        sprintf(fp_hex + i*2, "%02X", fingerprint[i]);
    }
    ESP_LOGI(TAG, "Received key: %s (FP: %.16s...)", user_id, fp_hex);
    return true;
}

uint8_t gpg_received_count(void) {
    if (!s_recv_cache_valid) {
        refresh_recv_cache();
    }
    return s_recv_count;
}

bool gpg_received_get_info(uint8_t index, gpg_received_key_info_t *info) {
    if (!info) return false;

    if (!s_recv_cache_valid) {
        refresh_recv_cache();
    }

    if (index >= s_recv_count) {
        return false;
    }

    // Build NVS key from cached fingerprint
    char nvs_key[24];
    build_nvs_key(s_recv_fingerprints[index], nvs_key);

    // Read from NVS
    nvs_handle_t nvs;
    if (nvs_open(NVS_GPG_RECV, NVS_READONLY, &nvs) != ESP_OK) {
        return false;
    }

    gpg_received_key_nvs_t data;
    size_t len = sizeof(data);
    esp_err_t err = nvs_get_blob(nvs, nvs_key, &data, &len);
    nvs_close(nvs);

    if (err != ESP_OK) {
        return false;
    }

    // Fill info struct
    info->curve = data.curve;
    strncpy(info->user_id, data.user_id, GPG_USER_ID_MAX);
    memcpy(info->fingerprint, data.fingerprint, GPG_FINGERPRINT_LEN);
    info->received_at = data.received_at;
    info->signed_by_me = (data.sig_len > 0);
    info->flags = data.flags;

    return true;
}

bool gpg_received_get_key(const uint8_t *fingerprint, uint8_t *pubkey,
                          size_t *pubkey_len, uint8_t *curve) {
    if (!fingerprint || !pubkey || !pubkey_len || !curve) {
        return false;
    }

    char nvs_key[24];
    build_nvs_key(fingerprint, nvs_key);

    nvs_handle_t nvs;
    if (nvs_open(NVS_GPG_RECV, NVS_READONLY, &nvs) != ESP_OK) {
        return false;
    }

    gpg_received_key_nvs_t data;
    size_t len = sizeof(data);
    esp_err_t err = nvs_get_blob(nvs, nvs_key, &data, &len);
    nvs_close(nvs);

    if (err != ESP_OK) {
        return false;
    }

    memcpy(pubkey, data.pubkey, data.pubkey_len);
    *pubkey_len = data.pubkey_len;
    *curve = data.curve;
    return true;
}

bool gpg_cross_sign(uint8_t index) {
    if (!gpg_is_initialized()) {
        ESP_LOGE(TAG, "No GPG key configured - cannot sign");
        return false;
    }

    if (!s_recv_cache_valid) {
        refresh_recv_cache();
    }

    if (index >= s_recv_count) {
        ESP_LOGE(TAG, "Invalid index: %u", index);
        return false;
    }

    // Build NVS key
    char nvs_key[24];
    build_nvs_key(s_recv_fingerprints[index], nvs_key);

    // Read existing data
    nvs_handle_t nvs;
    if (nvs_open(NVS_GPG_RECV, NVS_READWRITE, &nvs) != ESP_OK) {
        return false;
    }

    gpg_received_key_nvs_t data;
    size_t len = sizeof(data);
    esp_err_t err = nvs_get_blob(nvs, nvs_key, &data, &len);
    if (err != ESP_OK) {
        nvs_close(nvs);
        return false;
    }

    // Create hash to sign: SHA256(fingerprint || user_id)
    // This binds the key to the user ID
    uint8_t hash[32];
    mbedtls_sha256_context sha256;
    mbedtls_sha256_init(&sha256);
    mbedtls_sha256_starts(&sha256, 0);
    mbedtls_sha256_update(&sha256, data.fingerprint, GPG_FINGERPRINT_LEN);
    mbedtls_sha256_update(&sha256, (const uint8_t *)data.user_id, strlen(data.user_id));
    mbedtls_sha256_finish(&sha256, hash);
    mbedtls_sha256_free(&sha256);

    // Sign the hash
    size_t sig_len;
    if (!gpg_sign_hash(hash, sizeof(hash), data.signature, &sig_len)) {
        ESP_LOGE(TAG, "Failed to sign key");
        nvs_close(nvs);
        return false;
    }
    data.sig_len = sig_len;

    // Update in NVS
    err = nvs_set_blob(nvs, nvs_key, &data, sizeof(data));
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save signature");
        return false;
    }

    ESP_LOGI(TAG, "Cross-signed key: %s", data.user_id);
    return true;
}

bool gpg_received_delete(uint8_t index) {
    if (!s_recv_cache_valid) {
        refresh_recv_cache();
    }

    if (index >= s_recv_count) {
        return false;
    }

    char nvs_key[24];
    build_nvs_key(s_recv_fingerprints[index], nvs_key);

    nvs_handle_t nvs;
    if (nvs_open(NVS_GPG_RECV, NVS_READWRITE, &nvs) != ESP_OK) {
        return false;
    }

    esp_err_t err = nvs_erase_key(nvs, nvs_key);
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);

    if (err != ESP_OK) {
        return false;
    }

    // Invalidate cache
    s_recv_cache_valid = false;
    ESP_LOGI(TAG, "Deleted received key at index %u", index);
    return true;
}

bool gpg_received_get_signature(uint8_t index, uint8_t *sig_out, size_t *sig_len) {
    if (!sig_out || !sig_len) return false;

    if (!s_recv_cache_valid) {
        refresh_recv_cache();
    }

    if (index >= s_recv_count) {
        return false;
    }

    char nvs_key[24];
    build_nvs_key(s_recv_fingerprints[index], nvs_key);

    nvs_handle_t nvs;
    if (nvs_open(NVS_GPG_RECV, NVS_READONLY, &nvs) != ESP_OK) {
        return false;
    }

    gpg_received_key_nvs_t data;
    size_t len = sizeof(data);
    esp_err_t err = nvs_get_blob(nvs, nvs_key, &data, &len);
    nvs_close(nvs);

    if (err != ESP_OK || data.sig_len == 0) {
        return false;
    }

    memcpy(sig_out, data.signature, data.sig_len);
    *sig_len = data.sig_len;
    return true;
}

bool gpg_export_for_broadcast(uint8_t *pubkey, size_t *pubkey_len, uint8_t *curve,
                              char *user_id, uint8_t *fingerprint) {
    if (!gpg_is_initialized()) {
        return false;
    }

    if (!pubkey || !pubkey_len || !curve || !user_id || !fingerprint) {
        return false;
    }

    // Export public key
    if (!gpg_export_pubkey_raw(pubkey, pubkey_len, curve)) {
        return false;
    }

    // Copy user ID
    strncpy(user_id, s_metadata.user_id, GPG_USER_ID_MAX);

    // Copy fingerprint
    memcpy(fingerprint, s_metadata.fingerprint, GPG_FINGERPRINT_LEN);

    return true;
}

// ============================================================================
// RFC 4880 OpenPGP Packet Export
// ============================================================================

// Write OpenPGP packet header (new format) per RFC 4880 Section 4.2.2
// Returns bytes written, 0 on error
static size_t write_packet_header(uint8_t *buf, uint8_t tag, size_t body_len) {
    // New format packet header: 0xC0 | tag
    buf[0] = 0xC0 | (tag & 0x3F);

    if (body_len < 192) {
        // One-octet length: 0-191
        buf[1] = (uint8_t)body_len;
        return 2;
    } else if (body_len < 8384) {
        // Two-octet length: 192-8383
        // Encoding: first_byte = ((len - 192) >> 8) + 192
        //           second_byte = (len - 192) & 0xFF
        // Decoding: len = ((first_byte - 192) << 8) + second_byte + 192
        size_t adjusted = body_len - 192;
        buf[1] = (uint8_t)((adjusted >> 8) + 192);
        buf[2] = (uint8_t)(adjusted & 0xFF);
        return 3;
    } else {
        // Five-octet length: 8384+
        buf[1] = 0xFF;
        buf[2] = (uint8_t)((body_len >> 24) & 0xFF);
        buf[3] = (uint8_t)((body_len >> 16) & 0xFF);
        buf[4] = (uint8_t)((body_len >> 8) & 0xFF);
        buf[5] = (uint8_t)(body_len & 0xFF);
        return 6;
    }
}

// Build Public Key Packet body (Tag 6) for V4 key
// Returns body length, 0 on error
static size_t build_pubkey_packet_body(uint8_t *buf, size_t buf_size,
                                        const uint8_t *pubkey, size_t pubkey_len,
                                        uint8_t curve, uint32_t created_at) {
    // Algorithm IDs per RFC 6637/4880: 19=ECDSA, 22=EdDSA
    uint8_t algo = (curve == CDC_CURVE_ED25519) ? 22 : 19;

    // OIDs (length-prefixed, validated per RFC 6637 Section 9)
    // See calculate_fingerprint() for detailed OID documentation
    static const uint8_t oid_ed25519[] = {0x09, 0x2B, 0x06, 0x01, 0x04, 0x01, 0xDA, 0x47, 0x0F, 0x01};
    static const uint8_t oid_p256[] = {0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07};

    const uint8_t *oid;
    size_t oid_len;
    if (curve == CDC_CURVE_ED25519) {
        oid = oid_ed25519;
        oid_len = sizeof(oid_ed25519);
    } else {
        oid = oid_p256;
        oid_len = sizeof(oid_p256);
    }

    // Build MPI per RFC 4880 Section 3.2
    uint8_t mpi[67];
    size_t mpi_len;
    if (curve == CDC_CURVE_ED25519) {
        // Ed25519: 32-byte point, count actual bits (256 or 255 if MSB=0)
        uint16_t bits = 256;
        if ((pubkey[0] & 0x80) == 0) bits = 255;
        mpi[0] = (bits >> 8) & 0xFF;
        mpi[1] = bits & 0xFF;
        memcpy(mpi + 2, pubkey, 32);
        mpi_len = 2 + 32;
    } else {
        // P-256: 04 || X || Y = 65 bytes = 520 bits
        uint16_t bits = 520;
        mpi[0] = (bits >> 8) & 0xFF;
        mpi[1] = bits & 0xFF;
        mpi[2] = 0x04;
        memcpy(mpi + 3, pubkey, 64);
        mpi_len = 2 + 1 + 64;
    }

    // Total body: version(1) + timestamp(4) + algo(1) + oid + mpi
    size_t body_len = 1 + 4 + 1 + oid_len + mpi_len;
    if (body_len > buf_size) return 0;

    size_t idx = 0;
    buf[idx++] = 4;  // Version 4
    buf[idx++] = (created_at >> 24) & 0xFF;
    buf[idx++] = (created_at >> 16) & 0xFF;
    buf[idx++] = (created_at >> 8) & 0xFF;
    buf[idx++] = created_at & 0xFF;
    buf[idx++] = algo;
    memcpy(buf + idx, oid, oid_len);
    idx += oid_len;
    memcpy(buf + idx, mpi, mpi_len);
    idx += mpi_len;

    return idx;
}

// Build Signature Packet body (Tag 2) - Type 0x10 Generic Certification
// This is a V4 signature over a public key + user ID binding
static size_t build_certification_sig_packet_body(
    uint8_t *buf, size_t buf_size,
    const uint8_t *pubkey_packet_body, size_t pubkey_body_len,
    const char *user_id,
    const uint8_t *issuer_fingerprint,
    uint8_t signer_curve,
    uint32_t sig_creation_time
) {
    if (!gpg_is_initialized()) {
        ESP_LOGE(TAG, "No GPG key for signing");
        return 0;
    }

    // Signature subpackets
    // Hashed subpackets: creation time (sub 2), issuer fingerprint (sub 33)
    // Unhashed subpackets: issuer key ID (sub 16)

    // Hashed subpackets buffer
    uint8_t hashed_subpkts[64];
    size_t hashed_len = 0;

    // Sub 2: Signature Creation Time (4 bytes)
    hashed_subpkts[hashed_len++] = 5;  // Length
    hashed_subpkts[hashed_len++] = 2;  // Type
    hashed_subpkts[hashed_len++] = (sig_creation_time >> 24) & 0xFF;
    hashed_subpkts[hashed_len++] = (sig_creation_time >> 16) & 0xFF;
    hashed_subpkts[hashed_len++] = (sig_creation_time >> 8) & 0xFF;
    hashed_subpkts[hashed_len++] = sig_creation_time & 0xFF;

    // Sub 33: Issuer Fingerprint (V4 = 21 bytes: version + 20 bytes fingerprint)
    hashed_subpkts[hashed_len++] = 22;  // Length
    hashed_subpkts[hashed_len++] = 33;  // Type
    hashed_subpkts[hashed_len++] = 4;   // V4 key
    memcpy(hashed_subpkts + hashed_len, s_metadata.fingerprint, 20);
    hashed_len += 20;

    // Unhashed subpackets
    uint8_t unhashed_subpkts[16];
    size_t unhashed_len = 0;

    // Sub 16: Issuer Key ID (8 bytes = last 8 bytes of fingerprint)
    unhashed_subpkts[unhashed_len++] = 9;   // Length
    unhashed_subpkts[unhashed_len++] = 16;  // Type
    memcpy(unhashed_subpkts + unhashed_len, s_metadata.fingerprint + 12, 8);
    unhashed_len += 8;

    // Build hash input per RFC 4880 Section 5.2.4:
    // 1. 0x99 || 2-octet pubkey length || pubkey packet body
    // 2. 0xB4 || 4-octet user ID length || user ID
    // 3. Signature packet (version through hashed subpackets)
    // 4. Trailer: 0x04 || 0xFF || 4-octet length of hashed portion

    mbedtls_sha256_context sha256;
    mbedtls_sha256_init(&sha256);
    mbedtls_sha256_starts(&sha256, 0);

    // 1. Public key packet (for certification signatures)
    uint8_t pk_hdr[3];
    pk_hdr[0] = 0x99;
    pk_hdr[1] = (pubkey_body_len >> 8) & 0xFF;
    pk_hdr[2] = pubkey_body_len & 0xFF;
    mbedtls_sha256_update(&sha256, pk_hdr, 3);
    mbedtls_sha256_update(&sha256, pubkey_packet_body, pubkey_body_len);

    // 2. User ID packet
    size_t uid_len = strlen(user_id);
    uint8_t uid_hdr[5];
    uid_hdr[0] = 0xB4;
    uid_hdr[1] = (uid_len >> 24) & 0xFF;
    uid_hdr[2] = (uid_len >> 16) & 0xFF;
    uid_hdr[3] = (uid_len >> 8) & 0xFF;
    uid_hdr[4] = uid_len & 0xFF;
    mbedtls_sha256_update(&sha256, uid_hdr, 5);
    mbedtls_sha256_update(&sha256, (const uint8_t *)user_id, uid_len);

    // 3. Signature packet header (version through hashed subpackets)
    uint8_t sig_algo = (s_metadata.curve == CDC_CURVE_ED25519) ? 22 : 19;
    uint8_t sig_hdr[6];
    sig_hdr[0] = 4;     // Version 4
    sig_hdr[1] = 0x10;  // Type: Generic Certification
    sig_hdr[2] = sig_algo;  // Public key algorithm
    sig_hdr[3] = 8;     // Hash algorithm: SHA-256
    sig_hdr[4] = (hashed_len >> 8) & 0xFF;
    sig_hdr[5] = hashed_len & 0xFF;
    mbedtls_sha256_update(&sha256, sig_hdr, 6);
    mbedtls_sha256_update(&sha256, hashed_subpkts, hashed_len);

    // 4. Trailer
    uint32_t hashed_total = 6 + hashed_len;  // sig_hdr + hashed_subpkts
    uint8_t trailer[6];
    trailer[0] = 0x04;
    trailer[1] = 0xFF;
    trailer[2] = (hashed_total >> 24) & 0xFF;
    trailer[3] = (hashed_total >> 16) & 0xFF;
    trailer[4] = (hashed_total >> 8) & 0xFF;
    trailer[5] = hashed_total & 0xFF;
    mbedtls_sha256_update(&sha256, trailer, 6);

    uint8_t hash[32];
    mbedtls_sha256_finish(&sha256, hash);
    mbedtls_sha256_free(&sha256);

    // Sign the hash
    uint8_t raw_sig[64];
    size_t raw_sig_len;
    if (!gpg_sign_hash(hash, 32, raw_sig, &raw_sig_len)) {
        ESP_LOGE(TAG, "Failed to sign certification");
        return 0;
    }

    // Build signature MPIs per RFC 4880 Section 3.2
    // For ECDSA/EdDSA: two MPIs (R and S), each 32 bytes
    // Bit count must reflect actual bits (skip leading zeros)
    uint8_t sig_mpis[68];  // 2x (2-byte bitcount + 32 bytes)
    size_t mpi_idx = 0;

    // Helper: count actual bits in a 32-byte value
    auto count_mpi_bits = [](const uint8_t *data) -> uint16_t {
        // Find first non-zero byte
        int first_nonzero = 0;
        while (first_nonzero < 32 && data[first_nonzero] == 0) first_nonzero++;
        if (first_nonzero == 32) return 0;  // All zeros
        // Count bits: (32 - first_nonzero) * 8 - leading_zeros_in_first_byte
        uint8_t first_byte = data[first_nonzero];
        int leading_zeros = 0;
        for (int i = 7; i >= 0; i--) {
            if (first_byte & (1 << i)) break;
            leading_zeros++;
        }
        return (uint16_t)((32 - first_nonzero) * 8 - leading_zeros);
    };

    // R value
    uint16_t r_bits = count_mpi_bits(raw_sig);
    if (r_bits == 0) r_bits = 1;  // At least 1 bit for zero
    sig_mpis[mpi_idx++] = (r_bits >> 8) & 0xFF;
    sig_mpis[mpi_idx++] = r_bits & 0xFF;
    memcpy(sig_mpis + mpi_idx, raw_sig, 32);
    mpi_idx += 32;

    // S value
    uint16_t s_bits = count_mpi_bits(raw_sig + 32);
    if (s_bits == 0) s_bits = 1;
    sig_mpis[mpi_idx++] = (s_bits >> 8) & 0xFF;
    sig_mpis[mpi_idx++] = s_bits & 0xFF;
    memcpy(sig_mpis + mpi_idx, raw_sig + 32, 32);
    mpi_idx += 32;

    // Calculate total signature packet body length
    // sig_hdr(6) + hashed_subpkts + unhashed_len_field(2) + unhashed_subpkts + hash_prefix(2) + sig_mpis
    size_t sig_body_len = 6 + hashed_len + 2 + unhashed_len + 2 + mpi_idx;

    if (sig_body_len > buf_size) {
        ESP_LOGE(TAG, "Buffer too small for signature");
        return 0;
    }

    // Write signature packet body
    size_t idx = 0;
    memcpy(buf + idx, sig_hdr, 6);
    idx += 6;
    memcpy(buf + idx, hashed_subpkts, hashed_len);
    idx += hashed_len;
    buf[idx++] = (unhashed_len >> 8) & 0xFF;
    buf[idx++] = unhashed_len & 0xFF;
    memcpy(buf + idx, unhashed_subpkts, unhashed_len);
    idx += unhashed_len;
    // Hash prefix (first 2 bytes of hash for quick check)
    buf[idx++] = hash[0];
    buf[idx++] = hash[1];
    memcpy(buf + idx, sig_mpis, mpi_idx);
    idx += mpi_idx;

    return idx;
}

bool gpg_export_signed_key(uint8_t index, uint8_t *buf, size_t buf_size, size_t *out_len) {
    if (!buf || !out_len || buf_size < 256) {
        return false;
    }

    if (!gpg_is_initialized()) {
        ESP_LOGE(TAG, "No GPG key configured - cannot export");
        return false;
    }

    if (!s_recv_cache_valid) {
        refresh_recv_cache();
    }

    if (index >= s_recv_count) {
        ESP_LOGE(TAG, "Invalid index: %u", index);
        return false;
    }

    // Get received key data
    char nvs_key[24];
    build_nvs_key(s_recv_fingerprints[index], nvs_key);

    nvs_handle_t nvs;
    if (nvs_open(NVS_GPG_RECV, NVS_READONLY, &nvs) != ESP_OK) {
        return false;
    }

    gpg_received_key_nvs_t data;
    size_t len = sizeof(data);
    esp_err_t err = nvs_get_blob(nvs, nvs_key, &data, &len);
    nvs_close(nvs);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read key data");
        return false;
    }

    if (data.sig_len == 0) {
        ESP_LOGE(TAG, "Key not signed - sign first with gpg_cross_sign()");
        return false;
    }

    // Build packets
    size_t idx = 0;

    // 1. Public Key Packet (Tag 6)
    uint8_t pk_body[128];
    size_t pk_body_len = build_pubkey_packet_body(pk_body, sizeof(pk_body),
                                                   data.pubkey, data.pubkey_len,
                                                   data.curve, data.received_at);
    if (pk_body_len == 0) {
        ESP_LOGE(TAG, "Failed to build pubkey packet");
        return false;
    }

    size_t hdr_len = write_packet_header(buf + idx, 6, pk_body_len);
    idx += hdr_len;
    memcpy(buf + idx, pk_body, pk_body_len);
    idx += pk_body_len;

    // 2. User ID Packet (Tag 13)
    size_t uid_body_len = strlen(data.user_id);
    hdr_len = write_packet_header(buf + idx, 13, uid_body_len);
    idx += hdr_len;
    memcpy(buf + idx, data.user_id, uid_body_len);
    idx += uid_body_len;

    // 3. Signature Packet (Tag 2) - Certification
    uint8_t sig_body[256];
    size_t sig_body_len = build_certification_sig_packet_body(
        sig_body, sizeof(sig_body),
        pk_body, pk_body_len,
        data.user_id,
        data.fingerprint,
        data.curve,
        get_unix_time()
    );
    if (sig_body_len == 0) {
        ESP_LOGE(TAG, "Failed to build signature packet");
        return false;
    }

    hdr_len = write_packet_header(buf + idx, 2, sig_body_len);
    idx += hdr_len;
    memcpy(buf + idx, sig_body, sig_body_len);
    idx += sig_body_len;

    *out_len = idx;
    ESP_LOGI(TAG, "Exported signed key: %zu bytes", idx);
    return true;
}

// CRC24 for OpenPGP ASCII armor
static uint32_t crc24(const uint8_t *data, size_t len) {
    uint32_t crc = 0xB704CE;
    for (size_t i = 0; i < len; i++) {
        crc ^= ((uint32_t)data[i]) << 16;
        for (int j = 0; j < 8; j++) {
            crc <<= 1;
            if (crc & 0x1000000) {
                crc ^= 0x1864CFB;
            }
        }
    }
    return crc & 0xFFFFFF;
}

bool gpg_export_signed_key_armored(uint8_t index, char *buf, size_t buf_size, size_t *out_len) {
    if (!buf || !out_len || buf_size < 512) {
        return false;
    }

    // Get binary data first
    uint8_t binary[512];
    size_t binary_len;
    if (!gpg_export_signed_key(index, binary, sizeof(binary), &binary_len)) {
        return false;
    }

    // Calculate CRC24
    uint32_t crc = crc24(binary, binary_len);
    uint8_t crc_bytes[3] = {
        (uint8_t)((crc >> 16) & 0xFF),
        (uint8_t)((crc >> 8) & 0xFF),
        (uint8_t)(crc & 0xFF)
    };

    // Base64 encode binary
    size_t b64_len = 0;
    int ret = mbedtls_base64_encode(NULL, 0, &b64_len, binary, binary_len);
    if (ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) {
        return false;
    }

    char b64_data[768];
    ret = mbedtls_base64_encode((unsigned char *)b64_data, sizeof(b64_data), &b64_len,
                                 binary, binary_len);
    if (ret != 0) {
        ESP_LOGE(TAG, "Base64 encode failed");
        return false;
    }

    // Base64 encode CRC
    char b64_crc[8];
    size_t crc_b64_len;
    ret = mbedtls_base64_encode((unsigned char *)b64_crc, sizeof(b64_crc), &crc_b64_len,
                                 crc_bytes, 3);
    if (ret != 0) {
        return false;
    }

    // Format with line breaks (76 chars per line)
    char formatted_b64[1024];
    size_t fmt_idx = 0;
    for (size_t i = 0; i < b64_len; i++) {
        formatted_b64[fmt_idx++] = b64_data[i];
        if ((i + 1) % 64 == 0 && i + 1 < b64_len) {
            formatted_b64[fmt_idx++] = '\n';
        }
    }
    formatted_b64[fmt_idx] = '\0';

    // Build armored output
    int written = snprintf(buf, buf_size,
        "-----BEGIN PGP PUBLIC KEY BLOCK-----\n"
        "\n"
        "%s\n"
        "=%s\n"
        "-----END PGP PUBLIC KEY BLOCK-----\n",
        formatted_b64, b64_crc);

    if (written < 0 || (size_t)written >= buf_size) {
        return false;
    }

    *out_len = written;
    ESP_LOGI(TAG, "Exported armored signed key: %d bytes", written);
    return true;
}

#endif // FEATURE_GPG
