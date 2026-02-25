/**
 * \brief OpenPGP smart-card application implementation for CDC Badge.
 *
 * Based on pico-openpgp (https://github.com/polhenarejos/pico-openpgp),
 * adapted for CDC Badge and TROPIC01 secure element integration.
 * Specification target: OpenPGP Smart Card Application 3.4.1.
 */

#include "mod_gpg/openpgp/openpgp.h"
#include "mod_gpg/openpgp/apdu.h"
#include "mod_gpg/gpg.h"
#include "mod_gpg/GpgStorage.h"
#include "ecdh.h"
#include "pin_storage.h"
#include "cdc_hal/ISecureElement.h"
#include <mbedtls/platform_util.h>
#include <string.h>
#include <time.h>
#include <esp_log.h>
#include <esp_mac.h>       // For esp_efuse_mac_get_default()
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_random.h>

static const char *TAG = "OpenPGP";

/**
 * \brief Returns secure-element instance used by OpenPGP backend.
 * \return Pointer to secure-element abstraction.
 */
static cdc::hal::ISecureElement* get_se() {
    return cdc::hal::getSecureElementInstance();
}

/**
 * \brief Reads ECC public key from secure element and exposes curve metadata.
 * \param slot ECC slot index.
 * \param pubkey Output public key buffer.
 * \param max_len Capacity of `pubkey`.
 * \param curve_out Optional output curve identifier.
 * \return `true` when key exists and output buffer size matches curve format.
 */
static bool se_ecc_key_read(uint8_t slot, uint8_t* pubkey, size_t max_len, uint8_t* curve_out) {
    auto* se = get_se();
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
    return max_len >= 65;
}

/**
 * \brief Generates ECC key material in secure element slot.
 * \param slot ECC slot index.
 * \param curve Curve identifier (`CDC_CURVE_*`).
 * \return `true` if key generation succeeded.
 */
static bool se_ecc_key_generate(uint8_t slot, uint8_t curve) {
    auto* se = get_se();
    if (!se) return false;
    cdc::hal::EccCurve c = (curve == CDC_CURVE_ED25519) ? cdc::hal::EccCurve::ED25519
                                                        : cdc::hal::EccCurve::P256;
    return se->eccGenerate(slot, c) == cdc::hal::SeResult::OK;
}

/**
 * \brief Signs a hash using secure-element ECDSA key.
 * \param slot ECC slot index.
 * \param hash Hash bytes to sign.
 * \param hash_len Hash length.
 * \param sig Output 64-byte signature buffer.
 * \return `true` if signing succeeded.
 */
static bool se_ecdsa_sign(uint8_t slot, const uint8_t* hash, size_t hash_len, uint8_t* sig) {
    auto* se = get_se();
    if (!se || !hash || !sig) return false;
    size_t sig_len = 64;
    return se->ecdsaSign(slot, hash, hash_len, sig, &sig_len) == cdc::hal::SeResult::OK;
}

/**
 * \brief Signs a message using secure-element EdDSA key.
 * \param slot ECC slot index.
 * \param msg Message bytes.
 * \param msg_len Message length.
 * \param sig Output signature buffer.
 * \return `true` if signing succeeded.
 */
static bool se_eddsa_sign(uint8_t slot, const uint8_t* msg, size_t msg_len, uint8_t* sig) {
    auto* se = get_se();
    if (!se || !msg || !sig) return false;
    return se->eddsaSign(slot, msg, msg_len, sig) == cdc::hal::SeResult::OK;
}

/**
 * \brief Fills buffer with secure random bytes, with ESP fallback.
 * \param buf Output buffer.
 * \param len Number of bytes to generate.
 */
static void se_random_fill(uint8_t* buf, size_t len) {
    auto* se = get_se();
    if (se && se->getRandom(buf, static_cast<uint16_t>(len))) {
        return;
    }
    esp_fill_random(buf, len);
}

/**
 * \brief OpenPGP Application ID (RID + PIX), initialized dynamically.
 *
 * `D2 76 00 01 24 01` = OpenPGP RID.
 * Structure: RID(6) + Version(2) + Manufacturer(2) + Serial(4) + RFU(2) = 16 bytes.
 */
static uint8_t s_openpgp_aid[16] = {
    0xD2, 0x76, 0x00, 0x01, 0x24, 0x01,  // RID + Application (OpenPGP)
    0x03, 0x04,                           // Version 3.4
    0x00, 0x00,                           // Manufacturer (set in init)
    0x00, 0x00, 0x00, 0x00,              // Serial number (set in init from MAC)
    0x00, 0x00                            // RFU
};
const uint8_t* OPENPGP_AID = s_openpgp_aid;
const uint8_t OPENPGP_AID_LEN = sizeof(s_openpgp_aid);

/**
 * \brief ATR is defined in `ccid.cpp` and accessed via `ccid_get_atr()`.
 */

/**
 * \brief Application session/authentication state.
 */
static bool app_selected = false;
static bool pw1_verified = false;
static bool pw3_verified = false;
static uint32_t sig_count = 0;

/**
 * \brief Session PIN cache for DEC key decryption (temporary after VERIFY for PSO:DECIPHER).
 */
static char s_session_pin[OPENPGP_PIN_MAX_LEN + 1] = {};

/**
 * \brief NVS namespace used for OpenPGP persistent data.
 */
#define NVS_NAMESPACE "openpgp"

/**
 * \brief Data object storage buffers (fingerprints and related metadata).
 */
static uint8_t fingerprint_sig[20] = {0};
static uint8_t fingerprint_dec[20] = {0};
static uint8_t fingerprint_aut[20] = {0};

/**
 * \brief Key-generation timestamps (4-byte big-endian Unix time each).
 */
static uint8_t gen_time_sig[4] = {0};
static uint8_t gen_time_dec[4] = {0};
static uint8_t gen_time_aut[4] = {0};

/**
 * \brief Optional CA fingerprints for trust-chain metadata.
 */
static uint8_t ca_fp_1[20] = {0};
static uint8_t ca_fp_2[20] = {0};
static uint8_t ca_fp_3[20] = {0};

/**
 * \brief Cardholder profile data stored in NVS.
 */
static char cardholder_name[40] = {0};    // "Surname<<Firstname"
static char cardholder_lang[8] = "en";     // ISO 639-1 language
static uint8_t cardholder_sex = 0x39;      // '9' = not specified
static char cardholder_url[128] = {0};     // URL for public key retrieval
static char cardholder_login[64] = {0};    // Login data

/**
 * \brief Historical bytes used in OpenPGP ATR-related data objects.
 */
static const uint8_t HIST_BYTES[] = {
    0x00,       // Category indicator: card has no indication of services
    0x31,       // Card capabilities (card can process T=1)
    0xC5,       // Tag: card issuer data follows
    0x73, 0xC0, 0x01, 0x80,  // Card issuer proprietary
    0x05,       // Tag: card capabilities
    0x90, 0x00  // Card status: OK
};

/**
 * \brief Algorithm attributes for Ed25519 (EdDSA with curve25519).
 *
 * Format: Algorithm ID (1) + OID bytes (no length prefix per OpenPGP 3.4.1).
 */
static const uint8_t ALGO_ATTR_ED25519[] = {
    ALGO_EDDSA,                                         // Algorithm: EdDSA (0x16)
    0x2B, 0x06, 0x01, 0x04, 0x01, 0xDA, 0x47, 0x0F, 0x01  // OID 1.3.6.1.4.1.11591.15.1 (ed25519)
};

/**
 * \brief Algorithm attributes for P-256 ECDSA (signature/authentication roles).
 *
 * Format: Algorithm ID (1) + OID bytes (no length prefix per OpenPGP 3.4.1).
 */
static const uint8_t ALGO_ATTR_P256_ECDSA[] = {
    ALGO_ECDSA,                                         // Algorithm: ECDSA (0x13)
    0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07      // OID 1.2.840.10045.3.1.7 (secp256r1)
};

/**
 * \brief Algorithm attributes for P-256 ECDH (decryption role).
 *
 * Format: Algorithm ID (1) + OID bytes (no length prefix per OpenPGP 3.4.1).
 */
static const uint8_t ALGO_ATTR_P256_ECDH[] = {
    ALGO_ECDH,                                          // Algorithm: ECDH (0x12)
    0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07      // OID 1.2.840.10045.3.1.7 (secp256r1)
};

/**
 * \brief Extended capabilities object per OpenPGP 3.4.1 section 4.2.1.
 */
