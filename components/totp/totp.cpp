// TOTP Generator (RFC 6238)

#include "totp.h"
#include "cdc_log.h"
#include <mbedtls/md.h>
#include <string.h>
#include <sys/time.h>

// HMAC output sizes
#define HMAC_SHA1_SIZE   20
#define HMAC_SHA256_SIZE 32
#define HMAC_SHA512_SIZE 64

// Powers of 10 for digit truncation
static const uint32_t POWERS_10[] = {
    1,          // 0 digits (unused)
    10,         // 1 digit
    100,        // 2 digits
    1000,       // 3 digits
    10000,      // 4 digits
    100000,     // 5 digits
    1000000,    // 6 digits
    10000000,   // 7 digits
    100000000,  // 8 digits
};

/**
 * Compute HMAC with configurable algorithm
 */
static bool hmac_compute(totp_algorithm_t algo, const uint8_t *key, size_t keyLen,
                         const uint8_t *data, size_t dataLen,
                         uint8_t *output, size_t *outputLen) {
    mbedtls_md_type_t mdType;
    size_t expectedLen;

    switch (algo) {
        case TOTP_ALG_SHA256:
            mdType = MBEDTLS_MD_SHA256;
            expectedLen = HMAC_SHA256_SIZE;
            break;
        case TOTP_ALG_SHA512:
            mdType = MBEDTLS_MD_SHA512;
            expectedLen = HMAC_SHA512_SIZE;
            break;
        default:
            mdType = MBEDTLS_MD_SHA1;
            expectedLen = HMAC_SHA1_SIZE;
            break;
    }

    const mbedtls_md_info_t *mdInfo = mbedtls_md_info_from_type(mdType);
    if (!mdInfo) {
        LOG_E("TOTP", "HMAC algorithm not available");
        return false;
    }

    int ret = mbedtls_md_hmac(mdInfo, key, keyLen, data, dataLen, output);
    if (ret != 0) {
        LOG_E("TOTP", "HMAC failed: %d", ret);
        return false;
    }

    *outputLen = expectedLen;
    return true;
}

uint32_t totp_generate(const uint8_t *secret, size_t secretLen,
                       time_t timestamp, uint32_t period, uint8_t digits,
                       totp_algorithm_t algorithm) {
    if (!secret || secretLen == 0 || secretLen > TOTP_MAX_SECRET_LEN) {
        LOG_E("TOTP", "Invalid secret (len=%d)", secretLen);
        return 0;
    }

    if (digits < 6 || digits > 8) {
        LOG_D("TOTP", "Invalid digits %d, using 6", digits);
        digits = 6;
    }

    if (period == 0) {
        period = TOTP_DEFAULT_PERIOD;
    }

    // Calculate counter (T = floor(timestamp / period))
    uint64_t counter = timestamp / period;

    // Convert counter to big-endian byte array
    uint8_t counterBytes[8];
    for (int i = 7; i >= 0; i--) {
        counterBytes[i] = counter & 0xFF;
        counter >>= 8;
    }

    // Compute HMAC
    uint8_t hmac[HMAC_SHA512_SIZE];  // Max size
    size_t hmacLen = 0;
    if (!hmac_compute(algorithm, secret, secretLen, counterBytes, 8, hmac, &hmacLen)) {
        return 0;
    }

    // Dynamic truncation (RFC 4226)
    int offset = hmac[hmacLen - 1] & 0x0F;
    uint32_t binary =
        ((hmac[offset] & 0x7F) << 24) |
        ((hmac[offset + 1] & 0xFF) << 16) |
        ((hmac[offset + 2] & 0xFF) << 8) |
        (hmac[offset + 3] & 0xFF);

    // Truncate to requested digits
    uint32_t code = binary % POWERS_10[digits];

    LOG_D("TOTP", "Generated: %0*lu (offset=%d)", digits, code, offset);
    return code;
}

uint32_t totp_generate_default(const uint8_t *secret, size_t secretLen) {
    return totp_generate(secret, secretLen, time(NULL),
                         TOTP_DEFAULT_PERIOD, TOTP_DEFAULT_DIGITS,
                         TOTP_ALG_SHA1);
}

uint8_t totp_time_remaining(uint32_t period) {
    if (period == 0) {
        period = TOTP_DEFAULT_PERIOD;
    }
    return period - (time(NULL) % period);
}

bool totp_time_valid(void) {
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    // Check if year is after 2024 (tm_year is years since 1900)
    bool valid = timeinfo.tm_year >= 124;

    // Debug: log time check result
    LOG_D("TOTP", "Time check: %04d-%02d-%02d %02d:%02d (ts=%ld, tm_year=%d) -> %s",
          timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
          timeinfo.tm_hour, timeinfo.tm_min,
          (long)now, timeinfo.tm_year, valid ? "VALID" : "INVALID");

    return valid;
}
