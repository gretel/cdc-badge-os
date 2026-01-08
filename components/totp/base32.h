// Base32 Decoder (RFC 4648)

#ifndef BASE32_H
#define BASE32_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Decode a Base32 encoded string into bytes.
 * Supports both uppercase and lowercase input.
 * Ignores spaces and dashes for convenience (common in TOTP secrets).
 *
 * @param encoded Base32 encoded string (null-terminated)
 * @param out Output buffer for decoded bytes
 * @param outMax Maximum size of output buffer
 * @return Number of decoded bytes, or -1 on error
 */
int base32_decode(const char *encoded, uint8_t *out, size_t outMax);

/**
 * Calculate the decoded size for a Base32 string.
 *
 * @param encoded Base32 encoded string (null-terminated)
 * @return Expected decoded size in bytes
 */
size_t base32_decoded_size(const char *encoded);

/**
 * Encode binary data to Base32 string.
 *
 * @param data Input binary data
 * @param dataLen Length of input data
 * @param out Output buffer for Base32 string (null-terminated)
 * @param outMax Maximum size of output buffer (including null terminator)
 * @return Number of characters written (excluding null), or -1 on error
 */
int base32_encode(const uint8_t *data, size_t dataLen, char *out, size_t outMax);

#ifdef __cplusplus
}
#endif

#endif // BASE32_H
