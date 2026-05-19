#include "mod_gpg/gpg.h"
#include "mod_gpg/GpgStorage.h"
#include "cdc_core/pin_storage_c.h"
#include "mod_gpg/openpgp/openpgp.h"
#include "mod_gpg/openpgp/constants.h"
#include "cdc_hal/ISecureElement.h"
#include "cdc_log.h"
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>
#include <mbedtls/base64.h>
#include <nvs.h>
#include <cstring>
#include <ctime>

static constexpr const char* NVS_NAMESPACE = "mod_gpg";
static constexpr const char* NVS_KEY_META = "meta";

static bool s_initialized = false;
static gpg_metadata_t s_metadata = {};
static char s_pending_user_id[GPG_USER_ID_MAX] = {};

/**
 * \brief Returns the current Unix timestamp in seconds.
 * \return Current POSIX time.
 */
static uint32_t get_unix_time(void) {
    return static_cast<uint32_t>(time(nullptr));
}

/**
 * \brief Reads a public key from the secure element and normalizes layout per curve.
 * \param slot Secure element slot containing the key.
 * \param pubkey Destination buffer for the public key bytes.
 * \param max_len Size of `pubkey` in bytes.
 * \param curve_out Optional destination for the exported CDC curve identifier.
 * \return `true` on success, otherwise `false`.
 */
static bool se_get_pubkey(uint8_t slot, uint8_t* pubkey, size_t max_len, uint8_t* curve_out) {
    auto* se = cdc::hal::getSecureElementInstance();
    if (!se || !pubkey) return false;
    cdc::hal::EccCurve curve = cdc::hal::EccCurve::P256;
    auto res = se->eccGetPublicKey(slot, pubkey, &curve);
    if (res != cdc::hal::SeResult::OK) return false;
    if (curve_out) {
        *curve_out = (curve == cdc::hal::EccCurve::ED25519) ? CDC_CURVE_ED25519 : CDC_CURVE_P256;
    }
    if (curve == cdc::hal::EccCurve::ED25519) {
        return max_len >= 32;
    }
    if (max_len < 64) return false;
    if (pubkey[0] == 0x04) {
        memmove(pubkey, pubkey + 1, 64);
    }
    return true;
}

/**
 * \brief Generates a key pair in the secure element for the requested curve.
 * \param slot Secure element slot to populate.
 * \param curve CDC curve identifier.
 * \return `true` on success, otherwise `false`.
 */
static bool se_generate_key(uint8_t slot, uint8_t curve) {
    auto* se = cdc::hal::getSecureElementInstance();
    if (!se) return false;
    cdc::hal::EccCurve c = (curve == CDC_CURVE_ED25519) ? cdc::hal::EccCurve::ED25519
                                                         : cdc::hal::EccCurve::P256;
    return se->eccGenerate(slot, c) == cdc::hal::SeResult::OK;
}

/**
 * \brief Signs a digest with a P-256 key stored in the secure element.
 * \param slot Secure element slot holding the signing key.
 * \param hash Digest bytes to sign.
 * \param hash_len Length of `hash` in bytes.
 * \param sig Destination buffer for DER-encoded ECDSA signature.
 * \param sig_len In/out signature length value.
 * \return `true` on success, otherwise `false`.
 */
static bool se_sign_p256(uint8_t slot, const uint8_t* hash, size_t hash_len, uint8_t* sig, size_t* sig_len) {
    auto* se = cdc::hal::getSecureElementInstance();
    if (!se || !hash || !sig || !sig_len) return false;
    return se->ecdsaSign(slot, hash, hash_len, sig, sig_len) == cdc::hal::SeResult::OK;
}

/**
 * \brief Signs a message with an Ed25519 key stored in the secure element.
 * \param slot Secure element slot holding the signing key.
 * \param msg Message bytes to sign.
 * \param msg_len Length of `msg` in bytes.
 * \param sig Destination buffer for the 64-byte signature.
 * \return `true` on success, otherwise `false`.
 */
