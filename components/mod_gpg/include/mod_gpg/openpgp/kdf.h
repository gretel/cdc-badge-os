/**
 * \brief OpenPGP KDF Data Object (tag F9) byte codec.
 *
 * Encodes / decodes the byte sequence stored under DO 0xF9 per OpenPGP
 * Smart Card Application 3.4.1, §4.4.3.13. The KDF DO is itself a sequence
 * of inner TLVs that describe how the PIN values are pre-hashed before
 * being sent to VERIFY / CHANGE REFERENCE DATA:
 *
 *   Inner tag | Length | Contents
 *   ----------+--------+----------------------------------------------------
 *      0x81   |   1    | KDF algorithm  (0x00 none, 0x03 PBKDF2)
 *      0x82   |   1    | Hash algorithm (0x08 SHA-256, 0x0A SHA-512)
 *      0x83   |   4    | Iteration count, big-endian
 *      0x84   |   8    | Salt for PW1
 *      0x85   |   8    | Salt for RC (Resetting Code), optional
 *      0x86   |   8    | Salt for PW3 (Admin),         optional
 *      0x87   | 32/64  | Initial hash of PW1 (for fast-path verify), optional
 *      0x88   | 32/64  | Initial hash of PW3, optional
 *
 * This header covers ONLY the byte layout. Actual key derivation (PBKDF2 /
 * HKDF) lives elsewhere — it needs mbedTLS or equivalent and is out of
 * scope for the pure-logic test tier.
 */

#ifndef MOD_GPG_OPENPGP_KDF_H
#define MOD_GPG_OPENPGP_KDF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KDF_SALT_LEN 8
#define KDF_INITIAL_HASH_MAX 64

/** \brief KDF algorithm identifiers (DO 0xF9 inner tag 0x81). */
typedef enum {
    KDF_ALGO_NONE   = 0x00,
    KDF_ALGO_PBKDF2 = 0x03,  /**< Only PBKDF2 is currently used by gpg/scd. */
} kdf_algo_t;

/** \brief Hash algorithms accepted inside KDF-DO (inner tag 0x82). */
typedef enum {
    KDF_HASH_NONE   = 0x00,
    KDF_HASH_SHA256 = 0x08,
    KDF_HASH_SHA512 = 0x0A,
} kdf_hash_t;

/** \brief Parsed KDF-DO contents. */
typedef struct {
    kdf_algo_t algo;
    kdf_hash_t hash;
    uint32_t   iter_count;

    bool    has_pw1_salt;
    uint8_t pw1_salt[KDF_SALT_LEN];
    bool    has_rc_salt;
    uint8_t rc_salt[KDF_SALT_LEN];
    bool    has_pw3_salt;
    uint8_t pw3_salt[KDF_SALT_LEN];

    bool     has_pw1_initial;
    uint8_t  pw1_initial[KDF_INITIAL_HASH_MAX];
    uint8_t  pw1_initial_len;
    bool     has_pw3_initial;
    uint8_t  pw3_initial[KDF_INITIAL_HASH_MAX];
    uint8_t  pw3_initial_len;
} kdf_do_t;

/** \brief KDF-DO codec error codes. */
typedef enum {
    KDF_OK = 0,
    KDF_ERR_NULL,
    KDF_ERR_BUF_TOO_SMALL,
    KDF_ERR_BAD_LENGTH,
    KDF_ERR_BAD_TAG,
    KDF_ERR_BAD_ALGO,
    KDF_ERR_BAD_HASH,
    KDF_ERR_BAD_HASH_LEN,
} kdf_status_t;

/** \brief Zeroes out the structure. */
void kdf_do_clear(kdf_do_t *out);

/**
 * \brief Parse a KDF-DO byte sequence into structured form.
 *
 * Recognises the inner TLVs above; unknown inner tags cause
 * KDF_ERR_BAD_TAG so we never silently accept malformed input from the
 * host. A short DO with KDF_ALGO_NONE in the algorithm byte and no further
 * tags is accepted as "KDF disabled".
 */
kdf_status_t kdf_do_parse(const uint8_t *bytes, size_t len, kdf_do_t *out);

/**
 * \brief Serialise a kdf_do_t into the wire byte sequence.
 *
 * Only fields whose `has_*` flag is true are emitted. Algorithm and hash
 * bytes are always present (they describe whether the DO is active at all).
 */
kdf_status_t kdf_do_build(const kdf_do_t *kdf, uint8_t *out, size_t out_cap,
                          size_t *out_len);

/**
 * \brief Convenience helper: produce the "KDF disabled" KDF-DO body — three
 *        bytes (\c 81 01 00) — that hosts expect when KDF is opt-in and not
 *        yet enabled. Useful as the default GET DATA response for tag F9.
 *
 * \param out Output buffer (≥ 3 bytes).
 * \param out_cap Buffer capacity.
 * \param out_len Receives the number of bytes written.
 */
kdf_status_t kdf_do_build_disabled(uint8_t *out, size_t out_cap, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif  // MOD_GPG_OPENPGP_KDF_H
