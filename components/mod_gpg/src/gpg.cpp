#include "mod_gpg/gpg.h"
#include "mod_gpg/GpgStorage.h"
#include "mod_gpg/openpgp/openpgp.h"
#include "mod_gpg/openpgp/constants.h"
#include "cdc_hal/ISecureElement.h"
#include "cdc_log.h"
#include <mbedtls/sha1.h>
#include <mbedtls/base64.h>
#include <cstring>
#include <ctime>

namespace {

constexpr const char* TAG = "GPG";

// Sizes provided by mod_gpg/openpgp/constants.h:
// MPI_HEADER_SIZE, ED25519_PUBKEY_SIZE, P256_PUBKEY_SIZE,
// P256_PUBKEY_BITS, MPI_FULL_SIZE_ED25519, MPI_FULL_SIZE_P256

char s_pending_user_id[GPG_USER_ID_MAX] = {};

uint32_t get_unix_time(void) {
    return static_cast<uint32_t>(time(nullptr));
}

bool se_get_pubkey(uint8_t slot, uint8_t* pubkey, size_t max_len, uint8_t* curve_out) {
    auto* se = cdc::hal::getSecureElementInstance();
    if (!se || !pubkey) return false;
    cdc::hal::EccCurve curve = cdc::hal::EccCurve::P256;
    auto res = se->eccGetPublicKey(slot, pubkey, &curve);
    if (res != cdc::hal::SeResult::OK) return false;
    if (curve_out) {
        *curve_out = (curve == cdc::hal::EccCurve::ED25519) ? CDC_CURVE_ED25519
                                                             : CDC_CURVE_P256;
    }
    if (curve == cdc::hal::EccCurve::ED25519) {
        return max_len >= ED25519_PUBKEY_SIZE;
    }
    if (max_len < P256_PUBKEY_SIZE) return false;
    if (pubkey[0] == 0x04) {
        memmove(pubkey, pubkey + 1, P256_PUBKEY_SIZE);
    }
    return true;
}

bool se_generate_key(uint8_t slot, uint8_t curve) {
    auto* se = cdc::hal::getSecureElementInstance();
    if (!se) return false;
    auto c = (curve == CDC_CURVE_ED25519) ? cdc::hal::EccCurve::ED25519
                                           : cdc::hal::EccCurve::P256;
    return se->eccGenerate(slot, c) == cdc::hal::SeResult::OK;
}

bool calculate_fingerprint(const uint8_t *pubkey, uint8_t curve,
                           uint32_t created_at, uint8_t *fp_out) {
    if (!pubkey || !fp_out) return false;

    uint8_t algo = (curve == CDC_CURVE_ED25519) ? OPENPGP_ALGO_EDDSA : OPENPGP_ALGO_ECDSA;
    static const uint8_t oid_ed25519[] = {0x09, 0x2B, 0x06, 0x01, 0x04, 0x01, 0xDA, 0x47, 0x0F, 0x01};
    static const uint8_t oid_p256[]    = {0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07};

    const uint8_t *oid = (curve == CDC_CURVE_ED25519) ? oid_ed25519 : oid_p256;
    size_t oid_len     = (curve == CDC_CURVE_ED25519) ? sizeof(oid_ed25519)
                                                       : sizeof(oid_p256);

    uint8_t mpi[MPI_FULL_SIZE_P256];
    size_t mpi_len;
    if (curve == CDC_CURVE_ED25519) {
        uint16_t bits = 256;
        if ((pubkey[0] & 0x80) == 0) bits = 255;
        mpi[0] = (bits >> 8) & 0xFF;
        mpi[1] = bits & 0xFF;
        memcpy(mpi + MPI_HEADER_SIZE, pubkey, ED25519_PUBKEY_SIZE);
        mpi_len = MPI_FULL_SIZE_ED25519;
    } else {
        uint16_t bits = P256_PUBKEY_BITS;
        mpi[0] = (bits >> 8) & 0xFF;
        mpi[1] = bits & 0xFF;
        memcpy(mpi + MPI_HEADER_SIZE, pubkey, P256_PUBKEY_SIZE);
        mpi_len = MPI_FULL_SIZE_P256;
    }

    uint8_t body[128];
    size_t body_len = 0;
    body[body_len++] = 0x04;
    body[body_len++] = (created_at >> 24) & 0xFF;
    body[body_len++] = (created_at >> 16) & 0xFF;
    body[body_len++] = (created_at >> 8) & 0xFF;
    body[body_len++] = created_at & 0xFF;
    body[body_len++] = algo;
    memcpy(body + body_len, oid, oid_len);
    body_len += oid_len;
    memcpy(body + body_len, mpi, mpi_len);
    body_len += mpi_len;

    uint8_t prefix[3] = {0x99, (uint8_t)((body_len >> 8) & 0xFF), (uint8_t)(body_len & 0xFF)};

    mbedtls_sha1_context sha1;
    mbedtls_sha1_init(&sha1);
    mbedtls_sha1_starts(&sha1);
    mbedtls_sha1_update(&sha1, prefix, sizeof(prefix));
    mbedtls_sha1_update(&sha1, body, body_len);
    mbedtls_sha1_finish(&sha1, fp_out);
    mbedtls_sha1_free(&sha1);
    return true;
}

}  // namespace

