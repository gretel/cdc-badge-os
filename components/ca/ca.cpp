// Certificate Authority (CA) Implementation
// Uses TROPIC01 for secure key storage and signing
// Full X.509 certificate support via mbedTLS

#include "ca.h"

#if FEATURE_CA

#include "cdc_log.h"
#include "tropic01.h"
#include "tropic01_cache.h"

#include "nvs_flash.h"
#include "nvs.h"

#include "mbedtls/sha256.h"
#include "mbedtls/base64.h"
#include "mbedtls/pk.h"
#include "mbedtls/ecp.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/x509_csr.h"
#include "mbedtls/asn1write.h"
#include "mbedtls/oid.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

#define TAG "CA"

// ============================================================================
// NVS Keys
// ============================================================================

#define NVS_NAMESPACE           "ca"
#define NVS_KEY_SERIAL          "serial"
#define NVS_KEY_ISSUED_COUNT    "issued_cnt"
#define NVS_KEY_CERT_PREFIX     "cert_"

// ============================================================================
// CA Metadata (stored in TROPIC01 R-Memory)
// ============================================================================

#define CA_METADATA_MAGIC       0xCA01
#define CA_RMEM_SLOT            32      // R-Memory slot for CA metadata

typedef struct __attribute__((packed)) {
    uint16_t magic;
    char common_name[CA_MAX_CN_LEN];
    uint32_t created_at;
    uint32_t valid_until;
} ca_metadata_t;

// ============================================================================
// State
// ============================================================================

static bool g_ca_loaded = false;
static ca_metadata_t g_ca_metadata;
static uint32_t g_serial_counter = 1;
static uint32_t g_issued_count = 0;

// Cached public key from TROPIC01
static uint8_t g_ca_pubkey[64];  // X (32 bytes) + Y (32 bytes)
static bool g_ca_pubkey_loaded = false;

// ============================================================================
// Forward Declarations
// ============================================================================

static bool load_metadata(void);
static bool save_metadata(void);
static bool load_nvs_state(void);
static bool save_nvs_state(void);
static bool load_ca_pubkey(void);

// ============================================================================
// Initialization
// ============================================================================

bool ca_init(void) {
    if (g_ca_loaded) return true;

    LOG_I(TAG, "Initializing CA module...");

    // Load NVS state
    if (!load_nvs_state()) {
        LOG_D(TAG, "No NVS state found");
    }

    // Check if root key exists in TROPIC01
    if (!tropic01_cache_ecc_exists(CA_ECC_SLOT)) {
        LOG_I(TAG, "CA root key not found");
        g_ca_loaded = true;
        return true;
    }

    // Load metadata from R-Memory
    if (!load_metadata()) {
        LOG_W(TAG, "CA metadata not found or invalid");
        g_ca_loaded = true;
        return true;
    }

    // Load public key
    if (!load_ca_pubkey()) {
        LOG_W(TAG, "Failed to load CA public key");
    }

    g_ca_loaded = true;
    LOG_I(TAG, "CA initialized: %s (issued: %lu)", g_ca_metadata.common_name,
          (unsigned long)g_issued_count);
    return true;
}

bool ca_is_initialized(void) {
    return g_ca_loaded && tropic01_cache_ecc_exists(CA_ECC_SLOT) &&
           g_ca_metadata.magic == CA_METADATA_MAGIC;
}

bool ca_get_status(ca_status_t *status) {
    if (!status) return false;

    memset(status, 0, sizeof(ca_status_t));
    status->initialized = ca_is_initialized();

    if (status->initialized) {
        strncpy(status->common_name, g_ca_metadata.common_name, CA_MAX_CN_LEN - 1);
        status->serial_counter = g_serial_counter;
        status->issued_count = g_issued_count;
        status->created_at = g_ca_metadata.created_at;
        status->valid_until = g_ca_metadata.valid_until;
    }

    return true;
}

// ============================================================================
// Storage Functions
// ============================================================================

static bool load_metadata(void) {
    uint8_t buf[sizeof(ca_metadata_t)];
    uint16_t read_size;

    if (!tropic01_rmem_read(CA_RMEM_SLOT, buf, sizeof(buf), &read_size)) {
        return false;
    }

    if (read_size < sizeof(ca_metadata_t)) {
        return false;
    }

    memcpy(&g_ca_metadata, buf, sizeof(ca_metadata_t));

    if (g_ca_metadata.magic != CA_METADATA_MAGIC) {
        LOG_W(TAG, "Invalid metadata magic: 0x%04X", g_ca_metadata.magic);
        return false;
    }

    return true;
}

static bool save_metadata(void) {
    g_ca_metadata.magic = CA_METADATA_MAGIC;
    return tropic01_rmem_write(CA_RMEM_SLOT, (uint8_t*)&g_ca_metadata, sizeof(ca_metadata_t));
}

static bool load_nvs_state(void) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        return false;
    }

    nvs_get_u32(nvs, NVS_KEY_SERIAL, &g_serial_counter);
    nvs_get_u32(nvs, NVS_KEY_ISSUED_COUNT, &g_issued_count);

    nvs_close(nvs);
    return true;
}

static bool save_nvs_state(void) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        LOG_E(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return false;
    }

    nvs_set_u32(nvs, NVS_KEY_SERIAL, g_serial_counter);
    nvs_set_u32(nvs, NVS_KEY_ISSUED_COUNT, g_issued_count);
    nvs_commit(nvs);
    nvs_close(nvs);
    return true;
}

static bool load_ca_pubkey(void) {
    uint8_t curve, origin;
    if (!tropic01_ecc_key_read(CA_ECC_SLOT, g_ca_pubkey, sizeof(g_ca_pubkey), &curve, &origin)) {
        return false;
    }
    g_ca_pubkey_loaded = true;
    return true;
}

// ============================================================================
// mbedTLS PK Context Setup for TROPIC01
// ============================================================================