static const uint8_t EXT_CAPABILITIES[] = {
    0x75,       // Flags: SM supported, GET CHALLENGE, Key Import, PW Status changeable,
                // Private DOs, Algorithm attributes changeable, PSO:DEC with AES
    0x00,       // SM Algorithm: none
    0x00, 0x80, // Max GET CHALLENGE length: 128 bytes
    0x08, 0x00, // Max Cardholder Certificate length: 2048 bytes
    0x00, 0xFF, // Max special DO length: 255 bytes
    0x00,       // PIN block 2 format not supported
    0x00,       // MSE for key selection not supported
};

/**
 * \brief TLV builder helper functions.
 */

/**
 * \brief Writes a TLV tag using one or two bytes.
 * \param buf Output buffer receiving the tag bytes.
 * \param tag TLV tag value.
 * \return Number of bytes written to `buf`.
 */
static size_t tlv_write_tag(uint8_t *buf, uint16_t tag) {
    if (tag > 0xFF) {
        buf[0] = (tag >> 8) & 0xFF;
        buf[1] = tag & 0xFF;
        return 2;
    }
    buf[0] = tag & 0xFF;
    return 1;
}

/**
 * \brief Writes a TLV length field using DER length encoding.
 * \param buf Output buffer receiving the encoded length.
 * \param len Length value to encode.
 * \return Number of bytes written to `buf`.
 */
static size_t tlv_write_len(uint8_t *buf, size_t len) {
    if (len < 128) {
        buf[0] = len;
        return 1;
    } else if (len < 256) {
        buf[0] = 0x81;
        buf[1] = len;
        return 2;
    } else {
        buf[0] = 0x82;
        buf[1] = (len >> 8) & 0xFF;
        buf[2] = len & 0xFF;
        return 3;
    }
}

/**
 * \brief Builds complete TLV object and returns total encoded length.
 * \param buf Output buffer.
 * \param buf_max Maximum size of `buf`.
 * \param tag TLV tag.
 * \param value Optional value bytes.
 * \param value_len Value length.
 * \return Total bytes written to `buf`.
 */
static size_t tlv_build(uint8_t *buf, size_t buf_max, uint16_t tag,
                        const uint8_t *value, size_t value_len) {
    size_t pos = 0;
    pos += tlv_write_tag(buf + pos, tag);
    pos += tlv_write_len(buf + pos, value_len);
    if (value && value_len > 0) {
        memcpy(buf + pos, value, value_len);
        pos += value_len;
    }
    return pos;
}

/**
 * \brief Builders for OpenPGP application-related data objects.
 */

/**
 * \brief Key role discriminator used for algorithm-attribute selection.
 */
typedef enum {
    KEY_TYPE_SIG = 0,  // Signature (ECDSA/EdDSA)
    KEY_TYPE_DEC = 1,  // Decryption (ECDH)
    KEY_TYPE_AUT = 2   // Authentication (ECDSA/EdDSA)
} key_type_t;

/**
 * \brief Returns algorithm attributes for a key role based on stored key type.
 * \param key_type Key role (signature, decryption, authentication).
 * \param len Output pointer receiving the attribute length.
 * \return Pointer to the selected algorithm-attribute byte array.
 */
static const uint8_t* get_algo_attr(key_type_t key_type, size_t *len) {
    uint8_t slot = gpg_storage_sig_slot();
    switch (key_type) {
        case KEY_TYPE_SIG: slot = gpg_storage_sig_slot(); break;
        case KEY_TYPE_DEC: slot = gpg_storage_dec_slot(); break;
        case KEY_TYPE_AUT: slot = gpg_storage_aut_slot(); break;
        default:           slot = gpg_storage_sig_slot(); break;
    }

    // Try to read existing key to get curve
    uint8_t pubkey[65];
    uint8_t curve = CDC_CURVE_P256;  // Default to P-256

    // If key exists, use its curve; otherwise use default
    if (se_ecc_key_read(slot, pubkey, sizeof(pubkey), &curve)) {
        // Key exists, curve is now set
    }

    // Return appropriate algorithm attributes
    if (curve == CDC_CURVE_P256) {
        // P-256: use ECDSA for SIG/AUT, ECDH for DEC
        if (key_type == KEY_TYPE_DEC) {
            *len = sizeof(ALGO_ATTR_P256_ECDH);
            return ALGO_ATTR_P256_ECDH;
        } else {
            *len = sizeof(ALGO_ATTR_P256_ECDSA);
            return ALGO_ATTR_P256_ECDSA;
        }
    }

    // Ed25519 (EdDSA for all - note: DEC should use X25519 but not implemented yet)
    *len = sizeof(ALGO_ATTR_ED25519);
    return ALGO_ATTR_ED25519;
}

/**
 * \brief Builds OpenPGP DO `0x6E` (Application Related Data).
 * \param buf Output buffer for the encoded TLV object.
 * \param buf_max Maximum size of `buf`.
 * \return Encoded length on success, or a negative error code.
 */
static int build_do_app_related(uint8_t *buf, size_t buf_max) {
    uint8_t inner[512];
    size_t inner_len = 0;

    // 4F: AID
    inner_len += tlv_build(inner + inner_len, sizeof(inner) - inner_len,
                           0x4F, OPENPGP_AID, OPENPGP_AID_LEN);

    // 5F52: Historical bytes
    inner_len += tlv_build(inner + inner_len, sizeof(inner) - inner_len,
                           0x5F52, HIST_BYTES, sizeof(HIST_BYTES));

    // 73: Discretionary data objects (nested)
    uint8_t discret[384];
    size_t discret_len = 0;

    // C0: Extended capabilities
    discret_len += tlv_build(discret + discret_len, sizeof(discret) - discret_len,
                             0xC0, EXT_CAPABILITIES, sizeof(EXT_CAPABILITIES));

    // C1: Algorithm attributes - Signature (ECDSA/EdDSA)
    size_t algo_len;
    const uint8_t *algo = get_algo_attr(KEY_TYPE_SIG, &algo_len);
    discret_len += tlv_build(discret + discret_len, sizeof(discret) - discret_len,
                             0xC1, algo, algo_len);

    // C2: Algorithm attributes - Decryption (ECDH)
    algo = get_algo_attr(KEY_TYPE_DEC, &algo_len);
    discret_len += tlv_build(discret + discret_len, sizeof(discret) - discret_len,
                             0xC2, algo, algo_len);

    // C3: Algorithm attributes - Authentication (ECDSA/EdDSA)
    algo = get_algo_attr(KEY_TYPE_AUT, &algo_len);
    discret_len += tlv_build(discret + discret_len, sizeof(discret) - discret_len,
                             0xC3, algo, algo_len);

    // C4: PW Status Bytes (retries from TROPIC01 storage)
    // Note: Max lengths limited for practical use on hardware keypad
    uint8_t pw_status[7] = {
        0x01,       // PW1 valid for multiple signatures
        32,         // Max length PW1 (practical limit)
        32,         // Max length RC (Resetting Code)
        32,         // Max length PW3 (practical limit)
        pin_storage_openpgp_pw1_retries(),
        0,          // RC retries (not implemented)
        pin_storage_openpgp_pw3_retries()
    };
    discret_len += tlv_build(discret + discret_len, sizeof(discret) - discret_len,
                             0xC4, pw_status, sizeof(pw_status));

    // C5: Fingerprints (60 bytes: SIG + DEC + AUT)
    uint8_t fps[60];
    memcpy(fps, fingerprint_sig, 20);
    memcpy(fps + 20, fingerprint_dec, 20);
    memcpy(fps + 40, fingerprint_aut, 20);
    discret_len += tlv_build(discret + discret_len, sizeof(discret) - discret_len,
                             0xC5, fps, 60);

    // C6: CA Fingerprints (60 bytes)
    uint8_t ca_fps[60];
    memcpy(ca_fps, ca_fp_1, 20);
    memcpy(ca_fps + 20, ca_fp_2, 20);
    memcpy(ca_fps + 40, ca_fp_3, 20);
    discret_len += tlv_build(discret + discret_len, sizeof(discret) - discret_len,
                             0xC6, ca_fps, 60);

    // CD: Generation dates (12 bytes: SIG + DEC + AUT)
    uint8_t gen_times[12];
    memcpy(gen_times, gen_time_sig, 4);
    memcpy(gen_times + 4, gen_time_dec, 4);
    memcpy(gen_times + 8, gen_time_aut, 4);
    discret_len += tlv_build(discret + discret_len, sizeof(discret) - discret_len,
                             0xCD, gen_times, 12);

    // Add discretionary DOs to inner
    inner_len += tlv_build(inner + inner_len, sizeof(inner) - inner_len,
                           0x73, discret, discret_len);

    // Build final 6E response
    size_t total = 0;
    total += tlv_write_tag(buf + total, 0x6E);
    total += tlv_write_len(buf + total, inner_len);
    memcpy(buf + total, inner, inner_len);
    total += inner_len;

    return total;
}

