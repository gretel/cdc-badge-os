// TOTP Generator (RFC 6238)

#ifndef TOTP_H
#define TOTP_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Default TOTP parameters
#define TOTP_DEFAULT_DIGITS 6
#define TOTP_DEFAULT_PERIOD 30

// Maximum secret length (32 bytes = 256 bits)
#define TOTP_MAX_SECRET_LEN 32

// Algorithm types
typedef enum {
    TOTP_ALG_SHA1 = 0,    // Default (RFC 6238)
    TOTP_ALG_SHA256 = 1,
    TOTP_ALG_SHA512 = 2
} totp_algorithm_t;

/**
 * Generate a TOTP code for the given secret and time.
 *
 * @param secret The shared secret key (decoded from Base32)
 * @param secretLen Length of the secret in bytes
 * @param timestamp Unix timestamp (use time(NULL) for current time)
 * @param period Time step in seconds (default: 30)
 * @param digits Number of digits (6, 7, or 8)
 * @param algorithm HMAC algorithm (SHA-1, SHA-256, SHA-512)
 * @return TOTP code, or 0 on error
 */
uint32_t totp_generate(const uint8_t *secret, size_t secretLen,
                       time_t timestamp, uint32_t period, uint8_t digits,
                       totp_algorithm_t algorithm);

/**
 * Generate TOTP with default parameters (30s period, 6 digits, SHA-1).
 *
 * @param secret The shared secret key
 * @param secretLen Length of the secret in bytes
 * @return TOTP code, or 0 on error
 */
uint32_t totp_generate_default(const uint8_t *secret, size_t secretLen);

/**
 * Get remaining seconds until code changes.
 *
 * @param period Time step in seconds (default: 30)
 * @return Seconds remaining (0 to period-1)
 */
uint8_t totp_time_remaining(uint32_t period);

/**
 * Check if current time is valid for TOTP generation.
 * Time is invalid if it's before 2024 (likely not synced).
 *
 * @return true if time is valid
 */
bool totp_time_valid(void);

#ifdef __cplusplus
}
#endif

#endif // TOTP_H