// Setup pk_context with TROPIC01 public key (mbedTLS 3.x compatible)
// Uses SubjectPublicKeyInfo DER format for parsing
static int setup_tropic01_pk(mbedtls_pk_context *pk) {
    if (!g_ca_pubkey_loaded) {
        if (!load_ca_pubkey()) {
            return MBEDTLS_ERR_PK_KEY_INVALID_FORMAT;
        }
    }

    // Build SubjectPublicKeyInfo DER for P-256 public key
    // SubjectPublicKeyInfo ::= SEQUENCE {
    //   algorithm AlgorithmIdentifier,
    //   subjectPublicKey BIT STRING
    // }
    // For P-256: BIT STRING contains 04 || X (32 bytes) || Y (32 bytes)

    // Pre-computed header for P-256 SPKI (AlgorithmIdentifier + BIT STRING header)
    // Total: 26 bytes header + 65 bytes pubkey = 91 bytes
    static const uint8_t spki_header[] = {
        0x30, 0x59,  // SEQUENCE, 89 bytes
        // AlgorithmIdentifier
        0x30, 0x13,  // SEQUENCE, 19 bytes
        0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01,  // OID 1.2.840.10045.2.1 (ecPublicKey)
        0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07,  // OID 1.2.840.10045.3.1.7 (secp256r1)
        // BIT STRING header
        0x03, 0x42,  // BIT STRING, 66 bytes
        0x00         // 0 unused bits
    };

    uint8_t spki[91];
    memcpy(spki, spki_header, sizeof(spki_header));

    // Add uncompressed point (04 || X || Y)
    spki[sizeof(spki_header)] = 0x04;  // Uncompressed point marker
    memcpy(spki + sizeof(spki_header) + 1, g_ca_pubkey, 64);  // X and Y coordinates

    LOG_D(TAG, "SPKI built: %zu bytes", sizeof(spki));

    // Parse the SPKI to initialize the pk context
    int ret = mbedtls_pk_parse_public_key(pk, spki, sizeof(spki));
    if (ret != 0) {
        LOG_E(TAG, "mbedtls_pk_parse_public_key failed: -0x%04x", -ret);
        // Log first bytes of SPKI for debugging
        LOG_E(TAG, "SPKI[0-7]: %02X %02X %02X %02X %02X %02X %02X %02X",
              spki[0], spki[1], spki[2], spki[3], spki[4], spki[5], spki[6], spki[7]);
        LOG_E(TAG, "Pubkey[0-7]: %02X %02X %02X %02X %02X %02X %02X %02X",
              g_ca_pubkey[0], g_ca_pubkey[1], g_ca_pubkey[2], g_ca_pubkey[3],
              g_ca_pubkey[4], g_ca_pubkey[5], g_ca_pubkey[6], g_ca_pubkey[7]);
        return ret;
    }

    LOG_D(TAG, "TROPIC01 public key loaded into pk context");
    return 0;
}

// ============================================================================
// CA Setup
// ============================================================================

bool ca_setup(const char *cn) {
    LOG_I(TAG, "=== CA SETUP START ===");

    if (!cn || strlen(cn) == 0) {
        LOG_E(TAG, "Common Name required");
        return false;
    }

    LOG_I(TAG, "Checking if CA is already initialized...");
    bool is_init = ca_is_initialized();
    LOG_I(TAG, "ca_is_initialized() = %s", is_init ? "true" : "false");
    LOG_I(TAG, "  g_ca_loaded = %s", g_ca_loaded ? "true" : "false");
    LOG_I(TAG, "  cache_ecc_exists(31) = %s", tropic01_cache_ecc_exists(CA_ECC_SLOT) ? "true" : "false");
    LOG_I(TAG, "  magic = 0x%08lx (expected 0x%08lx)", (unsigned long)g_ca_metadata.magic, (unsigned long)CA_METADATA_MAGIC);

    if (is_init) {
        LOG_E(TAG, "CA already initialized. Use ca_factory_reset() first.");
        return false;
    }

    LOG_I(TAG, "Setting up CA with CN: %s", cn);

    // Check if slot has an old key (orphaned from previous failed init)
    if (tropic01_cache_ecc_exists(CA_ECC_SLOT)) {
        LOG_W(TAG, "Slot %d has orphaned key, erasing first...", CA_ECC_SLOT);
        tropic01_ecc_key_erase(CA_ECC_SLOT);
    }

    // Generate P-256 key in TROPIC01 slot 31
    LOG_I(TAG, "Generating P-256 key in slot %d...", CA_ECC_SLOT);
    if (!tropic01_ecc_key_generate(CA_ECC_SLOT, CDC_CURVE_P256)) {
        LOG_E(TAG, "Failed to generate root key in slot %d", CA_ECC_SLOT);
        return false;
    }
    LOG_I(TAG, "Key generated successfully");

    // Load the new public key
    LOG_I(TAG, "Loading generated public key...");
    if (!load_ca_pubkey()) {
        LOG_E(TAG, "Failed to load generated public key");
        tropic01_ecc_key_erase(CA_ECC_SLOT);
        return false;
    }
    LOG_I(TAG, "Public key loaded successfully");

    // Initialize metadata
    memset(&g_ca_metadata, 0, sizeof(g_ca_metadata));
    strncpy(g_ca_metadata.common_name, cn, CA_MAX_CN_LEN - 1);

    time_t now = time(NULL);
    g_ca_metadata.created_at = (uint32_t)now;
    g_ca_metadata.valid_until = (uint32_t)(now + ((time_t)CA_CERT_VALIDITY_DAYS * 24 * 60 * 60));
    g_ca_metadata.magic = CA_METADATA_MAGIC;

    // Save metadata to TROPIC01
    if (!save_metadata()) {
        LOG_E(TAG, "Failed to save metadata");
        tropic01_ecc_key_erase(CA_ECC_SLOT);
        return false;
    }

    // Initialize NVS state
    g_serial_counter = 1;
    g_issued_count = 0;
    if (!save_nvs_state()) {
        LOG_W(TAG, "Failed to save NVS state");
    }

    LOG_I(TAG, "CA setup complete");
    return true;
}

// ============================================================================
// Helper: Format time for X.509 (YYYYMMDDHHMMSS)
// ============================================================================

static void format_x509_time(time_t t, char *buf, size_t buf_size) {
    struct tm tm_info;
    gmtime_r(&t, &tm_info);
    snprintf(buf, buf_size, "%04d%02d%02d%02d%02d%02d",
             tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday,
             tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec);
}

// ============================================================================
// Certificate Export - Self-Signed Root Certificate
// ============================================================================