bool gpg_init(void) {
    s_pending_user_id[0] = '\0';
    return gpg_storage_ready();
}

bool gpg_is_initialized(void) {
    return openpgp_has_any_key();
}

bool gpg_get_status(gpg_status_t *status) {
    if (!status) return false;
    memset(status, 0, sizeof(*status));
    if (!openpgp_has_any_key()) return false;

    status->initialized = true;

    uint8_t fp[GPG_FINGERPRINT_LEN] = {0};
    if (openpgp_get_fingerprint(KEY_SIG, fp)) {
        memcpy(status->fingerprint, fp, sizeof(status->fingerprint));
    }
    status->created_at = openpgp_get_gen_time(KEY_SIG);
    status->sign_count = openpgp_get_sig_count();
    // Curve isn't currently exposed by the OpenPGP card-application state;
    // default to Ed25519 (the on-device generate default) until that lookup
    // lands.
    status->curve = CDC_CURVE_ED25519;

    char name[GPG_USER_ID_MAX] = {0};
    openpgp_get_cardholder_name(name, sizeof(name));
    if (name[0]) {
        strncpy(status->user_id, name, sizeof(status->user_id) - 1);
    }
    return true;
}

bool gpg_set_pending_user_id(const char *user_id) {
    if (!user_id || !user_id[0]) return false;
    strncpy(s_pending_user_id, user_id, sizeof(s_pending_user_id) - 1);
    s_pending_user_id[sizeof(s_pending_user_id) - 1] = '\0';
    return true;
}

bool gpg_has_pending_user_id(void) {
    return s_pending_user_id[0] != '\0';
}

bool gpg_generate_key(uint8_t curve) {
    if (!gpg_storage_ready()) return false;
    if (!gpg_has_pending_user_id() && !openpgp_has_any_key()) return false;

    uint8_t sig_slot = gpg_storage_sig_slot();
    uint8_t dec_slot = gpg_storage_dec_slot();
    uint8_t aut_slot = gpg_storage_aut_slot();

    if (!se_generate_key(sig_slot, curve)) return false;
    if (!se_generate_key(aut_slot, curve)) return false;
    if (!se_generate_key(dec_slot, CDC_CURVE_P256)) return false;

    uint32_t created_at = get_unix_time();

    uint8_t pub_sig[GPG_PUBKEY_MAX_LEN] = {};
    uint8_t pub_dec[GPG_PUBKEY_MAX_LEN] = {};
    uint8_t pub_aut[GPG_PUBKEY_MAX_LEN] = {};
    uint8_t curve_sig = CDC_CURVE_P256;
    uint8_t curve_dec = CDC_CURVE_P256;
    uint8_t curve_aut = CDC_CURVE_P256;

    if (!se_get_pubkey(sig_slot, pub_sig, sizeof(pub_sig), &curve_sig)) return false;
    if (!se_get_pubkey(dec_slot, pub_dec, sizeof(pub_dec), &curve_dec)) return false;
    if (!se_get_pubkey(aut_slot, pub_aut, sizeof(pub_aut), &curve_aut)) return false;

    uint8_t fp_sig[GPG_FINGERPRINT_LEN] = {};
    uint8_t fp_dec[GPG_FINGERPRINT_LEN] = {};
    uint8_t fp_aut[GPG_FINGERPRINT_LEN] = {};
    if (!calculate_fingerprint(pub_sig, curve_sig, created_at, fp_sig)) return false;
    if (!calculate_fingerprint(pub_dec, curve_dec, created_at, fp_dec)) return false;
    if (!calculate_fingerprint(pub_aut, curve_aut, created_at, fp_aut)) return false;

    openpgp_set_key_fingerprint(KEY_SIG, fp_sig, created_at);
    openpgp_set_key_fingerprint(KEY_DEC, fp_dec, created_at);
    openpgp_set_key_fingerprint(KEY_AUT, fp_aut, created_at);

    if (s_pending_user_id[0]) {
        openpgp_set_cardholder_name(s_pending_user_id);
    }

    s_pending_user_id[0] = '\0';
    return true;
}