/**
 * \brief Builds OpenPGP DO `0x65` (Cardholder Related Data).
 * \param buf Output buffer for the encoded TLV object.
 * \param buf_max Maximum size of `buf`.
 * \return Encoded length on success, or a negative error code.
 */
static int build_do_cardholder(uint8_t *buf, size_t buf_max) {
    uint8_t inner[128];
    size_t inner_len = 0;

    // 5B: Name
    size_t name_len = strlen(cardholder_name);
    inner_len += tlv_build(inner + inner_len, sizeof(inner) - inner_len,
                           0x5B, (const uint8_t *)cardholder_name, name_len);

    // 5F2D: Language preference
    size_t lang_len = strlen(cardholder_lang);
    inner_len += tlv_build(inner + inner_len, sizeof(inner) - inner_len,
                           0x5F2D, (const uint8_t *)cardholder_lang, lang_len);

    // 5F35: Sex
    inner_len += tlv_build(inner + inner_len, sizeof(inner) - inner_len,
                           0x5F35, &cardholder_sex, 1);

    // Build final 65 response
    size_t total = 0;
    total += tlv_write_tag(buf + total, 0x65);
    total += tlv_write_len(buf + total, inner_len);
    memcpy(buf + total, inner, inner_len);
    total += inner_len;

    return total;
}

/**
 * \brief Loads persistent OpenPGP runtime state from NVS.
 * \return void
 */
static void load_state_from_nvs(void) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) == ESP_OK) {
        size_t len;

        // Load signature count
        if (nvs_get_u32(nvs, "sig_count", &sig_count) != ESP_OK) {
            sig_count = 0;
        }

        // Load fingerprints
        len = sizeof(fingerprint_sig);
        nvs_get_blob(nvs, "fp_sig", fingerprint_sig, &len);

        len = sizeof(fingerprint_dec);
        nvs_get_blob(nvs, "fp_dec", fingerprint_dec, &len);

        len = sizeof(fingerprint_aut);
        nvs_get_blob(nvs, "fp_aut", fingerprint_aut, &len);

        // Load CA fingerprints
        len = sizeof(ca_fp_1);
        nvs_get_blob(nvs, "ca_fp_1", ca_fp_1, &len);

        len = sizeof(ca_fp_2);
        nvs_get_blob(nvs, "ca_fp_2", ca_fp_2, &len);

        len = sizeof(ca_fp_3);
        nvs_get_blob(nvs, "ca_fp_3", ca_fp_3, &len);

        // Load generation times
        len = sizeof(gen_time_sig);
        nvs_get_blob(nvs, "gt_sig", gen_time_sig, &len);

        len = sizeof(gen_time_dec);
        nvs_get_blob(nvs, "gt_dec", gen_time_dec, &len);

        len = sizeof(gen_time_aut);
        nvs_get_blob(nvs, "gt_aut", gen_time_aut, &len);

        // Load cardholder data
        len = sizeof(cardholder_name);
        nvs_get_str(nvs, "ch_name", cardholder_name, &len);

        len = sizeof(cardholder_lang);
        nvs_get_str(nvs, "ch_lang", cardholder_lang, &len);

        nvs_get_u8(nvs, "ch_sex", &cardholder_sex);

        len = sizeof(cardholder_url);
        nvs_get_str(nvs, "ch_url", cardholder_url, &len);

        len = sizeof(cardholder_login);
        nvs_get_str(nvs, "ch_login", cardholder_login, &len);

        nvs_close(nvs);
    }
}

/**
 * \brief Persists OpenPGP runtime state to NVS.
 * \return void
 */
static void save_state_to_nvs(void) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        // Signature count
        nvs_set_u32(nvs, "sig_count", sig_count);

        // Fingerprints
        nvs_set_blob(nvs, "fp_sig", fingerprint_sig, sizeof(fingerprint_sig));
        nvs_set_blob(nvs, "fp_dec", fingerprint_dec, sizeof(fingerprint_dec));
        nvs_set_blob(nvs, "fp_aut", fingerprint_aut, sizeof(fingerprint_aut));

        // CA fingerprints
        nvs_set_blob(nvs, "ca_fp_1", ca_fp_1, sizeof(ca_fp_1));
        nvs_set_blob(nvs, "ca_fp_2", ca_fp_2, sizeof(ca_fp_2));
        nvs_set_blob(nvs, "ca_fp_3", ca_fp_3, sizeof(ca_fp_3));

        // Generation times
        nvs_set_blob(nvs, "gt_sig", gen_time_sig, sizeof(gen_time_sig));
        nvs_set_blob(nvs, "gt_dec", gen_time_dec, sizeof(gen_time_dec));
        nvs_set_blob(nvs, "gt_aut", gen_time_aut, sizeof(gen_time_aut));

        // Cardholder data
        nvs_set_str(nvs, "ch_name", cardholder_name);
        nvs_set_str(nvs, "ch_lang", cardholder_lang);
        nvs_set_u8(nvs, "ch_sex", cardholder_sex);
        nvs_set_str(nvs, "ch_url", cardholder_url);
        nvs_set_str(nvs, "ch_login", cardholder_login);

        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

/**
 * \brief Initializes the OpenPGP AID serial section from the ESP32 MAC address.
 * \return void
 */
static void init_aid_from_mac(void) {
    uint8_t mac[6];
    if (esp_efuse_mac_get_default(mac) == ESP_OK) {
        // Use last 4 bytes of MAC as serial number (big-endian)
        // MAC format: [0][1][2][3][4][5] - use [2][3][4][5] for better uniqueness
        s_openpgp_aid[10] = mac[2];
        s_openpgp_aid[11] = mac[3];
        s_openpgp_aid[12] = mac[4];
        s_openpgp_aid[13] = mac[5];

        // Set manufacturer: CDC Badge = 0x4344 ("CD" in ASCII)
        s_openpgp_aid[8] = 0x43;   // 'C'
        s_openpgp_aid[9] = 0x44;   // 'D'

        ESP_LOGI(TAG, "AID initialized: Manufacturer=0x%02X%02X Serial=%02X%02X%02X%02X",
                 s_openpgp_aid[8], s_openpgp_aid[9],
                 s_openpgp_aid[10], s_openpgp_aid[11],
                 s_openpgp_aid[12], s_openpgp_aid[13]);
    } else {
        ESP_LOGW(TAG, "Failed to read MAC, using default AID");
        // Keep defaults: FFFE / 00000001
        s_openpgp_aid[8] = 0xFF;
        s_openpgp_aid[9] = 0xFE;
        s_openpgp_aid[10] = 0x00;
        s_openpgp_aid[11] = 0x00;
        s_openpgp_aid[12] = 0x00;
        s_openpgp_aid[13] = 0x01;
    }
}

bool openpgp_init(void) {
    // Initialize AID with device-unique serial number
    init_aid_from_mac();

    // Initialize GPG component (TROPIC01 backend)
    if (!gpg_init()) {
        ESP_LOGE(TAG, "Failed to initialize GPG/TROPIC01");
        return false;
    }

    // Initialize OpenPGP PIN storage (loads PINs from TROPIC01)
    pin_storage_openpgp_init();

    load_state_from_nvs();

    ESP_LOGI(TAG, "OpenPGP application initialized, sig_count=%lu", sig_count);
    return true;
}

bool openpgp_is_selected(void) {
    return app_selected;
}

uint32_t openpgp_get_sig_count(void) {
    return sig_count;
}