static bool se_sign_ed25519(uint8_t slot, const uint8_t* msg, size_t msg_len, uint8_t* sig) {
    auto* se = cdc::hal::getSecureElementInstance();
    if (!se || !msg || !sig) return false;
    return se->eddsaSign(slot, msg, msg_len, sig) == cdc::hal::SeResult::OK;
}

/**
 * \brief Loads persisted GPG metadata from NVS.
 * \return `true` when valid metadata was loaded, otherwise `false`.
 */
static bool load_metadata(void) {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    size_t len = sizeof(gpg_metadata_t);
    esp_err_t err = nvs_get_blob(handle, NVS_KEY_META, &s_metadata, &len);
    nvs_close(handle);
    if (err != ESP_OK || len != sizeof(gpg_metadata_t)) {
        memset(&s_metadata, 0, sizeof(s_metadata));
        return false;
    }
    if (s_metadata.magic != GPG_METADATA_MAGIC || s_metadata.version != GPG_METADATA_VERSION) {
        memset(&s_metadata, 0, sizeof(s_metadata));
        return false;
    }

    return true;
}

/**
 * \brief Persists the current GPG metadata to NVS.
 * \return `true` on success, otherwise `false`.
 */
static bool save_metadata(void) {
    s_metadata.magic = GPG_METADATA_MAGIC;
    s_metadata.version = GPG_METADATA_VERSION;
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        return false;
    }
    esp_err_t err = nvs_set_blob(handle, NVS_KEY_META, &s_metadata, sizeof(s_metadata));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err == ESP_OK;
}

/**
 * \brief Calculates the OpenPGP v4 fingerprint for a public key packet body.
 * \param pubkey Raw public key bytes.
 * \param pubkey_len Length of `pubkey` in bytes.
 * \param curve CDC curve identifier.
 * \param created_at Key creation timestamp.
 * \param fp_out Destination buffer for the 20-byte fingerprint.
 * \return `true` on success, otherwise `false`.
 */
static bool calculate_fingerprint(const uint8_t *pubkey, size_t pubkey_len,
                                  uint8_t curve, uint32_t created_at,
                                  uint8_t *fp_out) {
    (void)pubkey_len;
    if (!pubkey || !fp_out) return false;

    uint8_t algo = (curve == CDC_CURVE_ED25519) ? OPENPGP_ALGO_EDDSA : OPENPGP_ALGO_ECDSA;
    static const uint8_t oid_ed25519[] = {0x09, 0x2B, 0x06, 0x01, 0x04, 0x01, 0xDA, 0x47, 0x0F, 0x01};
    static const uint8_t oid_p256[] = {0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07};

    const uint8_t *oid = (curve == CDC_CURVE_ED25519) ? oid_ed25519 : oid_p256;
    size_t oid_len = (curve == CDC_CURVE_ED25519) ? sizeof(oid_ed25519) : sizeof(oid_p256);

    uint8_t mpi[MPI_FULL_SIZE_P256];
    size_t mpi_len = 0;

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

    uint8_t body[256];
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

    uint8_t prefix[3];
    prefix[0] = 0x99;
    prefix[1] = (body_len >> 8) & 0xFF;
    prefix[2] = body_len & 0xFF;

    uint8_t sha[OPENPGP_FINGERPRINT_SIZE];
    mbedtls_sha1_context sha1;
    mbedtls_sha1_init(&sha1);
    mbedtls_sha1_starts(&sha1);
    mbedtls_sha1_update(&sha1, prefix, sizeof(prefix));
    mbedtls_sha1_update(&sha1, body, body_len);
    mbedtls_sha1_finish(&sha1, sha);
    mbedtls_sha1_free(&sha1);
    memcpy(fp_out, sha, OPENPGP_FINGERPRINT_SIZE);
    return true;
}

/**
 * \brief Calculates the OpenPGP v5 fingerprint for a public key packet body.
 * \param pubkey Raw public key bytes.
 * \param pubkey_len Length of `pubkey` in bytes.
 * \param curve CDC curve identifier.
 * \param created_at Key creation timestamp.
 * \param fp_out Destination buffer for the 32-byte fingerprint.
 * \return `true` on success, otherwise `false`.
 */
