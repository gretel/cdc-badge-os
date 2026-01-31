#include "mod_gpg/gpg.h"
#include "mod_gpg/GpgStorage.h"
#include "mod_gpg/openpgp/openpgp.h"
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

static uint32_t get_unix_time(void) {
    return static_cast<uint32_t>(time(nullptr));
}

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

static bool se_generate_key(uint8_t slot, uint8_t curve) {
    auto* se = cdc::hal::getSecureElementInstance();
    if (!se) return false;
    cdc::hal::EccCurve c = (curve == CDC_CURVE_ED25519) ? cdc::hal::EccCurve::ED25519
                                                         : cdc::hal::EccCurve::P256;
    return se->eccGenerate(slot, c) == cdc::hal::SeResult::OK;
}

static bool se_delete_key(uint8_t slot) {
    auto* se = cdc::hal::getSecureElementInstance();
    if (!se) return false;
    return se->eccDelete(slot) == cdc::hal::SeResult::OK;
}

static bool se_sign_p256(uint8_t slot, const uint8_t* hash, size_t hash_len, uint8_t* sig, size_t* sig_len) {
    auto* se = cdc::hal::getSecureElementInstance();
    if (!se || !hash || !sig || !sig_len) return false;
    return se->ecdsaSign(slot, hash, hash_len, sig, sig_len) == cdc::hal::SeResult::OK;
}

static bool se_sign_ed25519(uint8_t slot, const uint8_t* msg, size_t msg_len, uint8_t* sig) {
    auto* se = cdc::hal::getSecureElementInstance();
    if (!se || !msg || !sig) return false;
    return se->eddsaSign(slot, msg, msg_len, sig) == cdc::hal::SeResult::OK;
}

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