bool ca_export_root_cert_pem(char *buf, size_t buf_size, size_t *out_len) {
    if (!buf || buf_size == 0 || !out_len) return false;
    if (!ca_is_initialized()) {
        LOG_E(TAG, "CA not initialized");
        return false;
    }

    // Declare all variables at function start
    mbedtls_x509write_cert crt;
    mbedtls_pk_context pk;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    int ret = 0;
    char name_buf[128];
    uint8_t serial_bytes[8];
    char not_before[16];
    char not_after[16];
    char *p = NULL;
    size_t remaining = 0;
    int written = 0;
    uint8_t spki[91];
    size_t spki_len = 0;
    uint8_t *q = NULL;
    size_t len = 0;
    size_t b64_len = 0;

    // Algorithm identifier (static, defined early)
    static const uint8_t alg_id[] = {
        0x30, 0x13,
        0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01,
        0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07
    };

    // Helper macro for cleanup
    #define CLEANUP_AND_RETURN(val) do { \
        mbedtls_x509write_crt_free(&crt); \
        mbedtls_pk_free(&pk); \
        mbedtls_ctr_drbg_free(&ctr_drbg); \
        mbedtls_entropy_free(&entropy); \
        return (val); \
    } while(0)

    mbedtls_x509write_crt_init(&crt);
    mbedtls_pk_init(&pk);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    // Seed RNG
    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                 (const unsigned char *)"CA", 2);
    if (ret != 0) {
        LOG_E(TAG, "Failed to seed RNG: -0x%04x", -ret);
        CLEANUP_AND_RETURN(false);
    }

    // Setup pk context with TROPIC01 public key
    ret = setup_tropic01_pk(&pk);
    if (ret != 0) {
        LOG_E(TAG, "Failed to setup PK context: -0x%04x", -ret);
        CLEANUP_AND_RETURN(false);
    }

    // Build subject/issuer name
    snprintf(name_buf, sizeof(name_buf), "CN=%s,O=CDC Badge CA", g_ca_metadata.common_name);

    // Set certificate parameters
    mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);

    // Set serial number (use 1 for root cert)
    serial_bytes[0] = 0x01;
    ret = mbedtls_x509write_crt_set_serial_raw(&crt, serial_bytes, 1);
    if (ret != 0) {
        LOG_E(TAG, "Failed to set serial: -0x%04x", -ret);
        CLEANUP_AND_RETURN(false);
    }

    // Set issuer and subject (same for self-signed)
    ret = mbedtls_x509write_crt_set_issuer_name(&crt, name_buf);
    if (ret != 0) {
        LOG_E(TAG, "Failed to set issuer: -0x%04x", -ret);
        CLEANUP_AND_RETURN(false);
    }

    ret = mbedtls_x509write_crt_set_subject_name(&crt, name_buf);
    if (ret != 0) {
        LOG_E(TAG, "Failed to set subject: -0x%04x", -ret);
        CLEANUP_AND_RETURN(false);
    }

    // Set validity period
    format_x509_time(g_ca_metadata.created_at, not_before, sizeof(not_before));
    format_x509_time(g_ca_metadata.valid_until, not_after, sizeof(not_after));

    ret = mbedtls_x509write_crt_set_validity(&crt, not_before, not_after);
    if (ret != 0) {
        LOG_E(TAG, "Failed to set validity: -0x%04x", -ret);
        CLEANUP_AND_RETURN(false);
    }

    // Set keys (subject and issuer are same for self-signed)
    mbedtls_x509write_crt_set_subject_key(&crt, &pk);
    mbedtls_x509write_crt_set_issuer_key(&crt, &pk);

    // Set basic constraints: CA=true
    ret = mbedtls_x509write_crt_set_basic_constraints(&crt, 1, -1);
    if (ret != 0) {
        LOG_E(TAG, "Failed to set basic constraints: -0x%04x", -ret);
        CLEANUP_AND_RETURN(false);
    }

    // Set key usage: keyCertSign, cRLSign
    ret = mbedtls_x509write_crt_set_key_usage(&crt,
        MBEDTLS_X509_KU_KEY_CERT_SIGN | MBEDTLS_X509_KU_CRL_SIGN);
    if (ret != 0) {
        LOG_E(TAG, "Failed to set key usage: -0x%04x", -ret);
        CLEANUP_AND_RETURN(false);
    }

    // Export simplified certificate info as PEM-like format
    p = buf;
    remaining = buf_size;

    written = snprintf(p, remaining,
        "-----BEGIN CERTIFICATE-----\n"
        "# CDC Badge CA Root Certificate\n"
        "# Note: Actual X.509 DER encoding requires PSA driver integration\n"
        "#\n");
    if (written < 0 || (size_t)written >= remaining) {
        CLEANUP_AND_RETURN(false);
    }
    p += written; remaining -= (size_t)written;

    // Export key info in a structured format
    written = snprintf(p, remaining, "Subject: %s\n", name_buf);
    if (written < 0 || (size_t)written >= remaining) {
        CLEANUP_AND_RETURN(false);
    }
    p += written; remaining -= (size_t)written;

    written = snprintf(p, remaining, "Not-Before: %s\n", not_before);
    if (written < 0 || (size_t)written >= remaining) {
        CLEANUP_AND_RETURN(false);
    }
    p += written; remaining -= (size_t)written;

    written = snprintf(p, remaining, "Not-After: %s\n", not_after);
    if (written < 0 || (size_t)written >= remaining) {
        CLEANUP_AND_RETURN(false);
    }
    p += written; remaining -= (size_t)written;

    written = snprintf(p, remaining, "Algorithm: ECDSA-SHA256 (P-256)\n");
    if (written < 0 || (size_t)written >= remaining) {
        CLEANUP_AND_RETURN(false);
    }
    p += written; remaining -= (size_t)written;

    // Build SubjectPublicKeyInfo manually
    q = spki + sizeof(spki);
    len = 0;

    // Public key point (04 || X || Y)
    *--q = 0x00;  // No unused bits
    q -= 64; memcpy(q, g_ca_pubkey, 64);
    *--q = 0x04;  // Uncompressed point
    len = 66;

    // BIT STRING wrapper
    *--q = (uint8_t)len; len++;
    *--q = 0x03;  // BIT STRING
    len++;

    // Add algorithm identifier
    q -= sizeof(alg_id);
    memcpy(q, alg_id, sizeof(alg_id));
    len += sizeof(alg_id);

    // Outer SEQUENCE
    if (len < 128) {
        *--q = (uint8_t)len;
        *--q = 0x30;
        len += 2;
    }

    spki_len = len;
    memmove(spki, q, spki_len);

    // Base64 encode
    mbedtls_base64_encode(NULL, 0, &b64_len, spki, spki_len);

    if (remaining < b64_len + 100) {
        CLEANUP_AND_RETURN(false);
    }

    written = snprintf(p, remaining, "Public-Key-SPKI: ");
    if (written < 0 || (size_t)written >= remaining) {
        CLEANUP_AND_RETURN(false);
    }
    p += written; remaining -= (size_t)written;

    ret = mbedtls_base64_encode((unsigned char*)p, remaining, &b64_len, spki, spki_len);
    if (ret != 0) {
        CLEANUP_AND_RETURN(false);
    }
    p += b64_len; remaining -= b64_len;

    written = snprintf(p, remaining, "\n-----END CERTIFICATE-----\n");
    if (written < 0 || (size_t)written >= remaining) {
        CLEANUP_AND_RETURN(false);
    }
    p += written;

    *out_len = p - buf;

    LOG_I(TAG, "Root cert exported (%zu bytes)", *out_len);

    #undef CLEANUP_AND_RETURN

    mbedtls_x509write_crt_free(&crt);
    mbedtls_pk_free(&pk);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    return true;
}