static bool calculate_fingerprint_v5(const uint8_t *pubkey, size_t pubkey_len,
                                     uint8_t curve, uint32_t created_at,
                                     uint8_t *fp_out) {
    (void)pubkey_len;
    if (!pubkey || !fp_out) return false;

    uint8_t algo = (curve == CDC_CURVE_ED25519) ? OPENPGP_ALGO_EDDSA : OPENPGP_ALGO_ECDSA;
    static const uint8_t oid_ed25519[] = {0x09, 0x2B, 0x06, 0x01, 0x04, 0x01, 0xDA, 0x47, 0x0F, 0x01};
    static const uint8_t oid_p256[] = {0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07};

    const uint8_t *oid = (curve == CDC_CURVE_ED25519) ? oid_ed25519 : oid_p256;
    size_t oid_len = (curve == CDC_CURVE_ED25519) ? sizeof(oid_ed25519) : sizeof(oid_p256);

    uint8_t mpi[MPI_FULL_SIZE_P256];
    size_t mpi_len = 0;

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

    uint8_t body[256];
    size_t body_len = 0;
    body[body_len++] = 0x05;
    body[body_len++] = (created_at >> 24) & 0xFF;
    body[body_len++] = (created_at >> 16) & 0xFF;
    body[body_len++] = (created_at >> 8) & 0xFF;
    body[body_len++] = created_at & 0xFF;
    body[body_len++] = algo;
    memcpy(body + body_len, oid, oid_len);
    body_len += oid_len;
    memcpy(body + body_len, mpi, mpi_len);
    body_len += mpi_len;

    uint8_t prefix[3];
    prefix[0] = 0x9A;
    prefix[1] = (body_len >> 8) & 0xFF;
    prefix[2] = body_len & 0xFF;

    uint8_t sha[SHA256_DIGEST_SIZE];
    mbedtls_sha256_context sha256;
    mbedtls_sha256_init(&sha256);
    mbedtls_sha256_starts(&sha256, 0);
    mbedtls_sha256_update(&sha256, prefix, sizeof(prefix));
    mbedtls_sha256_update(&sha256, body, body_len);
    mbedtls_sha256_finish(&sha256, sha);
    mbedtls_sha256_free(&sha256);
    memcpy(fp_out, sha, SHA256_DIGEST_SIZE);
    return true;
}

/**
 * \brief Initializes the GPG module and attempts to load persisted key metadata.
 * \return `true` when storage is reachable, otherwise `false`.
 */
bool gpg_init(void) {
    if (!gpg_storage_ready()) {
        return false;
    }
    bool loaded = load_metadata();
    s_initialized = loaded;
    return true;
}

/**
 * \brief Reports whether valid GPG metadata is currently loaded.
 * \return `true` when the module is initialized, otherwise `false`.
 */
bool gpg_is_initialized(void) {
    return s_initialized;
}

/**
 * \brief Returns current GPG status snapshot.
 * \param status Destination status structure.
 * \return `true` on success, otherwise `false`.
 */
bool gpg_get_status(gpg_status_t *status) {
    if (!status) return false;
    memset(status, 0, sizeof(*status));

    // Primary source of truth is the OpenPGP card-application state, because
    // gpg --card-edit writes there directly. The legacy mod_gpg metadata is
    // only consulted as a fallback for UI-side generations that haven't been
    // ported to the unified state yet.
    if (openpgp_has_any_key()) {
        status->initialized = true;
        uint8_t fp[GPG_FINGERPRINT_LEN] = {0};
        if (openpgp_get_fingerprint(KEY_SIG, fp)) {
            memcpy(status->fingerprint, fp, sizeof(status->fingerprint));
        }
        status->created_at = openpgp_get_gen_time(KEY_SIG);
        status->sign_count = openpgp_get_sig_count();
        char name[64] = {0};
        openpgp_get_cardholder_name(name, sizeof(name));
        if (name[0]) {
            strncpy(status->user_id, name, sizeof(status->user_id) - 1);
        } else if (s_initialized && s_metadata.user_id[0]) {
            strncpy(status->user_id, s_metadata.user_id, sizeof(status->user_id) - 1);
        }
        status->curve = s_initialized ? s_metadata.curve : CDC_CURVE_ED25519;
        return true;
    }

    if (!s_initialized) return false;
    status->initialized = true;
    status->curve = s_metadata.curve;
    strncpy(status->user_id, s_metadata.user_id, sizeof(status->user_id) - 1);
    memcpy(status->fingerprint, s_metadata.fingerprint, sizeof(status->fingerprint));
    status->created_at = s_metadata.created_at;
    status->sign_count = s_metadata.sign_count;
    return true;
}

