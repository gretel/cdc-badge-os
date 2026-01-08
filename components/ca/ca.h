// Certificate Authority (CA) Module for CDC Badge
// Uses TROPIC01 ECC Slot 31 for root key
// Stores metadata in R-Memory Slot 133
// Issued certificates tracked in NVS

#ifndef CDC_CA_H
#define CDC_CA_H

#include "feature_flags.h"

#if FEATURE_CA

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Constants
// ============================================================================

#define CA_ECC_SLOT             31      // TROPIC01 ECC slot for CA root key
#define CA_RMEM_SLOT            133     // R-Memory slot for CA metadata
#define CA_MAX_CN_LEN           64      // Max Common Name length
#define CA_MAX_ISSUED_CERTS     32      // Max tracked issued certificates
#define CA_CERT_VALIDITY_DAYS   3650    // 10 years default validity

// ============================================================================
// Data Structures
// ============================================================================

typedef struct {
    bool initialized;           // CA has been initialized
    char common_name[CA_MAX_CN_LEN];
    uint32_t serial_counter;    // Next certificate serial number
    uint32_t issued_count;      // Number of certificates issued
    time_t created_at;          // CA creation timestamp
    time_t valid_until;         // CA certificate validity end
} ca_status_t;

typedef struct {
    uint32_t serial;
    char subject_cn[CA_MAX_CN_LEN];
    time_t issued_at;
    time_t valid_until;
    bool revoked;
} ca_issued_cert_t;

// ============================================================================
// Initialization
// ============================================================================

// Initialize CA module (load state from storage)
bool ca_init(void);

// Check if CA is initialized with root key
bool ca_is_initialized(void);

// Get CA status
bool ca_get_status(ca_status_t *status);

// ============================================================================
// CA Setup
// ============================================================================

// Initialize CA: generate root key and self-signed certificate
// cn: Common Name for the CA (e.g., "CDC Badge Root CA")
// Returns true on success
bool ca_setup(const char *cn);

// Export root CA certificate in PEM format
// buf: output buffer
// buf_size: buffer size
// out_len: actual length written
bool ca_export_root_cert_pem(char *buf, size_t buf_size, size_t *out_len);

// Export root CA certificate in DER format
bool ca_export_root_cert_der(uint8_t *buf, size_t buf_size, size_t *out_len);

// ============================================================================
// Certificate Signing
// ============================================================================

// Sign a Certificate Signing Request (CSR)
// csr_pem: CSR in PEM format
// cert_pem: output buffer for signed certificate (PEM)
// cert_size: buffer size
// out_len: actual length written
// validity_days: certificate validity (0 = default CA_CERT_VALIDITY_DAYS)
bool ca_sign_csr(const char *csr_pem, char *cert_pem, size_t cert_size,
                 size_t *out_len, uint32_t validity_days);

// ============================================================================
// Certificate Management
// ============================================================================

// List issued certificates
uint32_t ca_get_issued_count(void);

// Get info about issued certificate by index
bool ca_get_issued_cert(uint32_t index, ca_issued_cert_t *cert);

// Revoke certificate by serial number
bool ca_revoke_cert(uint32_t serial);

// ============================================================================
// Cleanup
// ============================================================================

// Factory reset CA (erases root key and all certificates)
// DANGEROUS: Irreversible!
bool ca_factory_reset(void);

#ifdef __cplusplus
}
#endif

#endif // FEATURE_CA

#endif // CDC_CA_H