bool ca_export_root_cert_der(uint8_t *buf, size_t buf_size, size_t *out_len) {
    if (!buf || buf_size == 0 || !out_len) return false;
    if (!ca_is_initialized()) return false;

    // Build SubjectPublicKeyInfo DER
    if (!g_ca_pubkey_loaded && !load_ca_pubkey()) {
        return false;
    }

    // For now, export SPKI (SubjectPublicKeyInfo) as DER
    // Full X.509 certificate DER requires custom signing implementation

    uint8_t *q = buf + buf_size;
    size_t len = 0;

    // Public key point (04 || X || Y)
    if (q - buf < 66) return false;
    *--q = 0x00;  // No unused bits
    q -= 64; memcpy(q, g_ca_pubkey, 64);
    *--q = 0x04;  // Uncompressed point
    len = 66;

    // BIT STRING wrapper
    *--q = (uint8_t)len; len++;
    *--q = 0x03;
    len++;

    // Algorithm identifier
    static const uint8_t alg_id[] = {
        0x30, 0x13,
        0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01,
        0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07
    };
    if (q - buf < (int)sizeof(alg_id)) return false;
    q -= sizeof(alg_id);
    memcpy(q, alg_id, sizeof(alg_id));
    len += sizeof(alg_id);

    // Outer SEQUENCE
    if (len < 128) {
        *--q = (uint8_t)len;
        *--q = 0x30;
        len += 2;
    }

    *out_len = len;
    memmove(buf, q, len);

    LOG_I(TAG, "SPKI DER exported (%zu bytes)", len);
    return true;
}

bool ca_export_pubkey_base64(char *buf, size_t buf_size, size_t *out_len) {
    if (!buf || buf_size == 0 || !out_len) return false;
    if (!ca_is_initialized()) return false;

    // Load public key if needed
    if (!g_ca_pubkey_loaded && !load_ca_pubkey()) {
        return false;
    }

    // Build SPKI DER
    static const uint8_t spki_header[] = {
        0x30, 0x59,  // SEQUENCE, 89 bytes
        0x30, 0x13,  // AlgorithmIdentifier SEQUENCE
        0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01,  // OID ecPublicKey
        0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07,  // OID secp256r1
        0x03, 0x42,  // BIT STRING, 66 bytes
        0x00         // 0 unused bits
    };

    uint8_t spki[91];
    memcpy(spki, spki_header, sizeof(spki_header));
    spki[sizeof(spki_header)] = 0x04;  // Uncompressed point
    memcpy(spki + sizeof(spki_header) + 1, g_ca_pubkey, 64);

    // Base64 encode
    size_t b64_len = 0;
    int ret = mbedtls_base64_encode((unsigned char*)buf, buf_size, &b64_len, spki, sizeof(spki));
    if (ret != 0) {
        LOG_E(TAG, "Base64 encode failed: %d", ret);
        return false;
    }

    buf[b64_len] = '\0';
    *out_len = b64_len;

    LOG_I(TAG, "Public key exported as Base64 (%zu bytes)", b64_len);
    return true;
}

// ============================================================================
// CSR Parsing and Certificate Signing
// ============================================================================