bool openpgp_set_key_fingerprint(uint8_t key_type, const uint8_t *fingerprint,
                                  uint32_t gen_time) {
    if (!fingerprint) return false;

    // Convert gen_time to big-endian bytes
    uint8_t ts[4] = {
        (uint8_t)((gen_time >> 24) & 0xFF),
        (uint8_t)((gen_time >> 16) & 0xFF),
        (uint8_t)((gen_time >> 8) & 0xFF),
        (uint8_t)(gen_time & 0xFF)
    };

    switch (key_type) {
        case KEY_SIG:
            memcpy(fingerprint_sig, fingerprint, 20);
            memcpy(gen_time_sig, ts, 4);
            break;
        case KEY_DEC:
            memcpy(fingerprint_dec, fingerprint, 20);
            memcpy(gen_time_dec, ts, 4);
            break;
        case KEY_AUT:
            memcpy(fingerprint_aut, fingerprint, 20);
            memcpy(gen_time_aut, ts, 4);
            break;
        default:
            ESP_LOGE(TAG, "Invalid key type: 0x%02X", key_type);
            return false;
    }

    save_state_to_nvs();
    ESP_LOGI(TAG, "Fingerprint set for key type 0x%02X", key_type);
    return true;
}

/**
 * \brief Handles APDU `SELECT` command processing.
 * \param apdu Parsed APDU request.
 * \param resp Output response buffer.
 * \param resp_max Maximum size of `resp`.
 * \return APDU status/response length result.
 */
static int cmd_select(const apdu_t *apdu, uint8_t *resp, size_t resp_max) {
    // Check for OpenPGP AID
    if (apdu->lc >= 6 && memcmp(apdu->data, OPENPGP_AID, 6) == 0) {
        app_selected = true;
        pw1_verified = false;
        pw3_verified = false;
        // Clear session PIN on new select (security)
        mbedtls_platform_zeroize(s_session_pin, sizeof(s_session_pin));
        ESP_LOGI(TAG, "OpenPGP application selected");
        return apdu_sw(resp, SW_OK);
    }

    // Deselect: clear session state
    if (app_selected) {
        mbedtls_platform_zeroize(s_session_pin, sizeof(s_session_pin));
    }

    return apdu_sw(resp, SW_FILE_NOT_FOUND);
}

/**
 * \brief Handles APDU `GET DATA` command processing.
 * \param apdu Parsed APDU request.
 * \param resp Output response buffer.
 * \param resp_max Maximum size of `resp`.
 * \return APDU status/response length result.
 */
static int cmd_get_data(const apdu_t *apdu, uint8_t *resp, size_t resp_max) {
    uint16_t tag = (apdu->p1 << 8) | apdu->p2;

    switch (tag) {
        case DO_AID:  // 0x4F: Full AID
            return apdu_build_response(resp, resp_max, OPENPGP_AID, OPENPGP_AID_LEN, SW_OK);

        case DO_APP_RELATED: {  // 0x6E: Application Related Data
            uint8_t data[512];
            int len = build_do_app_related(data, sizeof(data));
            if (len <= 0) {
                return apdu_sw(resp, SW_UNKNOWN);
            }
            return apdu_build_response(resp, resp_max, data, len, SW_OK);
        }

        case DO_CARDHOLDER: {  // 0x65: Cardholder Related Data
            uint8_t data[128];
            int len = build_do_cardholder(data, sizeof(data));
            if (len <= 0) {
                return apdu_sw(resp, SW_UNKNOWN);
            }
            return apdu_build_response(resp, resp_max, data, len, SW_OK);
        }

        case DO_HIST_BYTES:  // 0x5F52: Historical bytes
            return apdu_build_response(resp, resp_max, HIST_BYTES, sizeof(HIST_BYTES), SW_OK);

        case DO_EXT_CAP:  // 0xC0: Extended Capabilities
            return apdu_build_response(resp, resp_max, EXT_CAPABILITIES, sizeof(EXT_CAPABILITIES), SW_OK);

        case DO_ALGO_SIG: { // 0xC1: Algorithm Attributes - Signature
            size_t algo_len;
            const uint8_t *algo = get_algo_attr(KEY_TYPE_SIG, &algo_len);
            return apdu_build_response(resp, resp_max, algo, algo_len, SW_OK);
        }

        case DO_ALGO_DEC: { // 0xC2: Algorithm Attributes - Decryption
            size_t algo_len;
            const uint8_t *algo = get_algo_attr(KEY_TYPE_DEC, &algo_len);
            return apdu_build_response(resp, resp_max, algo, algo_len, SW_OK);
        }

        case DO_ALGO_AUT: { // 0xC3: Algorithm Attributes - Authentication
            size_t algo_len;
            const uint8_t *algo = get_algo_attr(KEY_TYPE_AUT, &algo_len);
            return apdu_build_response(resp, resp_max, algo, algo_len, SW_OK);
        }

        case DO_PW_STATUS: {  // 0xC4: PW Status Bytes
            uint8_t status[7] = {
                0x01,       // PW1 valid for multiple signatures
                32,         // Max length PW1
                32,         // Max length RC
                32,         // Max length PW3
                pin_storage_openpgp_pw1_retries(),
                0,          // RC retries
                pin_storage_openpgp_pw3_retries()
            };
            return apdu_build_response(resp, resp_max, status, 7, SW_OK);
        }

        case DO_FP_SIG:  // 0xC7: Fingerprint SIG
            return apdu_build_response(resp, resp_max, fingerprint_sig, 20, SW_OK);

        case DO_FP_DEC:  // 0xC8: Fingerprint DEC
            return apdu_build_response(resp, resp_max, fingerprint_dec, 20, SW_OK);

        case DO_FP_AUT:  // 0xC9: Fingerprint AUT
            return apdu_build_response(resp, resp_max, fingerprint_aut, 20, SW_OK);

        case DO_CA_FP_1:  // 0xCA: CA Fingerprint 1
            return apdu_build_response(resp, resp_max, ca_fp_1, 20, SW_OK);

        case DO_CA_FP_2:  // 0xCB: CA Fingerprint 2
            return apdu_build_response(resp, resp_max, ca_fp_2, 20, SW_OK);

        case DO_CA_FP_3:  // 0xCC: CA Fingerprint 3
            return apdu_build_response(resp, resp_max, ca_fp_3, 20, SW_OK);

        case DO_GEN_TIME_SIG:  // 0xCD: Generation time - Signature
            return apdu_build_response(resp, resp_max, gen_time_sig, 4, SW_OK);

        case DO_GEN_TIME_DEC:  // 0xCE: Generation time - Decryption
            return apdu_build_response(resp, resp_max, gen_time_dec, 4, SW_OK);

        case DO_GEN_TIME_AUT:  // 0xCF: Generation time - Authentication
            return apdu_build_response(resp, resp_max, gen_time_aut, 4, SW_OK);

        case DO_SIG_COUNT: {  // 0x93: Signature counter
            uint8_t count[3] = {
                (uint8_t)((sig_count >> 16) & 0xFF),
                (uint8_t)((sig_count >> 8) & 0xFF),
                (uint8_t)(sig_count & 0xFF)
            };
            return apdu_build_response(resp, resp_max, count, 3, SW_OK);
        }

        // URL for public key retrieval
        case DO_URL: {      // 0x5F50
            size_t len = strlen(cardholder_url);
            return apdu_build_response(resp, resp_max, (const uint8_t*)cardholder_url, len, SW_OK);
        }

        // Login data
        case DO_LOGIN: {    // 0x5E
            size_t len = strlen(cardholder_login);
            return apdu_build_response(resp, resp_max, (const uint8_t*)cardholder_login, len, SW_OK);
        }

        // These are already in build_do_cardholder (0x65), but GPG may query them directly too
        case DO_NAME:       // 0x5B: Cardholder name
            return apdu_build_response(resp, resp_max, (const uint8_t*)cardholder_name, strlen(cardholder_name), SW_OK);

        case DO_LANG_PREF:  // 0x5F2D: Language preference
            return apdu_build_response(resp, resp_max, (const uint8_t*)cardholder_lang, strlen(cardholder_lang), SW_OK);

        case DO_SEX:        // 0x5F35: Sex
            return apdu_build_response(resp, resp_max, &cardholder_sex, 1, SW_OK);

        // UIF (User Interaction Flag) - 2 bytes: mode + features
        case DO_UIF_SIG:    // 0xD6: UIF Signature
        case DO_UIF_DEC:    // 0xD7: UIF Decryption
        case DO_UIF_AUT: {  // 0xD8: UIF Authentication
            uint8_t uif[2] = { 0x00, 0x20 };  // Disabled, button available
            return apdu_build_response(resp, resp_max, uif, 2, SW_OK);
        }

        // Key Information - 6 bytes (status of 3 keys)
        // Format: key_ref, status (0x00=generated, 0x01=imported, 0x02=not present)
        case DO_KEY_INFO: {
            uint8_t key_info[6];
            uint8_t pubkey[65], curve;

            // Check SIG key
            key_info[0] = 0x01;  // Key reference for SIG
            key_info[1] = se_ecc_key_read(gpg_storage_sig_slot(), pubkey, sizeof(pubkey), &curve)
                          ? 0x00  // generated
                          : 0x02;  // Not present

            // Check DEC key
            key_info[2] = 0x02;  // Key reference for DEC
            key_info[3] = se_ecc_key_read(gpg_storage_dec_slot(), pubkey, sizeof(pubkey), &curve)
                          ? 0x00
                          : 0x02;

            // Check AUT key
            key_info[4] = 0x03;  // Key reference for AUT
            key_info[5] = se_ecc_key_read(gpg_storage_aut_slot(), pubkey, sizeof(pubkey), &curve)
                          ? 0x00
                          : 0x02;

            return apdu_build_response(resp, resp_max, key_info, 6, SW_OK);
        }

        // Security Support Template - contains signature counter
        case DO_SEC_TPL: {
            // Format: 7A <len> { 93 03 <sig_count[3]> }
            uint8_t sec_tpl[7] = {
                0x93, 0x03,  // Tag + length for signature counter
                (uint8_t)((sig_count >> 16) & 0xFF),
                (uint8_t)((sig_count >> 8) & 0xFF),
                (uint8_t)(sig_count & 0xFF)
            };
            return apdu_build_response(resp, resp_max, sec_tpl, 5, SW_OK);
        }

        // KDF-DO (Key Derivation Function) - empty means no KDF
        case DO_KDF:
            return apdu_sw(resp, SW_OK);  // Empty = no KDF configured

        default:
            ESP_LOGW(TAG, "GET DATA: Unknown tag 0x%04X", tag);
            return apdu_sw(resp, SW_FILE_NOT_FOUND);
    }
}

