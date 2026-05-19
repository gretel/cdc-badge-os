/**
 * \brief OpenPGP algorithm-attribute codec (see algo_attr.h).
 *
 * The OID byte sequences match those required by gpg-card / scdaemon when
 * the corresponding ECC key is generated or imported. Verified against the
 * OpenPGP smart-card application 3.4.1 specification, §4.4.3.7-9.
 */

#include "mod_gpg/openpgp/algo_attr.h"

#include <string.h>

namespace {

// OIDs are stored without leading tag/length bytes, exactly as carried in
// the algorithm-attribute DO payload.

constexpr uint8_t kOidP256[]    = { 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07 };
constexpr uint8_t kOidEd25519[] = { 0x2B, 0x06, 0x01, 0x04, 0x01, 0xDA, 0x47, 0x0F, 0x01 };
constexpr uint8_t kOidX25519[]  = { 0x2B, 0x06, 0x01, 0x04, 0x01, 0x97, 0x55, 0x01, 0x05, 0x01 };

constexpr size_t kP256Len    = sizeof(kOidP256);
constexpr size_t kEd25519Len = sizeof(kOidEd25519);
constexpr size_t kX25519Len  = sizeof(kOidX25519);

algo_attr_curve_t classify_oid(const uint8_t *oid, size_t len) {
    if (len == kP256Len    && memcmp(oid, kOidP256,    len) == 0) return ALGO_ATTR_CURVE_P256;
    if (len == kEd25519Len && memcmp(oid, kOidEd25519, len) == 0) return ALGO_ATTR_CURVE_ED25519;
    if (len == kX25519Len  && memcmp(oid, kOidX25519,  len) == 0) return ALGO_ATTR_CURVE_X25519;
    return ALGO_ATTR_CURVE_UNKNOWN;
}

bool curve_to_oid(algo_attr_curve_t curve, const uint8_t **out, size_t *out_len) {
    switch (curve) {
        case ALGO_ATTR_CURVE_P256:    *out = kOidP256;    *out_len = kP256Len;    return true;
        case ALGO_ATTR_CURVE_ED25519: *out = kOidEd25519; *out_len = kEd25519Len; return true;
        case ALGO_ATTR_CURVE_X25519:  *out = kOidX25519;  *out_len = kX25519Len;  return true;
        default: return false;
    }
}

bool is_known_algo(uint8_t id) {
    return id == ALGO_ATTR_ID_RSA  || id == ALGO_ATTR_ID_ECDH ||
           id == ALGO_ATTR_ID_ECDSA || id == ALGO_ATTR_ID_EDDSA;
}

bool is_rsa_modulus_bits(uint16_t n_bits) {
    return n_bits == 2048 || n_bits == 3072 || n_bits == 4096;
}

}  // namespace

algo_attr_status_t algo_attr_parse(const uint8_t *bytes, size_t len, algo_attr_t *out) {
    if (!bytes || !out) return ALGO_ATTR_ERR_NULL;
    if (len < 2) return ALGO_ATTR_ERR_TOO_SHORT;

    memset(out, 0, sizeof(*out));
    out->algo_id = bytes[0];
    if (!is_known_algo(out->algo_id)) return ALGO_ATTR_ERR_BAD_ALGO;
    out->is_rsa = (out->algo_id == ALGO_ATTR_ID_RSA);

    if (out->is_rsa) {
        if (len < 6) return ALGO_ATTR_ERR_BAD_RSA;
        out->rsa_n_bits = static_cast<uint16_t>((bytes[1] << 8) | bytes[2]);
        out->rsa_e_bits = static_cast<uint16_t>((bytes[3] << 8) | bytes[4]);
        out->rsa_import_fmt = bytes[5];
        if (!is_rsa_modulus_bits(out->rsa_n_bits)) return ALGO_ATTR_ERR_BAD_RSA;
        if (out->rsa_e_bits == 0 || out->rsa_e_bits > 64) return ALGO_ATTR_ERR_BAD_RSA;
        if (out->rsa_import_fmt > 0x03) return ALGO_ATTR_ERR_BAD_RSA;
        return ALGO_ATTR_OK;
    }

    // ECC path: bytes[1..] is the OID; an optional trailing 0xFF flags the
    // standard public-key import format.
    const uint8_t *oid = bytes + 1;
    size_t oid_len = len - 1;
    if (oid_len > 0 && oid[oid_len - 1] == 0xFF) {
        out->has_import_format = true;
        out->import_format = 0xFF;
        oid_len -= 1;
    }
    out->curve = classify_oid(oid, oid_len);
    return ALGO_ATTR_OK;
}