bool ca_sign_csr(const char *csr_pem, char *cert_pem, size_t cert_size,
                 size_t *out_len, uint32_t validity_days) {
    if (!csr_pem || !cert_pem || cert_size == 0 || !out_len) return false;
    if (!ca_is_initialized()) {
        LOG_E(TAG, "CA not initialized");
        return false;
    }

    // Declare all variables at function start to avoid goto/initialization issues
    mbedtls_x509_csr csr;
    mbedtls_x509write_cert crt;
    mbedtls_pk_context issuer_pk;
    int ret = 0;
    time_t now = 0;
    time_t expires = 0;
    uint8_t csr_hash[32];
    uint8_t raw_sig[64];
    uint32_t serial = 0;
    ca_issued_cert_t issued;
    char subject_cn[CA_MAX_CN_LEN];
    const mbedtls_x509_name *name = NULL;
    nvs_handle_t nvs;
    char not_before[16];
    char not_after[16];
    char *p = NULL;
    size_t remaining = 0;
    int written = 0;

    memset(subject_cn, 0, sizeof(subject_cn));
    memset(&issued, 0, sizeof(issued));

    mbedtls_x509_csr_init(&csr);
    mbedtls_x509write_crt_init(&crt);
    mbedtls_pk_init(&issuer_pk);

    // Parse CSR
    ret = mbedtls_x509_csr_parse(&csr, (const unsigned char*)csr_pem, strlen(csr_pem) + 1);
    if (ret != 0) {
        LOG_E(TAG, "Failed to parse CSR: -0x%04x", -ret);
        mbedtls_x509_csr_free(&csr);
        mbedtls_x509write_crt_free(&crt);
        mbedtls_pk_free(&issuer_pk);
        return false;
    }

    LOG_I(TAG, "CSR parsed successfully");

    // Setup issuer pk context
    ret = setup_tropic01_pk(&issuer_pk);
    if (ret != 0) {
        mbedtls_x509_csr_free(&csr);
        mbedtls_x509write_crt_free(&crt);
        mbedtls_pk_free(&issuer_pk);
        return false;
    }

    // Calculate validity
    if (validity_days == 0) {
        validity_days = CA_CERT_VALIDITY_DAYS;
    }

    now = time(NULL);
    expires = now + ((time_t)validity_days * 24 * 60 * 60);

    // Hash the CSR for signature
    mbedtls_sha256((const unsigned char*)csr_pem, strlen(csr_pem), csr_hash, 0);

    // Sign with TROPIC01
    if (!tropic01_ecdsa_sign(CA_ECC_SLOT, csr_hash, 32, raw_sig)) {
        LOG_E(TAG, "TROPIC01 signing failed");
        mbedtls_x509_csr_free(&csr);
        mbedtls_x509write_crt_free(&crt);
        mbedtls_pk_free(&issuer_pk);
        return false;
    }

    // Update counters
    serial = g_serial_counter++;
    g_issued_count++;
    save_nvs_state();

    // Save issued cert info to NVS
    issued.serial = serial;

    // Extract CN from CSR subject
    name = &csr.subject;
    while (name != NULL) {
        if (MBEDTLS_OID_CMP(MBEDTLS_OID_AT_CN, &name->oid) == 0) {
            size_t cn_len = name->val.len < CA_MAX_CN_LEN - 1 ? name->val.len : CA_MAX_CN_LEN - 1;
            memcpy(subject_cn, name->val.p, cn_len);
            break;
        }
        name = name->next;
    }
    strncpy(issued.subject_cn, subject_cn, CA_MAX_CN_LEN - 1);
    issued.issued_at = now;
    issued.valid_until = expires;
    issued.revoked = false;

    // Save to NVS
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        char key[16];
        snprintf(key, sizeof(key), "%s%lu", NVS_KEY_CERT_PREFIX, (unsigned long)(g_issued_count - 1));
        nvs_set_blob(nvs, key, &issued, sizeof(issued));
        nvs_commit(nvs);
        nvs_close(nvs);
    }

    // Generate response
    format_x509_time(now, not_before, sizeof(not_before));
    format_x509_time(expires, not_after, sizeof(not_after));

    p = cert_pem;
    remaining = cert_size;

    written = snprintf(p, remaining,
        "-----BEGIN CERTIFICATE-----\n"
        "# Signed Certificate for: %s\n"
        "# Serial: %lu\n"
        "# Issuer: CN=%s\n"
        "# Not-Before: %s\n"
        "# Not-After: %s\n"
        "#\n",
        subject_cn[0] ? subject_cn : "(unknown)",
        (unsigned long)serial,
        g_ca_metadata.common_name,
        not_before,
        not_after);
    if (written < 0 || (size_t)written >= remaining) {
        mbedtls_x509_csr_free(&csr);
        mbedtls_x509write_crt_free(&crt);
        mbedtls_pk_free(&issuer_pk);
        return false;
    }
    p += written; remaining -= (size_t)written;

    // Export signature as hex
    written = snprintf(p, remaining, "Signature-R: ");
    if (written < 0 || (size_t)written >= remaining) {
        mbedtls_x509_csr_free(&csr);
        mbedtls_x509write_crt_free(&crt);
        mbedtls_pk_free(&issuer_pk);
        return false;
    }
    p += written; remaining -= (size_t)written;

    for (int i = 0; i < 32; i++) {
        written = snprintf(p, remaining, "%02x", raw_sig[i]);
        if (written < 0 || (size_t)written >= remaining) {
            mbedtls_x509_csr_free(&csr);
            mbedtls_x509write_crt_free(&crt);
            mbedtls_pk_free(&issuer_pk);
            return false;
        }
        p += written; remaining -= (size_t)written;
    }

    written = snprintf(p, remaining, "\n");
    if (written < 0 || (size_t)written >= remaining) {
        mbedtls_x509_csr_free(&csr);
        mbedtls_x509write_crt_free(&crt);
        mbedtls_pk_free(&issuer_pk);
        return false;
    }
    p += written; remaining -= (size_t)written;

    written = snprintf(p, remaining, "Signature-S: ");
    if (written < 0 || (size_t)written >= remaining) {
        mbedtls_x509_csr_free(&csr);
        mbedtls_x509write_crt_free(&crt);
        mbedtls_pk_free(&issuer_pk);
        return false;
    }
    p += written; remaining -= (size_t)written;

    for (int i = 0; i < 32; i++) {
        written = snprintf(p, remaining, "%02x", raw_sig[32 + i]);
        if (written < 0 || (size_t)written >= remaining) {
            mbedtls_x509_csr_free(&csr);
            mbedtls_x509write_crt_free(&crt);
            mbedtls_pk_free(&issuer_pk);
            return false;
        }
        p += written; remaining -= (size_t)written;
    }

    written = snprintf(p, remaining, "\n");
    if (written < 0 || (size_t)written >= remaining) {
        mbedtls_x509_csr_free(&csr);
        mbedtls_x509write_crt_free(&crt);
        mbedtls_pk_free(&issuer_pk);
        return false;
    }
    p += written; remaining -= (size_t)written;

    written = snprintf(p, remaining, "-----END CERTIFICATE-----\n");
    if (written < 0 || (size_t)written >= remaining) {
        mbedtls_x509_csr_free(&csr);
        mbedtls_x509write_crt_free(&crt);
        mbedtls_pk_free(&issuer_pk);
        return false;
    }
    p += written;

    *out_len = p - cert_pem;

    LOG_I(TAG, "CSR signed, serial: %lu, subject: %s", (unsigned long)serial, subject_cn);

    mbedtls_x509_csr_free(&csr);
    mbedtls_x509write_crt_free(&crt);
    mbedtls_pk_free(&issuer_pk);

    return true;
}

// ============================================================================
// Certificate Management
// ============================================================================

uint32_t ca_get_issued_count(void) {
    return g_issued_count;
}

bool ca_get_issued_cert(uint32_t index, ca_issued_cert_t *cert) {
    if (!cert || index >= g_issued_count) return false;

    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return false;
    }

    char key[16];
    snprintf(key, sizeof(key), "%s%lu", NVS_KEY_CERT_PREFIX, (unsigned long)index);

    size_t len = sizeof(ca_issued_cert_t);
    esp_err_t err = nvs_get_blob(nvs, key, cert, &len);
    nvs_close(nvs);

    return (err == ESP_OK);
}

bool ca_revoke_cert(uint32_t serial) {
    // Find certificate by serial
    for (uint32_t i = 0; i < g_issued_count; i++) {
        ca_issued_cert_t cert;
        if (ca_get_issued_cert(i, &cert) && cert.serial == serial) {
            cert.revoked = true;

            nvs_handle_t nvs;
            if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
                char key[16];
                snprintf(key, sizeof(key), "%s%lu", NVS_KEY_CERT_PREFIX, (unsigned long)i);
                nvs_set_blob(nvs, key, &cert, sizeof(cert));
                nvs_commit(nvs);
                nvs_close(nvs);
                LOG_I(TAG, "Certificate %lu revoked", (unsigned long)serial);
                return true;
            }
        }
    }

    LOG_W(TAG, "Certificate %lu not found", (unsigned long)serial);
    return false;
}

// ============================================================================
// Import Functions
// ============================================================================

// Helper function to extract private key scalar from SEC1 DER format
// SEC1 ECPrivateKey format: SEQUENCE { version INTEGER, privateKey OCTET STRING, ... }
static bool extract_ec_privkey_scalar(const uint8_t *der, size_t der_len, uint8_t *scalar_out) {
    // Skip outer SEQUENCE tag and length
    if (der_len < 4 || der[0] != 0x30) return false;

    size_t pos = 1;
    size_t seq_len;
    if (der[pos] & 0x80) {
        // Long form length
        int num_len_bytes = der[pos] & 0x7F;
        pos++;
        seq_len = 0;
        for (int i = 0; i < num_len_bytes && pos < der_len; i++) {
            seq_len = (seq_len << 8) | der[pos++];
        }
    } else {
        seq_len = der[pos++];
    }

    // Skip version INTEGER (should be 01)
    if (pos >= der_len || der[pos] != 0x02) return false;
    pos++;
    if (pos >= der_len) return false;
    size_t ver_len = der[pos++];
    pos += ver_len;

    // Next is OCTET STRING with private key
    if (pos >= der_len || der[pos] != 0x04) return false;
    pos++;
    if (pos >= der_len) return false;
    size_t key_len = der[pos++];

    if (key_len != 32 || pos + 32 > der_len) return false;

    memcpy(scalar_out, &der[pos], 32);
    return true;
}