bool gpg_reset(void) {
    if (!gpg_storage_ready()) return false;
    // openpgp_factory_reset() wipes ECC slots, DEC privkey, NVS state
    // (fingerprints, gen-times, counter, cardholder, RC) and PINs.
    openpgp_factory_reset();
    s_pending_user_id[0] = '\0';
    return true;
}

bool gpg_export_pubkey_pem(char *buf, size_t size, size_t *out_len) {
    if (!buf || size < 256 || !out_len) {
        LOG_W(TAG, "export: bad args");
        return false;
    }
    if (!openpgp_has_any_key() || !gpg_storage_ready()) {
        LOG_W(TAG, "export: no key configured");
        return false;
    }

    auto* se = cdc::hal::getSecureElementInstance();
    if (!se) {
        LOG_W(TAG, "export: no SE");
        return false;
    }

    // Buffer sized for the optional 0x04 SEC1 prefix that secure elements
    // can prepend on P-256 reads (so 65 bytes for P-256 vs 32 for Ed25519).
    uint8_t pubkey[P256_PUBKEY_SIZE + 1] = {0};
    cdc::hal::EccCurve eccCurve = cdc::hal::EccCurve::P256;
    uint8_t sig_slot = gpg_storage_sig_slot();
    auto res = se->eccGetPublicKey(sig_slot, pubkey, &eccCurve);
    if (res != cdc::hal::SeResult::OK) {
        LOG_W(TAG, "export: eccGetPublicKey slot=%u failed (%d)",
              sig_slot, static_cast<int>(res));
        return false;
    }
    uint8_t curve = (eccCurve == cdc::hal::EccCurve::ED25519) ? CDC_CURVE_ED25519
                                                                : CDC_CURVE_P256;
    if (curve == CDC_CURVE_P256 && pubkey[0] == 0x04) {
        memmove(pubkey, pubkey + 1, P256_PUBKEY_SIZE);
    }

    if (curve == CDC_CURVE_ED25519) {
        static const uint8_t ed25519_prefix[] = {
            0x30, 0x2a,
            0x30, 0x05,
            0x06, 0x03, 0x2b, 0x65, 0x70,
            0x03, 0x21, 0x00
        };

        uint8_t der[sizeof(ed25519_prefix) + 32];
        memcpy(der, ed25519_prefix, sizeof(ed25519_prefix));
        memcpy(der + sizeof(ed25519_prefix), pubkey, 32);

        size_t b64_len = 0;
        char b64[128];
        if (mbedtls_base64_encode(reinterpret_cast<unsigned char*>(b64), sizeof(b64),
                                   &b64_len, der, sizeof(der)) != 0) {
            return false;
        }

        int written = snprintf(buf, size,
                               "-----BEGIN PUBLIC KEY-----\n%s\n-----END PUBLIC KEY-----\n",
                               b64);
        if (written < 0 || static_cast<size_t>(written) >= size) return false;
        *out_len = static_cast<size_t>(written);
        return true;
    }

    static const uint8_t p256_prefix[] = {
        0x30, 0x59,
        0x30, 0x13,
        0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01,
        0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07,
        0x03, 0x42, 0x00
    };
    uint8_t der[sizeof(p256_prefix) + 65];
    memcpy(der, p256_prefix, sizeof(p256_prefix));
    der[sizeof(p256_prefix)] = 0x04;
    memcpy(der + sizeof(p256_prefix) + 1, pubkey, 64);

    size_t b64_len = 0;
    char b64[200];
    if (mbedtls_base64_encode(reinterpret_cast<unsigned char*>(b64), sizeof(b64),
                               &b64_len, der, sizeof(der)) != 0) {
        return false;
    }
    int written = snprintf(buf, size,
                           "-----BEGIN PUBLIC KEY-----\n%s\n-----END PUBLIC KEY-----\n",
                           b64);
    if (written < 0 || static_cast<size_t>(written) >= size) return false;
    *out_len = static_cast<size_t>(written);
    return true;
}
