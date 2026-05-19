/**
 * \brief OpenPGP algorithm-attribute codec.
 *
 * Encodes / decodes / validates the byte sequences carried by Data Objects
 * 0xC1 (SIG), 0xC2 (DEC) and 0xC3 (AUT) per OpenPGP Smart Card Application
 * 3.4.1, §4.4.3.7-9.
 *
 * Layout:
 *
 *   ECC (algorithm = ECDSA / EdDSA / ECDH):
 *     +------+----------------+--------+
 *     | algo |   curve OID    | format |
 *     +------+----------------+--------+
 *        1B       n bytes        1B (optional, 0xFF = standard)
 *
 *   RSA (algorithm = RSA):
 *     +------+-----+-----+---------+
 *     | algo | N-l | e-l | import  |
 *     +------+-----+-----+---------+
 *        1B   2B    2B      1B
 *
 *   Algorithm IDs come from RFC 4880 §9.1; curve OIDs are the DER-encoded
 *   bytes WITHOUT the leading tag/length, exactly as carried in the DO.
 *
 * The CDC Badge currently honours three ECC choices: Ed25519 (SIG/AUT),
 * P-256 ECDSA (SIG/AUT), P-256 ECDH (DEC). RSA is optional and is treated
 * as a fallback per the plan in plan2.md §10.
 */

#ifndef MOD_GPG_OPENPGP_ALGO_ATTR_H
#define MOD_GPG_OPENPGP_ALGO_ATTR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \brief RFC 4880 §9.1 algorithm IDs we care about. */
typedef enum {
    ALGO_ATTR_ID_RSA   = 0x01,
    ALGO_ATTR_ID_ECDH  = 0x12,
    ALGO_ATTR_ID_ECDSA = 0x13,
    ALGO_ATTR_ID_EDDSA = 0x16,
} algo_attr_algo_id_t;

/** \brief Curves the firmware recognises. UNKNOWN flags unsupported OIDs. */
typedef enum {
    ALGO_ATTR_CURVE_UNKNOWN = 0,
    ALGO_ATTR_CURVE_P256,
    ALGO_ATTR_CURVE_ED25519,
    ALGO_ATTR_CURVE_X25519,    /**< Reserved for future ECDH-X25519 support. */
} algo_attr_curve_t;

/** \brief Key role (selects which DO tag is being parsed / built). */
typedef enum {
    ALGO_ATTR_ROLE_SIG = 0,
    ALGO_ATTR_ROLE_DEC = 1,
    ALGO_ATTR_ROLE_AUT = 2,
} algo_attr_role_t;

/** \brief Parsed algorithm-attribute payload. */
typedef struct {
    uint8_t algo_id;                /**< RFC 4880 §9.1 value. */
    bool    is_rsa;                 /**< Convenience flag derived from algo_id. */
    /* ECC variant. */
    algo_attr_curve_t curve;
    bool    has_import_format;
    uint8_t import_format;          /**< Trailing 0xFF byte if present. */
    /* RSA variant. */
    uint16_t rsa_n_bits;            /**< Modulus length in bits (e.g. 2048). */
    uint16_t rsa_e_bits;            /**< Public exponent length in bits (typically 32). */
    uint8_t  rsa_import_fmt;        /**< 00 standard, 01 with mod, 02 mod+exp, 03 full CRT. */
} algo_attr_t;

/** \brief Outcome of algo-attribute operations. */
typedef enum {
    ALGO_ATTR_OK = 0,
    ALGO_ATTR_ERR_TOO_SHORT,
    ALGO_ATTR_ERR_BAD_ALGO,
    ALGO_ATTR_ERR_BAD_CURVE,
    ALGO_ATTR_ERR_BAD_RSA,
    ALGO_ATTR_ERR_ROLE_MISMATCH,
    ALGO_ATTR_ERR_BUF_TOO_SMALL,
    ALGO_ATTR_ERR_NULL,
} algo_attr_status_t;

/**
 * \brief Parse a raw algorithm-attribute byte sequence into structured form.
 *
 * The function recognises the three supported curves by OID match. Any other
 * OID lands as ALGO_ATTR_CURVE_UNKNOWN with the call still succeeding so the
 * caller can choose between rejection (`6A80`) and a permissive accept.
 */
algo_attr_status_t algo_attr_parse(const uint8_t *bytes, size_t len, algo_attr_t *out);

/**
 * \brief Serialise an algorithm-attribute structure to bytes.
 *
 * The trailing 0xFF "import format" byte for ECC is emitted only when
 * \p attr->has_import_format is set. RSA values are encoded in network byte
 * order (big-endian) per spec.
 */
algo_attr_status_t algo_attr_build(const algo_attr_t *attr, uint8_t *out,
                                   size_t out_cap, size_t *out_len);

/**
 * \brief Check whether the parsed attribute is compatible with the key role
 *        it will be installed into.
 *
 * Encodes the badge-specific policy: ECDSA + EdDSA only for SIG/AUT,
 * ECDH only for DEC; RSA is acceptable for any role when enabled.
 */
algo_attr_status_t algo_attr_validate_role(const algo_attr_t *attr,
                                           algo_attr_role_t role);

/**
 * \brief Check whether the badge's secure element / mbedTLS combination can
 *        actually execute this algorithm.
 *
 * Returns ALGO_ATTR_OK for Ed25519, P-256 ECDSA, P-256 ECDH, and (if
 * rsa_supported is true) RSA 2048/3072/4096. Anything else yields
 * ALGO_ATTR_ERR_BAD_CURVE or ALGO_ATTR_ERR_BAD_RSA.
 */
algo_attr_status_t algo_attr_validate_capability(const algo_attr_t *attr,
                                                 bool rsa_supported);

#ifdef __cplusplus
}
#endif

#endif  // MOD_GPG_OPENPGP_ALGO_ATTR_H