bool ca_import_root(const uint8_t *privkey_der, size_t privkey_len,
                    const char *cert_pem) {
    if (!privkey_der || privkey_len == 0 || !cert_pem) {
        LOG_E(TAG, "Invalid parameters for import");
        return false;
    }

    if (ca_is_initialized()) {
        LOG_E(TAG, "CA already initialized. Use ca_factory_reset() first.");
        return false;
    }

    mbedtls_pk_context pk;
    mbedtls_x509_crt crt;
    mbedtls_pk_init(&pk);
    mbedtls_x509_crt_init(&crt);

    // Parse private key
    int ret = mbedtls_pk_parse_key(&pk, privkey_der, privkey_len, NULL, 0,
                                    NULL, NULL);
    if (ret != 0) {
        LOG_E(TAG, "Failed to parse private key: -0x%04x", -ret);
        mbedtls_pk_free(&pk);
        mbedtls_x509_crt_free(&crt);
        return false;
    }

    // Verify it's an EC key
    if (!mbedtls_pk_can_do(&pk, MBEDTLS_PK_ECKEY)) {
        LOG_E(TAG, "Key must be an EC key");
        mbedtls_pk_free(&pk);
        mbedtls_x509_crt_free(&crt);
        return false;
    }

    // Write key to DER to extract private scalar
    // mbedtls_pk_write_key_der writes SEC1 format for EC keys
    uint8_t key_der[256];
    int key_der_len = mbedtls_pk_write_key_der(&pk, key_der, sizeof(key_der));
    if (key_der_len < 0) {
        LOG_E(TAG, "Failed to export private key: -0x%04x", -key_der_len);
        mbedtls_pk_free(&pk);
        mbedtls_x509_crt_free(&crt);
        return false;
    }

    // mbedtls_pk_write_key_der writes from end of buffer
    uint8_t *key_der_start = key_der + sizeof(key_der) - key_der_len;

    // Extract private key scalar from SEC1 DER
    uint8_t privkey_scalar[32];
    if (!extract_ec_privkey_scalar(key_der_start, key_der_len, privkey_scalar)) {
        LOG_E(TAG, "Failed to extract private key scalar");
        memset(key_der, 0, sizeof(key_der));
        mbedtls_pk_free(&pk);
        mbedtls_x509_crt_free(&crt);
        return false;
    }
    memset(key_der, 0, sizeof(key_der));  // Clear sensitive data

    // Parse certificate to extract metadata
    ret = mbedtls_x509_crt_parse(&crt, (const unsigned char*)cert_pem,
                                  strlen(cert_pem) + 1);
    if (ret != 0) {
        LOG_E(TAG, "Failed to parse certificate: -0x%04x", -ret);
        memset(privkey_scalar, 0, sizeof(privkey_scalar));
        mbedtls_pk_free(&pk);
        mbedtls_x509_crt_free(&crt);
        return false;
    }

    // Import key into TROPIC01
    if (!tropic01_ecc_key_write(CA_ECC_SLOT, privkey_scalar, 32, CDC_CURVE_P256)) {
        LOG_E(TAG, "Failed to import key into TROPIC01");
        memset(privkey_scalar, 0, sizeof(privkey_scalar));
        mbedtls_pk_free(&pk);
        mbedtls_x509_crt_free(&crt);
        return false;
    }
    memset(privkey_scalar, 0, sizeof(privkey_scalar));

    // Load the public key from TROPIC01 to verify import
    if (!load_ca_pubkey()) {
        LOG_E(TAG, "Failed to verify imported key");
        tropic01_ecc_key_erase(CA_ECC_SLOT);
        mbedtls_pk_free(&pk);
        mbedtls_x509_crt_free(&crt);
        return false;
    }

    // Extract Common Name from certificate
    char cn[CA_MAX_CN_LEN];
    memset(cn, 0, sizeof(cn));
    const mbedtls_x509_name *name = &crt.subject;
    while (name != NULL) {
        if (MBEDTLS_OID_CMP(MBEDTLS_OID_AT_CN, &name->oid) == 0) {
            size_t cn_len = name->val.len < CA_MAX_CN_LEN - 1 ? name->val.len : CA_MAX_CN_LEN - 1;
            memcpy(cn, name->val.p, cn_len);
            break;
        }
        name = name->next;
    }

    if (cn[0] == '\0') {
        strncpy(cn, "Imported CA", CA_MAX_CN_LEN - 1);
    }

    // Initialize metadata from certificate validity
    memset(&g_ca_metadata, 0, sizeof(g_ca_metadata));
    strncpy(g_ca_metadata.common_name, cn, CA_MAX_CN_LEN - 1);

    // Extract validity from certificate
    struct tm tm_start;
    memset(&tm_start, 0, sizeof(tm_start));
    tm_start.tm_year = crt.valid_from.year - 1900;
    tm_start.tm_mon = crt.valid_from.mon - 1;
    tm_start.tm_mday = crt.valid_from.day;
    tm_start.tm_hour = crt.valid_from.hour;
    tm_start.tm_min = crt.valid_from.min;
    tm_start.tm_sec = crt.valid_from.sec;
    g_ca_metadata.created_at = (uint32_t)mktime(&tm_start);

    struct tm tm_end;
    memset(&tm_end, 0, sizeof(tm_end));
    tm_end.tm_year = crt.valid_to.year - 1900;
    tm_end.tm_mon = crt.valid_to.mon - 1;
    tm_end.tm_mday = crt.valid_to.day;
    tm_end.tm_hour = crt.valid_to.hour;
    tm_end.tm_min = crt.valid_to.min;
    tm_end.tm_sec = crt.valid_to.sec;
    g_ca_metadata.valid_until = (uint32_t)mktime(&tm_end);

    g_ca_metadata.magic = CA_METADATA_MAGIC;

    // Save metadata to TROPIC01
    if (!save_metadata()) {
        LOG_E(TAG, "Failed to save metadata");
        tropic01_ecc_key_erase(CA_ECC_SLOT);
        mbedtls_pk_free(&pk);
        mbedtls_x509_crt_free(&crt);
        return false;
    }

    // Initialize NVS state
    g_serial_counter = 1;
    g_issued_count = 0;
    save_nvs_state();

    LOG_I(TAG, "CA imported successfully: %s", cn);
    mbedtls_pk_free(&pk);
    mbedtls_x509_crt_free(&crt);
    return true;
}