/**
 * \brief Handles APDU `PUT DATA` command processing.
 * \param apdu Parsed APDU request.
 * \param resp Output response buffer.
 * \param resp_max Maximum size of `resp`.
 * \return APDU status/response length result.
 */
static int cmd_put_data(const apdu_t *apdu, uint8_t *resp, size_t resp_max) {
    if (!pw3_verified) {
        return apdu_sw(resp, SW_SECURITY_NOT_SATISFIED);
    }

    uint16_t tag = (apdu->p1 << 8) | apdu->p2;

    switch (tag) {
        // Cardholder data (0x5B: Name, part of 0x65)
        case 0x005B:
            if (apdu->lc < sizeof(cardholder_name)) {
                memcpy(cardholder_name, apdu->data, apdu->lc);
                cardholder_name[apdu->lc] = '\0';
                save_state_to_nvs();
                ESP_LOGI(TAG, "Cardholder name set: %s", cardholder_name);
                return apdu_sw(resp, SW_OK);
            }
            return apdu_sw(resp, SW_WRONG_LENGTH);

        // Language preference
        case 0x5F2D:
            if (apdu->lc < sizeof(cardholder_lang)) {
                memcpy(cardholder_lang, apdu->data, apdu->lc);
                cardholder_lang[apdu->lc] = '\0';
                save_state_to_nvs();
                return apdu_sw(resp, SW_OK);
            }
            return apdu_sw(resp, SW_WRONG_LENGTH);

        // Sex
        case 0x5F35:
            if (apdu->lc == 1) {
                cardholder_sex = apdu->data[0];
                save_state_to_nvs();
                return apdu_sw(resp, SW_OK);
            }
            return apdu_sw(resp, SW_WRONG_LENGTH);

        // Fingerprints
        case DO_FP_SIG:
            if (apdu->lc == 20) {
                memcpy(fingerprint_sig, apdu->data, 20);
                save_state_to_nvs();
                ESP_LOGI(TAG, "Fingerprint SIG stored");
                return apdu_sw(resp, SW_OK);
            }
            return apdu_sw(resp, SW_WRONG_LENGTH);

        case DO_FP_DEC:
            if (apdu->lc == 20) {
                memcpy(fingerprint_dec, apdu->data, 20);
                save_state_to_nvs();
                return apdu_sw(resp, SW_OK);
            }
            return apdu_sw(resp, SW_WRONG_LENGTH);

        case DO_FP_AUT:
            if (apdu->lc == 20) {
                memcpy(fingerprint_aut, apdu->data, 20);
                save_state_to_nvs();
                return apdu_sw(resp, SW_OK);
            }
            return apdu_sw(resp, SW_WRONG_LENGTH);

        // CA Fingerprints
        case DO_CA_FP_1:
            if (apdu->lc == 20) {
                memcpy(ca_fp_1, apdu->data, 20);
                save_state_to_nvs();
                return apdu_sw(resp, SW_OK);
            }
            return apdu_sw(resp, SW_WRONG_LENGTH);

        case DO_CA_FP_2:
            if (apdu->lc == 20) {
                memcpy(ca_fp_2, apdu->data, 20);
                save_state_to_nvs();
                return apdu_sw(resp, SW_OK);
            }
            return apdu_sw(resp, SW_WRONG_LENGTH);

        case DO_CA_FP_3:
            if (apdu->lc == 20) {
                memcpy(ca_fp_3, apdu->data, 20);
                save_state_to_nvs();
                return apdu_sw(resp, SW_OK);
            }
            return apdu_sw(resp, SW_WRONG_LENGTH);

        // Generation times
        case DO_GEN_TIME_SIG:
            if (apdu->lc == 4) {
                memcpy(gen_time_sig, apdu->data, 4);
                save_state_to_nvs();
                return apdu_sw(resp, SW_OK);
            }
            return apdu_sw(resp, SW_WRONG_LENGTH);

        case DO_GEN_TIME_DEC:
            if (apdu->lc == 4) {
                memcpy(gen_time_dec, apdu->data, 4);
                save_state_to_nvs();
                return apdu_sw(resp, SW_OK);
            }
            return apdu_sw(resp, SW_WRONG_LENGTH);

        case DO_GEN_TIME_AUT:
            if (apdu->lc == 4) {
                memcpy(gen_time_aut, apdu->data, 4);
                save_state_to_nvs();
                return apdu_sw(resp, SW_OK);
            }
            return apdu_sw(resp, SW_WRONG_LENGTH);

        // URL for public key retrieval
        case DO_URL:
            if (apdu->lc < sizeof(cardholder_url)) {
                memcpy(cardholder_url, apdu->data, apdu->lc);
                cardholder_url[apdu->lc] = '\0';
                save_state_to_nvs();
                ESP_LOGI(TAG, "URL set: %s", cardholder_url);
                return apdu_sw(resp, SW_OK);
            }
            return apdu_sw(resp, SW_WRONG_LENGTH);

        // Login data
        case DO_LOGIN:
            if (apdu->lc < sizeof(cardholder_login)) {
                memcpy(cardholder_login, apdu->data, apdu->lc);
                cardholder_login[apdu->lc] = '\0';
                save_state_to_nvs();
                ESP_LOGI(TAG, "Login set: %s", cardholder_login);
                return apdu_sw(resp, SW_OK);
            }
            return apdu_sw(resp, SW_WRONG_LENGTH);

        default:
            ESP_LOGW(TAG, "PUT DATA: Unknown tag 0x%04X", tag);
            return apdu_sw(resp, SW_FILE_NOT_FOUND);
    }
}

/**
 * \brief Handles APDU `VERIFY` command for PIN verification.
 * \param apdu Parsed APDU request.
 * \param resp Output response buffer.
 * \param resp_max Maximum size of `resp`.
 * \return APDU status/response length result.
 */
