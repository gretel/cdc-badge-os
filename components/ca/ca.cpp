// Certificate Authority (CA) Implementation
// Uses TROPIC01 for secure key storage and signing

#include "ca.h"

#if FEATURE_CA

#include "cdc_log.h"
#include "tropic01.h"
#include "tropic01_cache.h"

#include "nvs_flash.h"
#include "nvs.h"

#include "mbedtls/sha256.h"
#include "mbedtls/base64.h"

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

// ============================================================================
// Forward Declarations
// ============================================================================

static bool load_metadata(void);
static bool save_metadata(void);
static bool load_nvs_state(void);
static bool save_nvs_state(void);

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

    g_ca_loaded = true;
    LOG_I(TAG, "CA initialized: %s", g_ca_metadata.common_name);
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

// ============================================================================
// CA Setup
// ============================================================================

bool ca_setup(const char *cn) {
    if (!cn || strlen(cn) == 0) {
        LOG_E(TAG, "Common Name required");
        return false;
    }

    if (ca_is_initialized()) {
        LOG_E(TAG, "CA already initialized. Use ca_factory_reset() first.");
        return false;
    }

    LOG_I(TAG, "Setting up CA with CN: %s", cn);

    // Generate P-256 key in TROPIC01 slot 31
    if (!tropic01_ecc_key_generate(CA_ECC_SLOT, CDC_CURVE_P256)) {
        LOG_E(TAG, "Failed to generate root key");
        return false;
    }

    // Initialize metadata
    memset(&g_ca_metadata, 0, sizeof(g_ca_metadata));
    strncpy(g_ca_metadata.common_name, cn, CA_MAX_CN_LEN - 1);

    time_t now = time(NULL);
    g_ca_metadata.created_at = (uint32_t)now;
    g_ca_metadata.valid_until = (uint32_t)(now + (CA_CERT_VALIDITY_DAYS * 24 * 60 * 60));
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
// Certificate Export
// ============================================================================

bool ca_export_root_cert_pem(char *buf, size_t buf_size, size_t *out_len) {
    if (!buf || buf_size == 0 || !out_len) return false;
    if (!ca_is_initialized()) return false;

    // Get public key from TROPIC01
    uint8_t pubkey[64];
    uint8_t curve, origin;
    if (!tropic01_ecc_key_read(CA_ECC_SLOT, pubkey, sizeof(pubkey), &curve, &origin)) {
        LOG_E(TAG, "Failed to read public key");
        return false;
    }

    // Export public key info as a simplified "certificate"
    // Note: This is not a proper X.509 certificate - just key info for now
    // Full certificate generation requires more complex mbedTLS integration

    char *p = buf;
    size_t remaining = buf_size;
    int written = 0;

    written = snprintf(p, remaining, "-----BEGIN CA INFO-----\n");
    if (written < 0 || (size_t)written >= remaining) return false;
    p += written; remaining -= (size_t)written;

    written = snprintf(p, remaining, "CN: %s\n", g_ca_metadata.common_name);
    if (written < 0 || (size_t)written >= remaining) return false;
    p += written; remaining -= (size_t)written;

    written = snprintf(p, remaining, "Created: %lu\n", (unsigned long)g_ca_metadata.created_at);
    if (written < 0 || (size_t)written >= remaining) return false;
    p += written; remaining -= (size_t)written;

    written = snprintf(p, remaining, "Valid Until: %lu\n", (unsigned long)g_ca_metadata.valid_until);
    if (written < 0 || (size_t)written >= remaining) return false;
    p += written; remaining -= (size_t)written;

    written = snprintf(p, remaining, "Curve: P-256\n");
    if (written < 0 || (size_t)written >= remaining) return false;
    p += written; remaining -= (size_t)written;

    written = snprintf(p, remaining, "Public Key (hex): ");
    if (written < 0 || (size_t)written >= remaining) return false;
    p += written; remaining -= (size_t)written;

    for (int i = 0; i < 64; i++) {
        written = snprintf(p, remaining, "%02x", pubkey[i]);
        if (written < 0 || (size_t)written >= remaining) return false;
        p += written; remaining -= (size_t)written;
    }
    written = snprintf(p, remaining, "\n");
    if (written < 0 || (size_t)written >= remaining) return false;
    p += written; remaining -= (size_t)written;

    written = snprintf(p, remaining, "-----END CA INFO-----\n");
    if (written < 0 || (size_t)written >= remaining) return false;
    p += written;

    *out_len = p - buf;

    LOG_I(TAG, "CA info exported (%zu bytes)", *out_len);
    return true;
}

bool ca_export_root_cert_der(uint8_t *buf, size_t buf_size, size_t *out_len) {
    // Not implemented - would need full X.509 DER generation
    (void)buf;
    (void)buf_size;
    (void)out_len;
    LOG_W(TAG, "DER export not implemented");
    return false;
}

// ============================================================================
// CSR Signing (Simplified)
// ============================================================================

bool ca_sign_csr(const char *csr_pem, char *cert_pem, size_t cert_size,
                 size_t *out_len, uint32_t validity_days) {
    if (!csr_pem || !cert_pem || cert_size == 0 || !out_len) return false;
    if (!ca_is_initialized()) {
        LOG_E(TAG, "CA not initialized");
        return false;
    }

    (void)validity_days;

    // Simplified: just acknowledge the CSR and return a placeholder
    // Full CSR signing requires mbedTLS X.509 write support with custom PK callback

    // For now, compute a hash of the CSR as "signature"
    uint8_t csr_hash[32];
    mbedtls_sha256((const unsigned char*)csr_pem, strlen(csr_pem), csr_hash, 0);

    // Sign the hash with TROPIC01
    uint8_t signature[64];
    if (!tropic01_ecdsa_sign(CA_ECC_SLOT, csr_hash, 32, signature)) {
        LOG_E(TAG, "TROPIC01 signing failed");
        return false;
    }

    // Update counters
    g_serial_counter++;
    g_issued_count++;
    save_nvs_state();

    // Return signed acknowledgment
    char *p = cert_pem;
    size_t remaining = cert_size;
    int written = 0;

    written = snprintf(p, remaining, "-----BEGIN SIGNED RESPONSE-----\n");
    if (written < 0 || (size_t)written >= remaining) return false;
    p += written; remaining -= (size_t)written;

    written = snprintf(p, remaining, "Serial: %lu\n", (unsigned long)g_serial_counter);
    if (written < 0 || (size_t)written >= remaining) return false;
    p += written; remaining -= (size_t)written;

    written = snprintf(p, remaining, "CA: %s\n", g_ca_metadata.common_name);
    if (written < 0 || (size_t)written >= remaining) return false;
    p += written; remaining -= (size_t)written;

    written = snprintf(p, remaining, "CSR-Hash: ");
    if (written < 0 || (size_t)written >= remaining) return false;
    p += written; remaining -= (size_t)written;
    for (int i = 0; i < 32; i++) {
        written = snprintf(p, remaining, "%02x", csr_hash[i]);
        if (written < 0 || (size_t)written >= remaining) return false;
        p += written; remaining -= (size_t)written;
    }
    written = snprintf(p, remaining, "\n");
    if (written < 0 || (size_t)written >= remaining) return false;
    p += written; remaining -= (size_t)written;

    written = snprintf(p, remaining, "Signature: ");
    if (written < 0 || (size_t)written >= remaining) return false;
    p += written; remaining -= (size_t)written;
    for (int i = 0; i < 64; i++) {
        written = snprintf(p, remaining, "%02x", signature[i]);
        if (written < 0 || (size_t)written >= remaining) return false;
        p += written; remaining -= (size_t)written;
    }
    written = snprintf(p, remaining, "\n");
    if (written < 0 || (size_t)written >= remaining) return false;
    p += written; remaining -= (size_t)written;

    written = snprintf(p, remaining, "-----END SIGNED RESPONSE-----\n");
    if (written < 0 || (size_t)written >= remaining) return false;
    p += written; remaining -= (size_t)written;

    written = snprintf(p, remaining, "\nNote: Full X.509 certificate generation not yet implemented.\n");
    if (written < 0 || (size_t)written >= remaining) return false;
    p += written; remaining -= (size_t)written;

    written = snprintf(p, remaining, "The signature above proves the CSR was received and signed by the CA.\n");
    if (written < 0 || (size_t)written >= remaining) return false;
    p += written;

    *out_len = p - cert_pem;

    LOG_I(TAG, "CSR signed, serial: %lu", (unsigned long)g_serial_counter);
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
    memset(&g_ca_metadata, 0, sizeof(g_ca_metadata));

    LOG_I(TAG, "CA factory reset complete");
    return true;
}

#endif // FEATURE_CA