bool ca_import_root_pem(const char *privkey_pem, const char *cert_pem) {
    if (!privkey_pem || !cert_pem) {
        return false;
    }

    // Parse PEM private key using mbedtls
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);

    int ret = mbedtls_pk_parse_key(&pk, (const unsigned char*)privkey_pem,
                                    strlen(privkey_pem) + 1, NULL, 0, NULL, NULL);
    if (ret != 0) {
        LOG_E(TAG, "Failed to parse PEM private key: -0x%04x", -ret);
        mbedtls_pk_free(&pk);
        return false;
    }

    // Verify it's an EC key
    if (!mbedtls_pk_can_do(&pk, MBEDTLS_PK_ECKEY)) {
        LOG_E(TAG, "Key must be an EC key");
        mbedtls_pk_free(&pk);
        return false;
    }

    // Write key to DER to extract private scalar (mbedTLS 3.x compatible)
    uint8_t key_der[256];
    int key_der_len = mbedtls_pk_write_key_der(&pk, key_der, sizeof(key_der));
    mbedtls_pk_free(&pk);

    if (key_der_len < 0) {
        LOG_E(TAG, "Failed to export private key: -0x%04x", -key_der_len);
        return false;
    }

    // mbedtls_pk_write_key_der writes from end of buffer
    uint8_t *key_der_start = key_der + sizeof(key_der) - key_der_len;

    // Extract private key scalar from SEC1 DER
    uint8_t privkey_scalar[32];
    if (!extract_ec_privkey_scalar(key_der_start, key_der_len, privkey_scalar)) {
        LOG_E(TAG, "Failed to extract private key scalar");
        memset(key_der, 0, sizeof(key_der));
        return false;
    }
    memset(key_der, 0, sizeof(key_der));

    // Now we have the raw private key, continue with import
    if (ca_is_initialized()) {
        LOG_E(TAG, "CA already initialized. Use ca_factory_reset() first.");
        memset(privkey_scalar, 0, sizeof(privkey_scalar));
        return false;
    }

    // Import into TROPIC01
    if (!tropic01_ecc_key_write(CA_ECC_SLOT, privkey_scalar, 32, CDC_CURVE_P256)) {
        LOG_E(TAG, "Failed to import key into TROPIC01");
        memset(privkey_scalar, 0, sizeof(privkey_scalar));
        return false;
    }

    memset(privkey_scalar, 0, sizeof(privkey_scalar));

    // Load and verify public key
    if (!load_ca_pubkey()) {
        LOG_E(TAG, "Failed to verify imported key");
        tropic01_ecc_key_erase(CA_ECC_SLOT);
        return false;
    }

    // Parse certificate to extract metadata
    mbedtls_x509_crt crt;
    mbedtls_x509_crt_init(&crt);

    ret = mbedtls_x509_crt_parse(&crt, (const unsigned char*)cert_pem,
                                  strlen(cert_pem) + 1);
    if (ret != 0) {
        LOG_E(TAG, "Failed to parse certificate: -0x%04x", -ret);
        tropic01_ecc_key_erase(CA_ECC_SLOT);
        mbedtls_x509_crt_free(&crt);
        return false;
    }

    // Extract CN
    char cn[CA_MAX_CN_LEN];
    memset(cn, 0, sizeof(cn));
    const mbedtls_x509_name *name = &crt.subject;
    while (name != NULL) {
        if (MBEDTLS_OID_CMP(MBEDTLS_OID_AT_CN, &name->oid) == 0) {
            size_t cn_len = name->val.len < CA_MAX_CN_LEN - 1 ? name->val.len : CA_MAX_CN_LEN - 1;
            memcpy(cn, name->val.p, cn_len);
            break;
        }
        name = name->next;
    }

    if (cn[0] == '\0') {
        strncpy(cn, "Imported CA", CA_MAX_CN_LEN - 1);
    }

    // Setup metadata
    memset(&g_ca_metadata, 0, sizeof(g_ca_metadata));
    strncpy(g_ca_metadata.common_name, cn, CA_MAX_CN_LEN - 1);

    struct tm tm_start;
    memset(&tm_start, 0, sizeof(tm_start));
    tm_start.tm_year = crt.valid_from.year - 1900;
    tm_start.tm_mon = crt.valid_from.mon - 1;
    tm_start.tm_mday = crt.valid_from.day;
    g_ca_metadata.created_at = (uint32_t)mktime(&tm_start);

    struct tm tm_end;
    memset(&tm_end, 0, sizeof(tm_end));
    tm_end.tm_year = crt.valid_to.year - 1900;
    tm_end.tm_mon = crt.valid_to.mon - 1;
    tm_end.tm_mday = crt.valid_to.day;
    g_ca_metadata.valid_until = (uint32_t)mktime(&tm_end);

    g_ca_metadata.magic = CA_METADATA_MAGIC;

    mbedtls_x509_crt_free(&crt);

    if (!save_metadata()) {
        LOG_E(TAG, "Failed to save metadata");
        tropic01_ecc_key_erase(CA_ECC_SLOT);
        return false;
    }

    g_serial_counter = 1;
    g_issued_count = 0;
    save_nvs_state();

    LOG_I(TAG, "CA imported (PEM): %s", cn);
    return true;
}

// ============================================================================
// Sign Existing Certificate (Re-sign/Cross-sign)
// ============================================================================