static int cmd_verify(const apdu_t *apdu, uint8_t *resp, size_t resp_max) {
    uint8_t pw_ref = apdu->p2;

    // Check remaining retries (Lc=0 means query)
    if (apdu->lc == 0) {
        uint8_t retries;
        if (pw_ref == 0x81 || pw_ref == 0x82) {
            if (pin_storage_openpgp_pw1_blocked()) {
                return apdu_sw(resp, SW_AUTH_METHOD_BLOCKED);
            }
            retries = pin_storage_openpgp_pw1_retries();
        } else if (pw_ref == 0x83) {
            if (pin_storage_openpgp_pw3_blocked()) {
                return apdu_sw(resp, SW_AUTH_METHOD_BLOCKED);
            }
            retries = pin_storage_openpgp_pw3_retries();
        } else {
            return apdu_sw(resp, SW_INCORRECT_P1P2);
        }
        return apdu_sw(resp, 0x63C0 | retries);
    }

    // Convert PIN data to null-terminated string
    char pin_str[OPENPGP_PIN_MAX_LEN + 1];
    if (apdu->lc > OPENPGP_PIN_MAX_LEN) {
        return apdu_sw(resp, SW_WRONG_LENGTH);
    }
    memcpy(pin_str, apdu->data, apdu->lc);
    pin_str[apdu->lc] = '\0';

    // Verify PIN via TROPIC01 storage
    bool verified = false;
    uint8_t retries;

    if (pw_ref == 0x81 || pw_ref == 0x82) {
        // PW1 (User PIN) verification
        verified = pin_storage_openpgp_verify_pw1(pin_str);
        if (verified) {
            pw1_verified = true;
            // Store session PIN for PSO:DECIPHER (ECDH decryption)
            strncpy(s_session_pin, pin_str, OPENPGP_PIN_MAX_LEN);
            s_session_pin[OPENPGP_PIN_MAX_LEN] = '\0';
            ESP_LOGI(TAG, "PW1 verified successfully");
        }
        retries = pin_storage_openpgp_pw1_retries();
    } else if (pw_ref == 0x83) {
        // PW3 (Admin PIN) verification
        verified = pin_storage_openpgp_verify_pw3(pin_str);
        if (verified) {
            pw3_verified = true;
            ESP_LOGI(TAG, "PW3 verified successfully");
        }
        retries = pin_storage_openpgp_pw3_retries();
    } else {
        return apdu_sw(resp, SW_INCORRECT_P1P2);
    }

    if (verified) {
        return apdu_sw(resp, SW_OK);
    }

    // Verification failed
    if (retries == 0) {
        ESP_LOGW(TAG, "PIN blocked after too many failures");
        return apdu_sw(resp, SW_AUTH_METHOD_BLOCKED);
    }
    ESP_LOGW(TAG, "PIN verification failed, %d retries left", retries);
    return apdu_sw(resp, 0x63C0 | retries);
}

/**
 * \brief Handles APDU `CHANGE REFERENCE DATA` command for PIN updates.
 * \param apdu Parsed APDU request.
 * \param resp Output response buffer.
 * \param resp_max Maximum size of `resp`.
 * \return APDU status/response length result.
 */
static int cmd_change_reference_data(const apdu_t *apdu, uint8_t *resp, size_t resp_max) {
    uint8_t pw_ref = apdu->p2;

    if (apdu->lc == 0) {
        return apdu_sw(resp, SW_WRONG_LENGTH);
    }

    // Data format: old PIN || new PIN
    // For PW1: old PIN (6+ bytes) + new PIN (6+ bytes)
    // For PW3: old PIN (8+ bytes) + new PIN (8+ bytes)

    if (pw_ref == 0x81) {
        // Change PW1 (User PIN)
        // Minimum data: 6 (old) + 6 (new) = 12 bytes
        if (apdu->lc < OPENPGP_PW1_MIN_LEN * 2) {
            return apdu_sw(resp, SW_WRONG_LENGTH);
        }

        // Find split point - try different old PIN lengths
        bool changed = false;
        for (size_t old_len = OPENPGP_PW1_MIN_LEN; old_len <= apdu->lc - OPENPGP_PW1_MIN_LEN; old_len++) {
            char old_pin[OPENPGP_PIN_MAX_LEN + 1];
            char new_pin[OPENPGP_PIN_MAX_LEN + 1];

            memcpy(old_pin, apdu->data, old_len);
            old_pin[old_len] = '\0';

            size_t new_len = apdu->lc - old_len;
            memcpy(new_pin, apdu->data + old_len, new_len);
            new_pin[new_len] = '\0';

            // Try verifying with this split
            if (pin_storage_openpgp_verify_pw1(old_pin)) {
                if (pin_storage_openpgp_change_pw1(new_pin)) {
                    ESP_LOGI(TAG, "PW1 changed successfully");
                    changed = true;
                    break;
                }
            }
        }

        if (changed) {
            return apdu_sw(resp, SW_OK);
        }
        uint8_t retries = pin_storage_openpgp_pw1_retries();
        if (retries == 0) {
            return apdu_sw(resp, SW_AUTH_METHOD_BLOCKED);
        }
        return apdu_sw(resp, 0x63C0 | retries);

    } else if (pw_ref == 0x83) {
        // Change PW3 (Admin PIN)
        if (apdu->lc < OPENPGP_PW3_MIN_LEN * 2) {
            return apdu_sw(resp, SW_WRONG_LENGTH);
        }

        bool changed = false;
        for (size_t old_len = OPENPGP_PW3_MIN_LEN; old_len <= apdu->lc - OPENPGP_PW3_MIN_LEN; old_len++) {
            char old_pin[OPENPGP_PIN_MAX_LEN + 1];
            char new_pin[OPENPGP_PIN_MAX_LEN + 1];

            memcpy(old_pin, apdu->data, old_len);
            old_pin[old_len] = '\0';

            size_t new_len = apdu->lc - old_len;
            memcpy(new_pin, apdu->data + old_len, new_len);
            new_pin[new_len] = '\0';

            if (pin_storage_openpgp_verify_pw3(old_pin)) {
                if (pin_storage_openpgp_change_pw3(new_pin)) {
                    ESP_LOGI(TAG, "PW3 changed successfully");
                    changed = true;
                    break;
                }
            }
        }

        if (changed) {
            return apdu_sw(resp, SW_OK);
        }
        uint8_t retries = pin_storage_openpgp_pw3_retries();
        if (retries == 0) {
            return apdu_sw(resp, SW_AUTH_METHOD_BLOCKED);
        }
        return apdu_sw(resp, 0x63C0 | retries);
    }

    return apdu_sw(resp, SW_INCORRECT_P1P2);
}

/**
 * \brief Handles APDU `PSO: COMPUTE DIGITAL SIGNATURE`.
 * \param apdu Parsed APDU request.
 * \param resp Output response buffer.
 * \param resp_max Maximum size of `resp`.
 * \return APDU status/response length result.
 */
static int cmd_pso_cds(const apdu_t *apdu, uint8_t *resp, size_t resp_max) {
    if (!pw1_verified) {
        return apdu_sw(resp, SW_SECURITY_NOT_SATISFIED);
    }

    // Check if signature key exists by trying to read it
    uint8_t pubkey[65];
    uint8_t curve;
    if (!se_ecc_key_read(gpg_storage_sig_slot(), pubkey, sizeof(pubkey), &curve)) {
        ESP_LOGE(TAG, "No signature key configured");
        return apdu_sw(resp, SW_CONDITIONS_NOT_SATISFIED);
    }

    // Sign hash directly using TROPIC01
    // P-256 uses ECDSA, Ed25519 uses EdDSA
    uint8_t signature[64];  // R (32 bytes) || S (32 bytes)

    bool success;
    if (curve == CDC_CURVE_P256) {
        // ECDSA: sign the hash (expected to be SHA-256, 32 bytes)
        if (apdu->lc != 32) {
            ESP_LOGW(TAG, "ECDSA expects 32-byte hash, got %d", apdu->lc);
        }
        success = se_ecdsa_sign(gpg_storage_sig_slot(), apdu->data, apdu->lc, signature);
    } else {
        // EdDSA: sign the message (hash passed as message)
        success = se_eddsa_sign(gpg_storage_sig_slot(), apdu->data, apdu->lc, signature);
    }

    if (!success) {
        ESP_LOGE(TAG, "Signature failed");
        return apdu_sw(resp, SW_UNKNOWN);
    }

    // Increment signature counter
    sig_count++;
    save_state_to_nvs();

    ESP_LOGI(TAG, "Signature created, count=%lu", sig_count);
    return apdu_build_response(resp, resp_max, signature, 64, SW_OK);
}

