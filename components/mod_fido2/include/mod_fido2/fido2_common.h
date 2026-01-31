// FIDO2 Common Helpers
// Shared utility functions used across the FIDO2 module

#ifndef FIDO2_COMMON_H
#define FIDO2_COMMON_H

#include <cdc_hal/ISecureElement.h>
#include <mbedtls/sha256.h>
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace cdc {
namespace mod_fido2 {

// Get secure element instance (inline to avoid linkage issues)
inline hal::ISecureElement* get_se() {
    return hal::getSecureElementInstance();
}

// Compute SHA-256 hash
inline void sha256(const uint8_t* data, size_t len, uint8_t out[32]) {
    mbedtls_sha256(data, len, out, 0);
}

// Compute SHA-256 of null-terminated string
inline void sha256_str(const char* str, uint8_t out[32]) {
    sha256(reinterpret_cast<const uint8_t*>(str), std::strlen(str), out);
}

} // namespace mod_fido2
} // namespace cdc

#endif // FIDO2_COMMON_H