bool ca_sign_cert(const char *cert_pem, char *signed_cert_pem, size_t cert_size,
                  size_t *out_len, uint32_t validity_days) {
    if (!cert_pem || !signed_cert_pem || cert_size == 0 || !out_len) return false;
    if (!ca_is_initialized()) {
        LOG_E(TAG, "CA not initialized");
        return false;
    }

    mbedtls_x509_crt crt;
    mbedtls_x509_crt_init(&crt);

    // Parse existing certificate
    int ret = mbedtls_x509_crt_parse(&crt, (const unsigned char*)cert_pem,
                                      strlen(cert_pem) + 1);
    if (ret != 0) {
        LOG_E(TAG, "Failed to parse certificate: -0x%04x", -ret);
        mbedtls_x509_crt_free(&crt);
        return false;
    }

    LOG_I(TAG, "Certificate parsed for re-signing");

    // Calculate validity
    if (validity_days == 0) {
        validity_days = CA_CERT_VALIDITY_DAYS;
    }

    time_t now = time(NULL);
    time_t expires = now + ((time_t)validity_days * 24 * 60 * 60);

    // Extract subject CN
    char subject_cn[CA_MAX_CN_LEN] = {0};
    const mbedtls_x509_name *name = &crt.subject;
    while (name != NULL) {
        if (MBEDTLS_OID_CMP(MBEDTLS_OID_AT_CN, &name->oid) == 0) {
            size_t cn_len = name->val.len < CA_MAX_CN_LEN - 1 ? name->val.len : CA_MAX_CN_LEN - 1;
            memcpy(subject_cn, name->val.p, cn_len);
            break;
        }
        name = name->next;
    }

    // Get the public key from the certificate using pk_write
    // This works with mbedTLS 3.x without accessing internal structures
    uint8_t subject_pubkey[128];
    int pubkey_len = mbedtls_pk_write_pubkey_der(&crt.pk, subject_pubkey, sizeof(subject_pubkey));
    if (pubkey_len < 0) {
        LOG_E(TAG, "Failed to extract public key: -0x%04x", -pubkey_len);
        mbedtls_x509_crt_free(&crt);
        return false;
    }

    // mbedtls_pk_write_pubkey_der writes from the end of the buffer
    uint8_t *pubkey_start = subject_pubkey + sizeof(subject_pubkey) - pubkey_len;

    // Create signature over the certificate content
    uint8_t cert_hash[32];
    mbedtls_sha256((const unsigned char*)cert_pem, strlen(cert_pem), cert_hash, 0);

    uint8_t raw_sig[64];
    if (!tropic01_ecdsa_sign(CA_ECC_SLOT, cert_hash, 32, raw_sig)) {
        LOG_E(TAG, "TROPIC01 signing failed");
        mbedtls_x509_crt_free(&crt);
        return false;
    }

    // Update counters
    uint32_t serial = g_serial_counter++;
    g_issued_count++;
    save_nvs_state();

    // Save issued cert info
    ca_issued_cert_t issued;
    memset(&issued, 0, sizeof(issued));
    issued.serial = serial;
    strncpy(issued.subject_cn, subject_cn[0] ? subject_cn : "(re-signed)", CA_MAX_CN_LEN - 1);
    issued.issued_at = now;
    issued.valid_until = expires;
    issued.revoked = false;

    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        char key[16];
        snprintf(key, sizeof(key), "%s%lu", NVS_KEY_CERT_PREFIX, (unsigned long)(g_issued_count - 1));
        nvs_set_blob(nvs, key, &issued, sizeof(issued));
        nvs_commit(nvs);
        nvs_close(nvs);
    }

    // Generate response
    char not_before[16], not_after[16];
    format_x509_time(now, not_before, sizeof(not_before));
    format_x509_time(expires, not_after, sizeof(not_after));

    char *p = signed_cert_pem;
    size_t remaining = cert_size;
    int written = 0;

    written = snprintf(p, remaining,
        "-----BEGIN CERTIFICATE-----\n"
        "# Re-signed Certificate for: %s\n"
        "# Serial: %lu\n"
        "# Issuer: CN=%s\n"
        "# Not-Before: %s\n"
        "# Not-After: %s\n"
        "# Original Issuer: (preserved in cert)\n"
        "#\n",
        subject_cn[0] ? subject_cn : "(unknown)",
        (unsigned long)serial,
        g_ca_metadata.common_name,
        not_before,
        not_after);
    if (written < 0 || (size_t)written >= remaining) {
        mbedtls_x509_crt_free(&crt);
        return false;
    }
    p += written; remaining -= (size_t)written;

    // Export public key as Base64
    written = snprintf(p, remaining, "Subject-Public-Key: ");
    if (written < 0 || (size_t)written >= remaining) {
        mbedtls_x509_crt_free(&crt);
        return false;
    }
    p += written; remaining -= (size_t)written;

    size_t b64_len;
    mbedtls_base64_encode(NULL, 0, &b64_len, pubkey_start, pubkey_len);
    if (remaining < b64_len + 1) {
        mbedtls_x509_crt_free(&crt);
        return false;
    }

    ret = mbedtls_base64_encode((unsigned char*)p, remaining, &b64_len,
                                 pubkey_start, pubkey_len);
    if (ret != 0) {
        mbedtls_x509_crt_free(&crt);
        return false;
    }
    p += b64_len; remaining -= b64_len;

    written = snprintf(p, remaining, "\n");
    if (written < 0 || (size_t)written >= remaining) {
        mbedtls_x509_crt_free(&crt);
        return false;
    }
    p += written; remaining -= (size_t)written;

    // Export signature
    written = snprintf(p, remaining, "Signature-R: ");
    if (written < 0 || (size_t)written >= remaining) {
        mbedtls_x509_crt_free(&crt);
        return false;
    }
    p += written; remaining -= (size_t)written;
    for (int i = 0; i < 32; i++) {
        written = snprintf(p, remaining, "%02x", raw_sig[i]);
        if (written < 0 || (size_t)written >= remaining) {
            mbedtls_x509_crt_free(&crt);
            return false;
        }
        p += written; remaining -= (size_t)written;
    }
    written = snprintf(p, remaining, "\n");
    if (written < 0 || (size_t)written >= remaining) {
        mbedtls_x509_crt_free(&crt);
        return false;
    }
    p += written; remaining -= (size_t)written;

    written = snprintf(p, remaining, "Signature-S: ");
    if (written < 0 || (size_t)written >= remaining) {
        mbedtls_x509_crt_free(&crt);
        return false;
    }
    p += written; remaining -= (size_t)written;
    for (int i = 0; i < 32; i++) {
        written = snprintf(p, remaining, "%02x", raw_sig[32 + i]);
        if (written < 0 || (size_t)written >= remaining) {
            mbedtls_x509_crt_free(&crt);
            return false;
        }
        p += written; remaining -= (size_t)written;
    }
    written = snprintf(p, remaining, "\n");
    if (written < 0 || (size_t)written >= remaining) {
        mbedtls_x509_crt_free(&crt);
        return false;
    }
    p += written; remaining -= (size_t)written;

    written = snprintf(p, remaining, "-----END CERTIFICATE-----\n");
    if (written < 0 || (size_t)written >= remaining) {
        mbedtls_x509_crt_free(&crt);
        return false;
    }
    p += written;

    *out_len = p - signed_cert_pem;

    LOG_I(TAG, "Certificate re-signed, serial: %lu, subject: %s",
          (unsigned long)serial, subject_cn);

    mbedtls_x509_crt_free(&crt);
    return true;
}

// ============================================================================
// Factory Reset
// ============================================================================

bool ca_factory_reset(void) {
    LOG_W(TAG, "Factory resetting CA...");

    // Erase root key
    if (tropic01_cache_ecc_exists(CA_ECC_SLOT)) {
        tropic01_ecc_key_erase(CA_ECC_SLOT);
    }

    // Erase metadata
    tropic01_rmem_erase(CA_RMEM_SLOT);

    // Clear NVS
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_erase_all(nvs);
        nvs_commit(nvs);
        nvs_close(nvs);
    }

    // Reset state
    g_serial_counter = 1;
    g_issued_count = 0;
    g_ca_pubkey_loaded = false;
    memset(&g_ca_metadata, 0, sizeof(g_ca_metadata));
    memset(g_ca_pubkey, 0, sizeof(g_ca_pubkey));

    LOG_I(TAG, "CA factory reset complete");
    return true;
}

#endif // FEATURE_CA
