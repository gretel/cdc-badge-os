// FIDO2 Common Helpers
// Shared utility functions used across the FIDO2 module

#ifndef FIDO2_COMMON_H
#define FIDO2_COMMON_H

#include <cdc_hal/ISecureElement.h>
#include <mbedtls/sha256.h>
#include <cstdint>
#include <cstddef>
#include <cstring>

// ============================================================================
// FIDO2 cryptographic component sizes (NIST P-256 / Ed25519)
//   - NIST P-256 (secp256r1): 32-byte R and S components, 64-byte raw R||S
//   - Ed25519: 64-byte signature, 32-byte public key
//   - SHA-256: 32-byte digest
// ============================================================================
#define FIDO2_SHA256_DIGEST_SIZE        32
#define FIDO2_PUBKEY_COMPONENT_SIZE     32   // Size of single coordinate (X or Y)
#define FIDO2_PRIVKEY_SIZE              32   // P-256 / Ed25519 private key size
#define FIDO2_SIG_COMPONENT_SIZE        32   // Size of single ECDSA component (R or S)
#define FIDO2_SIG_SIZE                  64   // Raw ECDSA P-256 (R||S) and Ed25519 signature size
#define FIDO2_P256_UNCOMPRESSED_SIZE    65   // 0x04 || X || Y
#define FIDO2_P256_PUBKEY_XY_SIZE       64   // X || Y without prefix

namespace cdc {
namespace mod_fido2 {

// Compute SHA-256 hash
inline void sha256(const uint8_t* data, size_t len, uint8_t out[FIDO2_SHA256_DIGEST_SIZE]) {
    mbedtls_sha256(data, len, out, 0);
}

// Compute SHA-256 of null-terminated string
inline void sha256_str(const char* str, uint8_t out[FIDO2_SHA256_DIGEST_SIZE]) {
    sha256(reinterpret_cast<const uint8_t*>(str), std::strlen(str), out);
}

} // namespace mod_fido2
} // namespace cdc

#endif // FIDO2_COMMON_H