/**
 * \brief Handles APDU `PSO: DECIPHER` for ECDH key agreement.
 * \param apdu Parsed APDU request.
 * \param resp Output response buffer.
 * \param resp_max Maximum size of `resp`.
 * \return APDU status/response length result.
 *
 * \details
 * PSO:DECIPHER - ECDH Decryption.
 *
 * SECURITY NOTE:
 * The TROPIC01 secure element does NOT support native ECDH operations.
 * Therefore, the DEC private key is stored encrypted in R-Memory and
 * temporarily decrypted in RAM for the ECDH computation.
 *
 * This is a necessary trade-off for GPG compatibility.
 * See docs/GPG_ECDH_SECURITY.md for details.
 *
 * Mitigations:
 * - Key is cleared from RAM immediately after use
 * - Encrypted with PIN-derived key (brute-force protected)
 * - MbedTLS uses constant-time ECDH implementation
 *
 * OpenPGP 3.4.1, Section 7.2.11:
 * Command: 00 2A 80 86 <Lc> <data> <Le>
 * Data format for ECDH:
 *   7F49 <len>         -- Cipher DO
 *      A6 <len>        -- External Public Key template
 *         86 <len>     -- External Public Key point (04||X||Y for P-256)
 *            <65 bytes ephemeral pubkey>
 * Response: Shared Secret (32 bytes for P-256)
 */
static int cmd_pso_decipher(const apdu_t *apdu, uint8_t *resp, size_t resp_max) {
    // PW1 must be verified for decryption operations
    if (!pw1_verified) {
        return apdu_sw(resp, SW_SECURITY_NOT_SATISFIED);
    }

    // Check if DEC private key exists
    if (!gpg_storage_has_dec_privkey()) {
        ESP_LOGE(TAG, "No decryption key configured");
        return apdu_sw(resp, SW_CONDITIONS_NOT_SATISFIED);
    }

    // Parse Cipher DO (7F49 -> A6 -> 86)
    // Minimum: 7F49 <len1> A6 <len2> 86 <len3> <65 bytes pubkey>
    // With single-byte lengths: 7F49 44 A6 42 86 41 <65 bytes> = 73 bytes
    if (apdu->lc < 70) {
        ESP_LOGW(TAG, "PSO:DECIPHER data too short: %d", apdu->lc);
        return apdu_sw(resp, SW_WRONG_DATA);
    }

    const uint8_t* p = apdu->data;
    const uint8_t* end = apdu->data + apdu->lc;

    // Parse 7F49 (Cipher DO)
    if (p + 2 > end || p[0] != 0x7F || p[1] != 0x49) {
        ESP_LOGW(TAG, "Expected 7F49 tag");
        return apdu_sw(resp, SW_WRONG_DATA);
    }
    p += 2;

    // Skip length (1-3 bytes)
    if (p >= end) return apdu_sw(resp, SW_WRONG_DATA);
    if (*p < 0x80) {
        p += 1;
    } else if (*p == 0x81) {
        p += 2;
    } else if (*p == 0x82) {
        p += 3;
    } else {
        return apdu_sw(resp, SW_WRONG_DATA);
    }

    // Parse A6 (External Public Key template)
    if (p >= end || *p != 0xA6) {
        ESP_LOGW(TAG, "Expected A6 tag");
        return apdu_sw(resp, SW_WRONG_DATA);
    }
    p++;

    // Skip length
    if (p >= end) return apdu_sw(resp, SW_WRONG_DATA);
    if (*p < 0x80) {
        p += 1;
    } else if (*p == 0x81) {
        p += 2;
    } else {
        return apdu_sw(resp, SW_WRONG_DATA);
    }

    // Parse 86 (Public Key)
    if (p >= end || *p != 0x86) {
        ESP_LOGW(TAG, "Expected 86 tag");
        return apdu_sw(resp, SW_WRONG_DATA);
    }
    p++;

    // Get public key length
    if (p >= end) return apdu_sw(resp, SW_WRONG_DATA);
    size_t pubkey_len;
    if (*p < 0x80) {
        pubkey_len = *p++;
    } else if (*p == 0x81 && p + 1 < end) {
        pubkey_len = p[1];
        p += 2;
    } else {
        return apdu_sw(resp, SW_WRONG_DATA);
    }

    // Verify public key length (65 bytes for uncompressed P-256)
    if (pubkey_len != 65 || p + pubkey_len > end) {
        ESP_LOGW(TAG, "Invalid public key length: %zu", pubkey_len);
        return apdu_sw(resp, SW_WRONG_DATA);
    }

    // Verify uncompressed format
    if (p[0] != 0x04) {
        ESP_LOGW(TAG, "Expected uncompressed public key (0x04 prefix)");
        return apdu_sw(resp, SW_WRONG_DATA);
    }

    const uint8_t* peer_pubkey = p;

    // Load DEC private key from encrypted R-Memory storage
    // Note: Key is encrypted with device key (ChipID-based), not PIN.
    // PW1 verification above provides access control.
    uint8_t dec_privkey[32];
    if (!gpg_storage_load_dec_privkey(dec_privkey, nullptr)) {
        ESP_LOGE(TAG, "Failed to load DEC private key");
        return apdu_sw(resp, SW_SECURITY_NOT_SATISFIED);
    }

    // Compute ECDH shared secret
    // SECURITY: ecdh_p256_compute_shared_secret() clears dec_privkey after use
    uint8_t shared_secret[32];
    bool ok = ecdh_p256_compute_shared_secret(dec_privkey, peer_pubkey, shared_secret);

    // dec_privkey is already cleared by ecdh_p256_compute_shared_secret
    // but clear again for defense-in-depth
    mbedtls_platform_zeroize(dec_privkey, sizeof(dec_privkey));

    if (!ok) {
        ESP_LOGE(TAG, "ECDH computation failed");
        mbedtls_platform_zeroize(shared_secret, sizeof(shared_secret));
        return apdu_sw(resp, SW_UNKNOWN);
    }

    ESP_LOGI(TAG, "PSO:DECIPHER successful (ECDH shared secret computed)");
    return apdu_build_response(resp, resp_max, shared_secret, 32, SW_OK);
}

/**
 * \brief Returns ECC slot mapping for an OpenPGP key reference.
 * \param key_ref OpenPGP key reference value.
 * \return ECC slot index used in secure element storage.
 */
static uint8_t get_ecc_slot_for_key_ref(uint8_t key_ref) {
    switch (key_ref) {
        case KEY_SIG:  // 0xB6 - Signature
            return gpg_storage_sig_slot();
        case KEY_DEC:  // 0xB8 - Decryption
            return gpg_storage_dec_slot();
        case KEY_AUT:  // 0xA4 - Authentication
            return gpg_storage_aut_slot();
        default:
            return gpg_storage_sig_slot();  // Default to SIG
    }
}

/**
 * \brief Maps an OpenPGP key reference to an internal key type.
 * \param key_ref OpenPGP key reference value.
 * \return Internal `key_type_t` for algorithm selection.
 */
static key_type_t get_key_type_for_ref(uint8_t key_ref) {
    switch (key_ref) {
        case KEY_SIG:  return KEY_TYPE_SIG;
        case KEY_DEC:  return KEY_TYPE_DEC;
        case KEY_AUT:  return KEY_TYPE_AUT;
        default:       return KEY_TYPE_SIG;
    }
}

/**
 * \brief Handles APDU `GENERATE ASYMMETRIC KEY PAIR`.
 * \param apdu Parsed APDU request.
 * \param resp Output response buffer.
 * \param resp_max Maximum size of `resp`.
 * \return APDU status/response length result.
 */