static bool calculate_fingerprint(const uint8_t *pubkey, size_t pubkey_len,
                                  uint8_t curve, uint32_t created_at,
                                  uint8_t *fp_out) {
    if (!pubkey || !fp_out) return false;

    uint8_t algo = (curve == CDC_CURVE_ED25519) ? 22 : 19;
    static const uint8_t oid_ed25519[] = {0x09, 0x2B, 0x06, 0x01, 0x04, 0x01, 0xDA, 0x47, 0x0F, 0x01};
    static const uint8_t oid_p256[] = {0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07};

    const uint8_t *oid = (curve == CDC_CURVE_ED25519) ? oid_ed25519 : oid_p256;
    size_t oid_len = (curve == CDC_CURVE_ED25519) ? sizeof(oid_ed25519) : sizeof(oid_p256);

    uint8_t mpi[67];
    size_t mpi_len = 0;

    if (curve == CDC_CURVE_ED25519) {
        uint16_t bits = 256;
        if ((pubkey[0] & 0x80) == 0) bits = 255;
        mpi[0] = (bits >> 8) & 0xFF;
        mpi[1] = bits & 0xFF;
        memcpy(mpi + 2, pubkey, 32);
        mpi_len = 34;
    } else {
        uint16_t bits = 520;
        mpi[0] = (bits >> 8) & 0xFF;
        mpi[1] = bits & 0xFF;
        memcpy(mpi + 2, pubkey, 65);
        mpi_len = 67;
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

    uint8_t sha[20];
    mbedtls_sha1_context sha1;
    mbedtls_sha1_init(&sha1);
    mbedtls_sha1_starts(&sha1);
    mbedtls_sha1_update(&sha1, prefix, sizeof(prefix));
    mbedtls_sha1_update(&sha1, body, body_len);
    mbedtls_sha1_finish(&sha1, sha);
    mbedtls_sha1_free(&sha1);
    memcpy(fp_out, sha, 20);
    return true;
}

static bool calculate_fingerprint_v5(const uint8_t *pubkey, size_t pubkey_len,
                                     uint8_t curve, uint32_t created_at,
                                     uint8_t *fp_out) {
    if (!pubkey || !fp_out) return false;

    uint8_t algo = (curve == CDC_CURVE_ED25519) ? 22 : 19;
    static const uint8_t oid_ed25519[] = {0x09, 0x2B, 0x06, 0x01, 0x04, 0x01, 0xDA, 0x47, 0x0F, 0x01};
    static const uint8_t oid_p256[] = {0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07};

    const uint8_t *oid = (curve == CDC_CURVE_ED25519) ? oid_ed25519 : oid_p256;
    size_t oid_len = (curve == CDC_CURVE_ED25519) ? sizeof(oid_ed25519) : sizeof(oid_p256);

    uint8_t mpi[67];
    size_t mpi_len = 0;

    if (curve == CDC_CURVE_ED25519) {
        uint16_t bits = 256;
        if ((pubkey[0] & 0x80) == 0) bits = 255;
        mpi[0] = (bits >> 8) & 0xFF;
        mpi[1] = bits & 0xFF;
        memcpy(mpi + 2, pubkey, 32);
        mpi_len = 34;
    } else {
        uint16_t bits = 520;
        mpi[0] = (bits >> 8) & 0xFF;
        mpi[1] = bits & 0xFF;
        memcpy(mpi + 2, pubkey, 65);
        mpi_len = 67;
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

    uint8_t sha[32];
    mbedtls_sha256_context sha256;
    mbedtls_sha256_init(&sha256);
    mbedtls_sha256_starts(&sha256, 0);
    mbedtls_sha256_update(&sha256, prefix, sizeof(prefix));
    mbedtls_sha256_update(&sha256, body, body_len);
    mbedtls_sha256_finish(&sha256, sha);
    mbedtls_sha256_free(&sha256);
    memcpy(fp_out, sha, 32);
    return true;
}

bool gpg_init(void) {
    if (!gpg_storage_ready()) {
        return false;
    }
    bool loaded = load_metadata();
    s_initialized = loaded;
    return true;
}

bool gpg_is_initialized(void) {
    return s_initialized;
}

bool gpg_get_status(gpg_status_t *status) {
    if (!status) return false;
    if (!s_initialized) return false;
    memset(status, 0, sizeof(*status));
    status->initialized = true;
    status->curve = s_metadata.curve;
    strncpy(status->user_id, s_metadata.user_id, sizeof(status->user_id) - 1);
    memcpy(status->fingerprint, s_metadata.fingerprint, sizeof(status->fingerprint));
    status->created_at = s_metadata.created_at;
    status->sign_count = s_metadata.sign_count;
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

bool gpg_reset(void) {
    if (!gpg_storage_ready()) return false;
    se_delete_key(gpg_storage_sig_slot());
    se_delete_key(gpg_storage_dec_slot());
    se_delete_key(gpg_storage_aut_slot());
    uint8_t zero_fp[GPG_FINGERPRINT_LEN] = {};
    openpgp_set_key_fingerprint(KEY_SIG, zero_fp, 0);
    openpgp_set_key_fingerprint(KEY_DEC, zero_fp, 0);
    openpgp_set_key_fingerprint(KEY_AUT, zero_fp, 0);
    memset(&s_metadata, 0, sizeof(s_metadata));
    s_initialized = false;
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_erase_key(handle, NVS_KEY_META);
        nvs_commit(handle);
        nvs_close(handle);
    }
    return true;
}

bool gpg_export_pubkey_pem(char *buf, size_t size, size_t *out_len) {
    if (!buf || size < 256 || !out_len) {
        return false;
    }
    if (!s_initialized) {
        return false;
    }

    const uint8_t *pubkey = s_metadata.pubkey;

    if (s_metadata.curve == CDC_CURVE_ED25519) {
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

bool gpg_export_pubkey_raw(uint8_t *pubkey, size_t *pubkey_len, uint8_t *curve) {
    if (!pubkey || !pubkey_len || !curve) return false;
    if (!s_initialized) return false;
    memcpy(pubkey, s_metadata.pubkey, s_metadata.pubkey_len);
    *pubkey_len = s_metadata.pubkey_len;
    *curve = s_metadata.curve;
    return true;
}

bool gpg_get_fingerprint(uint8_t *fp_out) {
    if (!fp_out || !s_initialized) return false;
    memcpy(fp_out, s_metadata.fingerprint, sizeof(s_metadata.fingerprint));
    return true;
}

bool gpg_get_fingerprint_v5(uint8_t *fp_out) {
    if (!fp_out || !s_initialized) return false;
    memcpy(fp_out, s_metadata.fingerprint_v5, sizeof(s_metadata.fingerprint_v5));
    return true;
}

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