/**
 * \brief Stores a user ID that will be bound to the next generated key set.
 * \param user_id User ID string to stage.
 * \return `true` on success, otherwise `false`.
 */
bool gpg_set_pending_user_id(const char *user_id) {
    if (!user_id || !user_id[0]) return false;
    strncpy(s_pending_user_id, user_id, sizeof(s_pending_user_id) - 1);
    s_pending_user_id[sizeof(s_pending_user_id) - 1] = '\0';
    return true;
}

/**
 * \brief Indicates whether a pending user ID is staged for key generation.
 * \return `true` when a pending user ID exists, otherwise `false`.
 */
bool gpg_has_pending_user_id(void) {
    return s_pending_user_id[0] != '\0';
}

/**
 * \brief Generates SIG/DEC/AUT key material and updates persisted metadata.
 * \param curve Primary signing/authentication curve identifier.
 * \return `true` on success, otherwise `false`.
 */
bool gpg_generate_key(uint8_t curve) {
    if (!gpg_storage_ready()) return false;
    if (!gpg_has_pending_user_id() && !s_initialized) return false;

    uint8_t sig_slot = gpg_storage_sig_slot();
    uint8_t dec_slot = gpg_storage_dec_slot();
    uint8_t aut_slot = gpg_storage_aut_slot();

    if (!se_generate_key(sig_slot, curve)) return false;
    if (!se_generate_key(aut_slot, curve)) return false;
    if (!se_generate_key(dec_slot, CDC_CURVE_P256)) return false;

    uint32_t created_at = get_unix_time();

    uint8_t fp_sig[GPG_FINGERPRINT_LEN] = {};
    uint8_t fp_dec[GPG_FINGERPRINT_LEN] = {};
    uint8_t fp_aut[GPG_FINGERPRINT_LEN] = {};

    uint8_t pub_sig[GPG_PUBKEY_MAX_LEN] = {};
    uint8_t pub_dec[GPG_PUBKEY_MAX_LEN] = {};
    uint8_t pub_aut[GPG_PUBKEY_MAX_LEN] = {};
    uint8_t curve_sig = CDC_CURVE_P256;
    uint8_t curve_dec = CDC_CURVE_P256;
    uint8_t curve_aut = CDC_CURVE_P256;

    if (!se_get_pubkey(sig_slot, pub_sig, sizeof(pub_sig), &curve_sig)) return false;
    if (!se_get_pubkey(dec_slot, pub_dec, sizeof(pub_dec), &curve_dec)) return false;
    if (!se_get_pubkey(aut_slot, pub_aut, sizeof(pub_aut), &curve_aut)) return false;

    size_t sig_len = (curve_sig == CDC_CURVE_ED25519) ? 32 : 64;
    size_t dec_len = (curve_dec == CDC_CURVE_ED25519) ? 32 : 64;
    size_t aut_len = (curve_aut == CDC_CURVE_ED25519) ? 32 : 64;

    if (!calculate_fingerprint(pub_sig, sig_len, curve_sig, created_at, fp_sig)) return false;
    if (!calculate_fingerprint(pub_dec, dec_len, curve_dec, created_at, fp_dec)) return false;
    if (!calculate_fingerprint(pub_aut, aut_len, curve_aut, created_at, fp_aut)) return false;

    uint8_t fp_v5[GPG_FINGERPRINT_V5_LEN] = {};
    calculate_fingerprint_v5(pub_sig, sig_len, curve_sig, created_at, fp_v5);

    char existing_user_id[GPG_USER_ID_MAX] = {};
    strncpy(existing_user_id, s_metadata.user_id, sizeof(existing_user_id) - 1);
    memset(&s_metadata, 0, sizeof(s_metadata));
    s_metadata.magic = GPG_METADATA_MAGIC;
    s_metadata.version = GPG_METADATA_VERSION;
    s_metadata.curve = curve_sig;
    if (s_pending_user_id[0]) {
        strncpy(s_metadata.user_id, s_pending_user_id, sizeof(s_metadata.user_id) - 1);
    } else {
        strncpy(s_metadata.user_id, existing_user_id, sizeof(s_metadata.user_id) - 1);
    }
    s_metadata.created_at = created_at;
    memcpy(s_metadata.fingerprint, fp_sig, sizeof(fp_sig));
    memcpy(s_metadata.fingerprint_v5, fp_v5, sizeof(fp_v5));
    memcpy(s_metadata.pubkey, pub_sig, sig_len);
    s_metadata.pubkey_len = static_cast<uint8_t>(sig_len);
    s_metadata.sign_count = 0;

    if (!save_metadata()) {
        return false;
    }

    openpgp_set_key_fingerprint(KEY_SIG, fp_sig, created_at);
    openpgp_set_key_fingerprint(KEY_DEC, fp_dec, created_at);
    openpgp_set_key_fingerprint(KEY_AUT, fp_aut, created_at);

    s_initialized = true;
    s_pending_user_id[0] = '\0';
    return true;
}