algo_attr_status_t algo_attr_build(const algo_attr_t *attr, uint8_t *out,
                                   size_t out_cap, size_t *out_len) {
    if (!attr || !out || !out_len) return ALGO_ATTR_ERR_NULL;
    if (!is_known_algo(attr->algo_id)) return ALGO_ATTR_ERR_BAD_ALGO;

    if (attr->algo_id == ALGO_ATTR_ID_RSA) {
        if (!is_rsa_modulus_bits(attr->rsa_n_bits)) return ALGO_ATTR_ERR_BAD_RSA;
        if (out_cap < 6) return ALGO_ATTR_ERR_BUF_TOO_SMALL;
        out[0] = attr->algo_id;
        out[1] = static_cast<uint8_t>((attr->rsa_n_bits >> 8) & 0xFF);
        out[2] = static_cast<uint8_t>(attr->rsa_n_bits & 0xFF);
        out[3] = static_cast<uint8_t>((attr->rsa_e_bits >> 8) & 0xFF);
        out[4] = static_cast<uint8_t>(attr->rsa_e_bits & 0xFF);
        out[5] = attr->rsa_import_fmt;
        *out_len = 6;
        return ALGO_ATTR_OK;
    }

    const uint8_t *oid = nullptr;
    size_t oid_len = 0;
    if (!curve_to_oid(attr->curve, &oid, &oid_len)) return ALGO_ATTR_ERR_BAD_CURVE;

    const size_t need = 1 + oid_len + (attr->has_import_format ? 1 : 0);
    if (out_cap < need) return ALGO_ATTR_ERR_BUF_TOO_SMALL;
    out[0] = attr->algo_id;
    memcpy(out + 1, oid, oid_len);
    if (attr->has_import_format) {
        out[1 + oid_len] = attr->import_format;
    }
    *out_len = need;
    return ALGO_ATTR_OK;
}

algo_attr_status_t algo_attr_validate_role(const algo_attr_t *attr,
                                           algo_attr_role_t role) {
    if (!attr) return ALGO_ATTR_ERR_NULL;
    if (attr->is_rsa) return ALGO_ATTR_OK;  // RSA works for any role.

    switch (role) {
        case ALGO_ATTR_ROLE_SIG:
        case ALGO_ATTR_ROLE_AUT:
            if (attr->algo_id == ALGO_ATTR_ID_ECDSA || attr->algo_id == ALGO_ATTR_ID_EDDSA) {
                return ALGO_ATTR_OK;
            }
            return ALGO_ATTR_ERR_ROLE_MISMATCH;
        case ALGO_ATTR_ROLE_DEC:
            if (attr->algo_id == ALGO_ATTR_ID_ECDH) return ALGO_ATTR_OK;
            return ALGO_ATTR_ERR_ROLE_MISMATCH;
    }
    return ALGO_ATTR_ERR_ROLE_MISMATCH;
}

algo_attr_status_t algo_attr_validate_capability(const algo_attr_t *attr,
                                                 bool rsa_supported) {
    if (!attr) return ALGO_ATTR_ERR_NULL;
    if (attr->is_rsa) {
        return rsa_supported ? ALGO_ATTR_OK : ALGO_ATTR_ERR_BAD_RSA;
    }
    switch (attr->curve) {
        case ALGO_ATTR_CURVE_P256:
            if (attr->algo_id == ALGO_ATTR_ID_ECDSA || attr->algo_id == ALGO_ATTR_ID_ECDH) {
                return ALGO_ATTR_OK;
            }
            return ALGO_ATTR_ERR_BAD_CURVE;
        case ALGO_ATTR_CURVE_ED25519:
            return (attr->algo_id == ALGO_ATTR_ID_EDDSA) ? ALGO_ATTR_OK : ALGO_ATTR_ERR_BAD_CURVE;
        case ALGO_ATTR_CURVE_X25519:
        case ALGO_ATTR_CURVE_UNKNOWN:
        default:
            return ALGO_ATTR_ERR_BAD_CURVE;
    }
}