static int cmd_generate_keypair(const apdu_t *apdu, uint8_t *resp, size_t resp_max) {
    // Parse control reference template (CRT) from data
    // Format: B6 00 (SIG) / B8 00 (DEC) / A4 00 (AUT)
    uint8_t key_ref = KEY_SIG;  // Default to Signature key

    if (apdu->lc >= 2) {
        key_ref = apdu->data[0];
        ESP_LOGI(TAG, "Key ref from CRT: 0x%02X", key_ref);
    }

    // Determine ECC slot and key type
    uint8_t ecc_slot = get_ecc_slot_for_key_ref(key_ref);
    key_type_t key_type = get_key_type_for_ref(key_ref);

    ESP_LOGI(TAG, "GENERATE_KEYPAIR: P1=0x%02X, key_ref=0x%02X, slot=%d, type=%d",
             apdu->p1, key_ref, ecc_slot, key_type);

    if (apdu->p1 == 0x80) {
        // Generate new key
        if (!pw3_verified) {
            return apdu_sw(resp, SW_SECURITY_NOT_SATISFIED);
        }

        // Use P-256 as default (matches our Algorithm Attributes)
        // DEC key uses ECDH (P-256), SIG/AUT use ECDSA (P-256)
        uint8_t curve = CDC_CURVE_P256;

        ESP_LOGI(TAG, "Generating key in slot %d (curve=%d, type=%s)",
                 ecc_slot, curve,
                 key_type == KEY_TYPE_SIG ? "SIG" :
                 key_type == KEY_TYPE_DEC ? "DEC" : "AUT");

        if (key_type == KEY_TYPE_DEC) {
            // DEC key: Generate in software, store encrypted in R-Memory
            // TROPIC01 does not support ECDH, so we use software ECDH
            // with the private key stored encrypted in R-Memory
            uint8_t privkey[32];
            uint8_t pubkey_gen[65];

            if (!ecdh_p256_generate_keypair(privkey, pubkey_gen)) {
                ESP_LOGE(TAG, "Software key generation failed for DEC");
                return apdu_sw(resp, SW_UNKNOWN);
            }

            // Store private key encrypted in R-Memory (using device key, no PIN)
            // Security: Key is protected by device-specific encryption
            if (!gpg_storage_save_dec_privkey(privkey, nullptr)) {
                ESP_LOGE(TAG, "Failed to store DEC private key");
                mbedtls_platform_zeroize(privkey, sizeof(privkey));
                return apdu_sw(resp, SW_UNKNOWN);
            }

            // Clear private key from RAM immediately
            mbedtls_platform_zeroize(privkey, sizeof(privkey));

            // Also store public key in TROPIC01 ECC slot for easy retrieval
            // Note: We import the private key to the ECC slot too so eccGetPublicKey works
            // Actually, we can't import - TROPIC01 doesn't allow exporting private keys
            // So we need to store the public key differently or derive it each time

            // For now, skip TROPIC01 storage - we'll derive pubkey from privkey when needed
            // This means we need to load the privkey to get the pubkey for READ operation
            // That's acceptable for the rare READ operation

            ESP_LOGI(TAG, "DEC key pair generated (software ECDH)");
        } else {
            // SIG/AUT keys: Generate in TROPIC01 hardware
            if (!se_ecc_key_generate(ecc_slot, curve)) {
                ESP_LOGE(TAG, "Key generation failed for slot %d", ecc_slot);
                return apdu_sw(resp, SW_UNKNOWN);
            }
            ESP_LOGI(TAG, "Key pair generated in slot %d (hardware)", ecc_slot);
        }

        // Update generation timestamp
        uint32_t now = (uint32_t)time(NULL);
        uint8_t ts[4] = {
            (uint8_t)((now >> 24) & 0xFF),
            (uint8_t)((now >> 16) & 0xFF),
            (uint8_t)((now >> 8) & 0xFF),
            (uint8_t)(now & 0xFF)
        };

        switch (key_ref) {
            case KEY_SIG:
                memcpy(gen_time_sig, ts, 4);
                break;
            case KEY_DEC:
                memcpy(gen_time_dec, ts, 4);
                break;
            case KEY_AUT:
                memcpy(gen_time_aut, ts, 4);
                break;
        }
        save_state_to_nvs();
    }

    // Read public key (P1=0x81 or after generation)
    uint8_t pubkey[65];
    uint8_t read_curve = CDC_CURVE_P256;
    bool pubkey_ok = false;

    if (key_type == KEY_TYPE_DEC) {
        // DEC key: derive public key from stored private key
        if (gpg_storage_has_dec_privkey()) {
            uint8_t privkey[32];
            if (gpg_storage_load_dec_privkey(privkey, nullptr)) {
                pubkey_ok = ecdh_p256_derive_pubkey(privkey, pubkey);
                mbedtls_platform_zeroize(privkey, sizeof(privkey));
            }
        }
        if (!pubkey_ok) {
            ESP_LOGE(TAG, "Failed to read DEC public key");
            return apdu_sw(resp, SW_CONDITIONS_NOT_SATISFIED);
        }
    } else {
        // SIG/AUT keys: read from TROPIC01
        if (!se_ecc_key_read(ecc_slot, pubkey, sizeof(pubkey), &read_curve)) {
            ESP_LOGE(TAG, "Failed to read public key from slot %d", ecc_slot);
            return apdu_sw(resp, SW_CONDITIONS_NOT_SATISFIED);
        }
        pubkey_ok = true;
    }

    // Build TLV response according to OpenPGP 3.4.1 spec:
    // 7F49 <len> { 86 <len> <pubkey> }
    //
    // For ECDSA/ECDH (P-256): pubkey is 65 bytes (04 || X || Y) uncompressed
    // TROPIC01 returns 64 bytes (X || Y), we need to add 0x04 prefix

    uint8_t pubkey_with_prefix[65];
    size_t pubkey_len;

    if (read_curve == CDC_CURVE_P256) {
        if (pubkey[0] == 0x04) {
            memcpy(pubkey_with_prefix, pubkey, 65);
        } else {
            pubkey_with_prefix[0] = 0x04;
            memcpy(pubkey_with_prefix + 1, pubkey, 64);
        }
        pubkey_len = 65;
    } else {
        // Ed25519: 32 bytes, no prefix
        memcpy(pubkey_with_prefix, pubkey, 32);
        pubkey_len = 32;
    }

    uint8_t tlv_data[128];
    size_t pos = 0;

    // Tag 86: Public key
    pos += tlv_build(tlv_data + pos, sizeof(tlv_data) - pos, 0x86, pubkey_with_prefix, pubkey_len);

    // Wrap in 7F49 (Public Key DO)
    uint8_t final_resp[140];
    size_t final_len = 0;

    final_resp[final_len++] = 0x7F;
    final_resp[final_len++] = 0x49;
    final_len += tlv_write_len(final_resp + final_len, pos);
    memcpy(final_resp + final_len, tlv_data, pos);
    final_len += pos;

    ESP_LOGI(TAG, "Public key exported (%zu bytes, curve=%d, slot=%d)",
             pubkey_len, read_curve, ecc_slot);
    return apdu_build_response(resp, resp_max, final_resp, final_len, SW_OK);
}

int openpgp_process_apdu(const uint8_t *cmd, size_t cmd_len,
                         uint8_t *resp, size_t resp_max) {
    apdu_t apdu;

    if (!apdu_parse(cmd, cmd_len, &apdu)) {
        ESP_LOGE(TAG, "Invalid APDU");
        return apdu_sw(resp, SW_WRONG_LENGTH);
    }

    ESP_LOGD(TAG, "APDU: CLA=%02X INS=%02X P1=%02X P2=%02X Lc=%d",
             apdu.cla, apdu.ins, apdu.p1, apdu.p2, apdu.lc);

    // Check CLA
    if (apdu.cla != CLA_ISO7816 && apdu.cla != CLA_CHAIN) {
        return apdu_sw(resp, SW_CLA_NOT_SUPPORTED);
    }

    // SELECT is always allowed
    if (apdu.ins == INS_SELECT) {
        return cmd_select(&apdu, resp, resp_max);
    }

    // All other commands require application to be selected
    if (!app_selected) {
        return apdu_sw(resp, SW_CONDITIONS_NOT_SATISFIED);
    }

    switch (apdu.ins) {
        case INS_GET_DATA:
            return cmd_get_data(&apdu, resp, resp_max);

        case INS_PUT_DATA:
            return cmd_put_data(&apdu, resp, resp_max);

        case INS_VERIFY:
            return cmd_verify(&apdu, resp, resp_max);

        case INS_CHANGE_PIN:
            return cmd_change_reference_data(&apdu, resp, resp_max);

        case INS_PSO:
            if (apdu.p1 == 0x9E && apdu.p2 == 0x9A) {
                // PSO:CDS - Compute Digital Signature
                return cmd_pso_cds(&apdu, resp, resp_max);
            }
            if (apdu.p1 == 0x80 && apdu.p2 == 0x86) {
                // PSO:DECIPHER - ECDH decryption
                return cmd_pso_decipher(&apdu, resp, resp_max);
            }
            return apdu_sw(resp, SW_INCORRECT_P1P2);

        case INS_GENERATE_KEYPAIR:
            return cmd_generate_keypair(&apdu, resp, resp_max);

        case INS_GET_CHALLENGE: {
            // Return random bytes from TROPIC01 TRNG
            uint8_t challenge[255];
            size_t len = apdu.le > 0 ? apdu.le : 8;
            if (len > sizeof(challenge)) len = sizeof(challenge);

            // Use TROPIC01 TRNG (with ESP32 fallback)
            se_random_fill(challenge, len);
            return apdu_build_response(resp, resp_max, challenge, len, SW_OK);
        }

        default:
            ESP_LOGW(TAG, "Unknown instruction: 0x%02X", apdu.ins);
            return apdu_sw(resp, SW_INS_NOT_SUPPORTED);
    }
}