/**
 * \brief Removes all GPG keys and metadata from secure element and NVS.
 * \return `true` on success, otherwise `false`.
 */
bool gpg_reset(void) {
    if (!gpg_storage_ready()) return false;
    memset(&s_metadata, 0, sizeof(s_metadata));
    s_initialized = false;
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_erase_key(handle, NVS_KEY_META);
        nvs_commit(handle);
        nvs_close(handle);
    }
    // Wipes ECC slots, DEC privkey, NVS state (sig_count, RC, gen_times,
    // cardholder, fingerprints, selected curves) and PINs.
    openpgp_factory_reset();
    return true;
}

/**
 * \brief Exports the current public key as PEM SubjectPublicKeyInfo.
 * \param buf Destination string buffer.
 * \param size Size of `buf` in bytes.
 * \param out_len Destination for generated PEM length.
 * \return `true` on success, otherwise `false`.
 */
bool gpg_export_pubkey_pem(char *buf, size_t size, size_t *out_len) {
    if (!buf || size < 256 || !out_len) {
        LOG_W("GPG", "export: bad args");
        return false;
    }

    uint8_t pubkey_buf[64] = {0};
    const uint8_t *pubkey = nullptr;
    uint8_t curve = CDC_CURVE_ED25519;

    if (s_initialized && s_metadata.pubkey_len > 0) {
        pubkey = s_metadata.pubkey;
        curve = s_metadata.curve;
    } else if (openpgp_has_any_key() && gpg_storage_ready()) {
        // CCID-pfad: SIG-Pubkey direkt vom Tropic-Slot lesen
        auto* se = cdc::hal::getSecureElementInstance();
        if (!se) {
            LOG_W("GPG", "export: no SE");
            return false;
        }
        cdc::hal::EccCurve eccCurve = cdc::hal::EccCurve::P256;
        uint8_t sigSlot = gpg_storage_sig_slot();
        auto res = se->eccGetPublicKey(sigSlot, pubkey_buf, &eccCurve);
        if (res != cdc::hal::SeResult::OK) {
            LOG_W("GPG", "export: eccGetPublicKey slot=%u failed (%d)",
                  sigSlot, static_cast<int>(res));
            return false;
        }
        pubkey = pubkey_buf;
        curve = (eccCurve == cdc::hal::EccCurve::ED25519) ? CDC_CURVE_ED25519
                                                          : CDC_CURVE_P256;
    } else {
        LOG_W("GPG", "export: no key configured");
        return false;
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
        int ret = mbedtls_base64_encode(NULL, 0, &b64_len, der, sizeof(der));
        if (ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) {
            return false;
        }

        char b64[128];
        ret = mbedtls_base64_encode(reinterpret_cast<unsigned char*>(b64), sizeof(b64), &b64_len,
                                    der, sizeof(der));
        if (ret != 0) {
            return false;
        }

        int written = snprintf(buf, size,
                               "-----BEGIN PUBLIC KEY-----\n%s\n-----END PUBLIC KEY-----\n", b64);
        if (written < 0 || static_cast<size_t>(written) >= size) {
            return false;
        }
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
    int ret = mbedtls_base64_encode(NULL, 0, &b64_len, der, sizeof(der));
    if (ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) {
        return false;
    }

    char b64[256];
    ret = mbedtls_base64_encode(reinterpret_cast<unsigned char*>(b64), sizeof(b64), &b64_len,
                                der, sizeof(der));
    if (ret != 0) {
        return false;
    }

    int written = snprintf(buf, size,
                           "-----BEGIN PUBLIC KEY-----\n%s\n-----END PUBLIC KEY-----\n", b64);
    if (written < 0 || static_cast<size_t>(written) >= size) {
        return false;
    }
    *out_len = static_cast<size_t>(written);
    return true;
}

/**
 * \brief Exports the raw public key bytes and associated curve identifier.
 * \param pubkey Destination buffer for key bytes.
 * \param pubkey_len Destination for key length.
 * \param curve Destination for CDC curve identifier.
 * \return `true` on success, otherwise `false`.
 */
bool gpg_export_pubkey_raw(uint8_t *pubkey, size_t *pubkey_len, uint8_t *curve) {
    if (!pubkey || !pubkey_len || !curve) return false;
    if (!s_initialized) return false;
    memcpy(pubkey, s_metadata.pubkey, s_metadata.pubkey_len);
    *pubkey_len = s_metadata.pubkey_len;
    *curve = s_metadata.curve;
    return true;
}

/**
 * \brief Returns the OpenPGP v4 fingerprint of the current signing key.
 * \param fp_out Destination buffer for fingerprint bytes.
 * \return `true` on success, otherwise `false`.
 */
bool gpg_get_fingerprint(uint8_t *fp_out) {
    if (!fp_out) return false;
    uint8_t fp[GPG_FINGERPRINT_LEN] = {};
    if (openpgp_get_fingerprint(KEY_SIG, fp)) {
        bool all_zero = true;
        for (size_t i = 0; i < sizeof(fp); ++i) {
            if (fp[i] != 0) { all_zero = false; break; }
        }
        if (!all_zero) {
            memcpy(fp_out, fp, sizeof(fp));
            return true;
        }
    }
    if (!s_initialized) return false;
    memcpy(fp_out, s_metadata.fingerprint, sizeof(s_metadata.fingerprint));
    return true;
}

/**
 * \brief Returns the OpenPGP v5 fingerprint of the current signing key.
 * \param fp_out Destination buffer for fingerprint bytes.
 * \return `true` on success, otherwise `false`.
 */
bool gpg_get_fingerprint_v5(uint8_t *fp_out) {
    if (!fp_out || !s_initialized) return false;
    memcpy(fp_out, s_metadata.fingerprint_v5, sizeof(s_metadata.fingerprint_v5));
    return true;
}

/**
 * \brief Signs input data using the currently active signing key.
 * \param hash Input data or digest to sign.
 * \param hash_len Length of `hash` in bytes.
 * \param sig_out Destination buffer for signature bytes.
 * \param sig_len In/out signature length value.
 * \return `true` on success, otherwise `false`.
 */
bool gpg_sign_hash(const uint8_t *hash, size_t hash_len,
                   uint8_t *sig_out, size_t *sig_len) {
    if (!hash || !sig_out || !sig_len) return false;
    if (!s_initialized) return false;

    if (s_metadata.curve == CDC_CURVE_P256) {
        return se_sign_p256(gpg_storage_sig_slot(), hash, hash_len, sig_out, sig_len);
    }
    if (hash_len == 0) return false;
    bool ok = se_sign_ed25519(gpg_storage_sig_slot(), hash, hash_len, sig_out);
    if (ok) {
        *sig_len = 64;
    }
    return ok;
}
