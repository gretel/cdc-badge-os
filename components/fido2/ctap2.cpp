// CTAP2 Protocol Implementation (FIDO2)
// Based on FIDO2 CTAP2 Specification v2.1

#include "ctap2.h"
#include "cbor_helpers.h"
#include "fido2.h"
#include "fido2_storage.h"
#include "ctaphid.h"
#include "u2f.h"
#include "tropic01.h"
#include "cdc_log.h"
#include "feature_flags.h"
#include <esp_system.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/ecp.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/md.h>
#include <mbedtls/sha256.h>
#include <mbedtls/aes.h>
#include "pin_storage.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

// ============================================================================
// Configuration
// ============================================================================

// Debug flags
#define CTAP2_DEBUG                 0   // Verbose CBOR/response dumps
#define CTAP2_DEBUG_COMMANDS        0   // Command logging

// AAGUID - Authenticator Attestation GUID (unique per device model)
// CDC Badge v1 - 39C3: CDCBAD6E-39C3-0001-BAD6-E00100000001
static const uint8_t AAGUID[16] = {
    0xCD, 0xCB, 0xAD, 0x6E,  // "CDCBAD6E"
    0x39, 0xC3,              // 39C3
    0x00, 0x01,              // Version 1
    0xBA, 0xD6, 0xE0, 0x01,  // "BADGE01"
    0x00, 0x00, 0x00, 0x01   // Device type
};

// Device info strings
static const char *INFO_TRANSPORTS[] = {"usb"};

#define USER_PRESENCE_TIMEOUT_MS    30000   // 30 seconds for user to respond

// ============================================================================
// State
// ============================================================================

static struct {
    bool initialized;
    bool operation_pending;
    bool cancelled;

    // For getNextAssertion
    uint8_t assertion_creds[FIDO2_MAX_CREDENTIALS];
    uint8_t assertion_count;
    uint8_t assertion_index;
    uint8_t assertion_rp_id_hash[32];
    uint8_t assertion_client_data_hash[32];
    bool assertion_up_done;
    bool assertion_include_user;
    bool assertion_appid_used;
} g_ctap2 = {};

// ============================================================================
// ClientPIN State (Protocol 2)
// ============================================================================

#define PIN_PROTOCOL_VERSION    2
#define PIN_TOKEN_SIZE          32
#define PIN_RETRIES_MAX         8
#define PIN_UV_RETRIES_MAX      3

// ClientPIN subcommands
#define PIN_CMD_GET_RETRIES         0x01
#define PIN_CMD_GET_KEY_AGREEMENT   0x02
#define PIN_CMD_SET_PIN             0x03
#define PIN_CMD_CHANGE_PIN          0x04
#define PIN_CMD_GET_PIN_TOKEN       0x05
#define PIN_CMD_GET_PIN_UV_TOKEN    0x09

// pinUvAuthToken permissions (CTAP 2.1)
#define PIN_PERM_MAKE_CREDENTIAL    0x01    // mc
#define PIN_PERM_GET_ASSERTION      0x02    // ga
#define PIN_PERM_CRED_MGMT          0x04    // cm
#define PIN_PERM_BIO_ENROLLMENT     0x08    // be
#define PIN_PERM_LARGE_BLOB_WRITE   0x10    // lbw
#define PIN_PERM_AUTHN_CONFIG       0x20    // acfg

static struct {
    bool initialized;

    // ECDH key pair (generated on init, regenerated on reset)
    mbedtls_ecp_keypair ecdh_key;
    bool ecdh_valid;

    // PIN token (regenerated on each getPinToken)
    uint8_t pin_token[PIN_TOKEN_SIZE];
    bool pin_token_valid;

    // Token permissions (CTAP 2.1) - 0 means all permissions (legacy)
    uint8_t token_permissions;
    uint8_t token_rp_id_hash[32];   // RP restriction (if any)
    bool token_rp_id_set;

    // Retry counters
    uint8_t pin_retries;
    uint8_t uv_retries;
} g_client_pin = {};

// ============================================================================
// Credential Management State (CTAP 2.1)
// ============================================================================

// CredentialManagement subcommands
#define CRED_MGMT_GET_CREDS_METADATA            0x01
#define CRED_MGMT_ENUMERATE_RPS_BEGIN           0x02
#define CRED_MGMT_ENUMERATE_RPS_GET_NEXT        0x03
#define CRED_MGMT_ENUMERATE_CREDS_BEGIN         0x04
#define CRED_MGMT_ENUMERATE_CREDS_GET_NEXT      0x05
#define CRED_MGMT_DELETE_CREDENTIAL             0x06

static struct {
    // RP enumeration state
    uint8_t rp_slots[FIDO2_MAX_CREDENTIALS];    // Slots with unique RPs
    uint8_t rp_count;                            // Number of unique RPs
    uint8_t rp_index;                            // Current enumeration index

    // Credential enumeration state
    uint8_t cred_slots[FIDO2_MAX_CREDENTIALS];  // Slots for current RP
    uint8_t cred_count;                          // Number of credentials for RP
    uint8_t cred_index;                          // Current enumeration index
    uint8_t current_rp_id_hash[32];              // RP being enumerated
} g_cred_mgmt = {};

// ============================================================================
// Helper Functions
// ============================================================================

static void sha256(const uint8_t *data, size_t len, uint8_t *hash) {
    mbedtls_sha256(data, len, hash, 0);
}

static void sha256_str(const char *str, uint8_t *hash) {
    sha256((const uint8_t *)str, strlen(str), hash);
}

static uint8_t build_authenticator_data(
    const uint8_t *rp_id_hash,
    uint8_t flags,
    uint32_t sign_count,
    const uint8_t *attested_cred_data,
    uint16_t attested_cred_len,
    const uint8_t *ext_data,
    uint16_t ext_len,
    uint8_t *out,
    uint16_t *out_len
);

static int ctap2_random(void *ctx, unsigned char *out, size_t len) {
    (void)ctx;
    // Use TROPIC01 TRNG (with ESP32 fallback)
    secure_random_fill(out, len);
    return 0;
}

static bool ctap2_build_attested_cred(const uint8_t *cred_id,
                                      uint16_t cred_id_len,
                                      const uint8_t *pubkey,
                                      uint8_t curve,
                                      uint8_t *out,
                                      uint16_t *out_len) {
    if (!out || !out_len) return false;
    uint16_t off = 0;

    memcpy(out + off, AAGUID, 16);
    off += 16;

    out[off++] = (cred_id_len >> 8) & 0xFF;
    out[off++] = cred_id_len & 0xFF;

    memcpy(out + off, cred_id, cred_id_len);
    off += cred_id_len;

    cbor_writer_t cose_w;
    cbor_writer_init(&cose_w, out + off, 200 - off);
    if (curve == CDC_CURVE_ED25519) {
        cbor_encode_cose_key_ed25519(&cose_w, pubkey);
    } else {
        cbor_encode_cose_key_p256(&cose_w, pubkey, pubkey + 32);
    }
    off += cbor_writer_length(&cose_w);

    *out_len = off;
    return !cbor_writer_error(&cose_w);
}

static bool ctap2_build_auth_data_for_cred(const uint8_t *rp_id_hash,
                                           const uint8_t *attested_cred,
                                           uint16_t attested_len,
                                           uint8_t *auth_data,
                                           uint16_t *auth_data_len) {
    // Flags: UP=0x01, UV=0x04, AT=0x40
    uint8_t flags = 0x01 | 0x40;  // UP=1, AT=1
    bool pin_verified = fido2_is_pin_verified();
    LOG_I("CTAP2", "Building authData: pin_verified=%d", pin_verified);
    if (pin_verified) {
        flags |= 0x04;  // UV=1 when PIN was verified
        LOG_I("CTAP2", "UV flag SET -> flags=0x%02X", flags);
    }
    return build_authenticator_data(rp_id_hash, flags, 0,
                                    attested_cred, attested_len,
                                    NULL, 0,
                                    auth_data, auth_data_len) == CTAP2_OK;
}

static uint16_t ctap2_build_appid_extension(uint8_t *out, size_t out_size) {
    cbor_writer_t w;
    cbor_writer_init(&w, out, out_size);
    cbor_encode_map(&w, 1);
    cbor_encode_text(&w, "appid");
    cbor_encode_bool(&w, true);
    if (cbor_writer_error(&w)) {
        return 0;
    }
    return (uint16_t)cbor_writer_length(&w);
}

static uint8_t ctap2_build_make_credential_response_packed(const uint8_t *auth_data,
                                                            uint16_t auth_data_len,
                                                            const uint8_t *sig,
                                                            uint8_t sig_len,
                                                            const uint8_t *cert,
                                                            uint16_t cert_len,
                                                            uint8_t *response,
                                                            uint16_t *response_len) {
    cbor_writer_t w;
    cbor_writer_init(&w, response + 1, *response_len - 1);

    cbor_encode_map(&w, 3);

    // 0x01: fmt
    cbor_encode_uint(&w, 0x01);
    if (sig_len == 0 && (cert == NULL || cert_len == 0)) {
        // None attestation
        cbor_encode_text(&w, "none");
    } else {
        cbor_encode_text(&w, "packed");
    }

    // 0x02: authData
    cbor_encode_uint(&w, 0x02);
    cbor_encode_bytes(&w, auth_data, auth_data_len);

    // 0x03: attStmt
    cbor_encode_uint(&w, 0x03);
    if (sig_len == 0 && (cert == NULL || cert_len == 0)) {
        // None attestation - empty map
        cbor_encode_map(&w, 0);
    } else if (cert && cert_len > 0) {
        // Basic attestation with certificate
        cbor_encode_map(&w, 3);
        cbor_encode_text(&w, "alg");
        cbor_encode_int(&w, COSE_ALG_ES256);
        cbor_encode_text(&w, "sig");
        cbor_encode_bytes(&w, sig, sig_len);
        cbor_encode_text(&w, "x5c");
        cbor_encode_array(&w, 1);  // Array with single certificate
        cbor_encode_bytes(&w, cert, cert_len);
    } else {
        // Self attestation (no certificate)
        cbor_encode_map(&w, 2);
        cbor_encode_text(&w, "alg");
        cbor_encode_int(&w, COSE_ALG_ES256);
        cbor_encode_text(&w, "sig");
        cbor_encode_bytes(&w, sig, sig_len);
    }

    if (cbor_writer_error(&w)) {
        response[0] = CTAP2_ERR_OTHER;
        *response_len = 1;
        return CTAP2_ERR_OTHER;
    }

    response[0] = CTAP2_OK;
    *response_len = 1 + cbor_writer_length(&w);
    return CTAP2_OK;
}

static bool ctap2_generate_ephemeral_keypair(mbedtls_ecp_keypair *key, uint8_t pubkey[64]) {
    if (!key || !pubkey) return false;
    mbedtls_ecp_keypair_init(key);

    int rc = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, key, ctap2_random, NULL);
    if (rc != 0) {
        mbedtls_ecp_keypair_free(key);
        return false;
    }

#if defined(MBEDTLS_PRIVATE)
#define CTAP2_ECP_GRP(k) (k).MBEDTLS_PRIVATE(grp)
#define CTAP2_ECP_Q(k)   (k).MBEDTLS_PRIVATE(Q)
#else
#define CTAP2_ECP_GRP(k) (k).grp
#define CTAP2_ECP_Q(k)   (k).Q
#endif

    uint8_t buf[65];
    size_t olen = 0;
    rc = mbedtls_ecp_point_write_binary(&CTAP2_ECP_GRP((*key)), &CTAP2_ECP_Q((*key)),
                                        MBEDTLS_ECP_PF_UNCOMPRESSED,
                                        &olen, buf, sizeof(buf));
#undef CTAP2_ECP_GRP
#undef CTAP2_ECP_Q
    if (rc != 0 || olen != sizeof(buf)) {
        mbedtls_ecp_keypair_free(key);
        return false;
    }
    memcpy(pubkey, buf + 1, 64);

    return true;
}

static bool ctap2_sign_with_keypair(mbedtls_ecp_keypair *key,
                                    const uint8_t *msg, size_t msg_len,
                                    uint8_t *sig, size_t sig_size, size_t *sig_len) {
    if (!key || !msg || !sig || !sig_len) return false;
    uint8_t hash[32];
    sha256(msg, msg_len, hash);

    mbedtls_ecdsa_context ecdsa;
    mbedtls_ecdsa_init(&ecdsa);
    int rc = mbedtls_ecdsa_from_keypair(&ecdsa, key);
    if (rc != 0) {
        mbedtls_ecdsa_free(&ecdsa);
        return false;
    }

    rc = mbedtls_ecdsa_write_signature(&ecdsa, MBEDTLS_MD_SHA256,
                                       hash, sizeof(hash),
                                       sig, sig_size, sig_len,
                                       ctap2_random, NULL);
    mbedtls_ecdsa_free(&ecdsa);

    return rc == 0;
}

static uint8_t build_authenticator_data(
    const uint8_t *rp_id_hash,
    uint8_t flags,
    uint32_t sign_count,
    const uint8_t *attested_cred_data,
    uint16_t attested_cred_len,
    const uint8_t *ext_data,
    uint16_t ext_len,
    uint8_t *out,
    uint16_t *out_len
) {
    uint16_t offset = 0;

    // RP ID hash (32 bytes)
    memcpy(out + offset, rp_id_hash, 32);
    offset += 32;

    // Flags (1 byte)
    if (ext_data && ext_len > 0) {
        flags |= 0x80;  // ED
    }
    out[offset++] = flags;

    // Sign count (4 bytes, big endian)
    out[offset++] = (sign_count >> 24) & 0xFF;
    out[offset++] = (sign_count >> 16) & 0xFF;
    out[offset++] = (sign_count >> 8) & 0xFF;
    out[offset++] = sign_count & 0xFF;

    // Attested credential data (if present)
    if (attested_cred_data && attested_cred_len > 0) {
        memcpy(out + offset, attested_cred_data, attested_cred_len);
        offset += attested_cred_len;
    }

    if (ext_data && ext_len > 0) {
        memcpy(out + offset, ext_data, ext_len);
        offset += ext_len;
    }

    *out_len = offset;
    return CTAP2_OK;
}

static bool wait_for_user_presence(const char *rp_id, fido2_action_t action, const char *user_name) {
    LOG_I("CTAP2", "User presence required for %s at %s",
          action == FIDO2_ACTION_REGISTER ? "registration" : "authentication",
          rp_id ? rp_id : "unknown");

    // Send keepalive to signal user presence is needed
    uint32_t cid = ctaphid_get_current_cid();
    ctaphid_send_keepalive(cid, CTAPHID_STATUS_UPNEEDED);

    // Request user presence via callback
    fido2_user_presence_result_t result = fido2_request_user_presence(rp_id, action, user_name);

    switch (result) {
        case FIDO2_UP_APPROVED:
            LOG_I("CTAP2", "User presence approved");
            return true;
        case FIDO2_UP_DENIED:
            LOG_I("CTAP2", "User presence denied");
            return false;
        case FIDO2_UP_TIMEOUT:
            LOG_I("CTAP2", "User presence timeout");
            return false;
        default:
            LOG_W("CTAP2", "User presence unknown state: %d", result);
            return false;
    }
}

// ============================================================================
// getInfo (0x04)
// ============================================================================

uint8_t ctap2_get_info(uint8_t *response, uint16_t *response_len) {
    cbor_writer_t w;
    cbor_writer_init(&w, response + 1, *response_len - 1);

    // Response is a map (10 items)
    cbor_encode_map(&w, 10);

    // 0x01: versions - TEST: add FIDO_2_1
    cbor_encode_uint(&w, 0x01);
    cbor_encode_array(&w, 3);
    cbor_encode_text(&w, "FIDO_2_0");
    cbor_encode_text(&w, "FIDO_2_1");
    cbor_encode_text(&w, "U2F_V2");

    // 0x02: extensions - sorted by length for CBOR canonical form
    cbor_encode_uint(&w, 0x02);
    cbor_encode_array(&w, 3);
    cbor_encode_text(&w, "appid");          // 5 chars
    cbor_encode_text(&w, "credProtect");    // 11 chars - required for resident keys
    cbor_encode_text(&w, "appidExclude");   // 12 chars

    // 0x03: aaguid
    cbor_encode_uint(&w, 0x03);
    cbor_encode_bytes(&w, AAGUID, 16);

    // 0x04: options - SORTED BY KEY LENGTH (CBOR canonical form!)
    cbor_encode_uint(&w, 0x04);
    cbor_encode_map(&w, 7);
    cbor_encode_text(&w, "rk");              // 2 chars
    cbor_encode_bool(&w, true);
    cbor_encode_text(&w, "up");              // 2 chars
    cbor_encode_bool(&w, true);
    cbor_encode_text(&w, "uv");              // 2 chars
    cbor_encode_bool(&w, false);
    cbor_encode_text(&w, "plat");            // 4 chars
    cbor_encode_bool(&w, false);
    cbor_encode_text(&w, "credMgmt");        // 8 chars
    cbor_encode_bool(&w, true);
    cbor_encode_text(&w, "clientPin");       // 9 chars
    cbor_encode_bool(&w, true);
    cbor_encode_text(&w, "pinUvAuthToken");  // 14 chars
    cbor_encode_bool(&w, true);

    // 0x05: maxMsgSize
    cbor_encode_uint(&w, 0x05);
    cbor_encode_uint(&w, 1200);

    // 0x06: pinUvAuthProtocols (must include protocol 2 for FIDO 2.1)
    cbor_encode_uint(&w, 0x06);
    cbor_encode_array(&w, 1);
    cbor_encode_uint(&w, 2);        // PIN/UV Auth Protocol Two

    // 0x07: maxCredentialCountInList
    cbor_encode_uint(&w, 0x07);
    cbor_encode_uint(&w, 8);

    // 0x08: maxCredentialIdLength
    cbor_encode_uint(&w, 0x08);
    cbor_encode_uint(&w, FIDO2_CRED_ID_LEN);

    // 0x09: transports
    cbor_encode_uint(&w, 0x09);
    cbor_encode_array(&w, 1);
    cbor_encode_text(&w, INFO_TRANSPORTS[0]);

    // 0x0A: algorithms - PublicKeyCredentialParameters array
    // Keys sorted by length: "alg" (3) < "type" (4) for CBOR canonical form
    cbor_encode_uint(&w, 0x0A);
    cbor_encode_array(&w, 2);
    // ES256 (P-256/ECDSA) - alg=-7
    cbor_encode_map(&w, 2);
    cbor_encode_text(&w, "alg");
    cbor_encode_int(&w, -7);
    cbor_encode_text(&w, "type");
    cbor_encode_text(&w, "public-key");
    // EdDSA (Ed25519) - alg=-8
    cbor_encode_map(&w, 2);
    cbor_encode_text(&w, "alg");
    cbor_encode_int(&w, -8);
    cbor_encode_text(&w, "type");
    cbor_encode_text(&w, "public-key");

    if (cbor_writer_error(&w)) {
        response[0] = CTAP2_ERR_OTHER;
        *response_len = 1;
        return CTAP2_ERR_OTHER;
    }

    response[0] = CTAP2_OK;
    *response_len = 1 + cbor_writer_length(&w);

#if CTAP2_DEBUG
    LOG_I("CTAP2", "getInfo response len=%u", *response_len);
    for (uint16_t offset = 0; offset < *response_len; offset += 16) {
        char hex[50] = {0};
        int dump_len = ((*response_len - offset) < 16) ? (*response_len - offset) : 16;
        for (int i = 0; i < dump_len; i++) {
            sprintf(hex + (i * 3), "%02X ", response[offset + i]);
        }
        LOG_D("CTAP2", "%03u: %s", offset, hex);
    }
#endif

    return CTAP2_OK;
}

// ============================================================================
// makeCredential (0x01)
// ============================================================================

uint8_t ctap2_make_credential(const uint8_t *params, uint16_t params_len,
                               uint8_t *response, uint16_t *response_len) {
    cbor_reader_t r;
    cbor_reader_init(&r, params, params_len);

    // Parse parameters map
    int map_count = cbor_read_map(&r);
    if (map_count < 0) {
        response[0] = CTAP2_ERR_INVALID_CBOR;
        *response_len = 1;
        return CTAP2_ERR_INVALID_CBOR;
    }

    // Required parameters
    uint8_t client_data_hash[32] = {0};
    char rp_id[FIDO2_RP_ID_MAX_LEN] = {0};
    uint8_t rp_id_hash[32] = {0};
    uint8_t user_id[FIDO2_USER_ID_MAX_LEN] = {0};
    uint8_t user_id_len = 0;
    char user_name[FIDO2_USER_NAME_MAX_LEN] = {0};
    bool rk = false;  // Resident key - use browser's preference
    uint8_t cred_protect = 0;
    int alg = 0;
    bool option_uv = false;
    bool option_up = true;
    char appid_exclude[256] = {0};
    bool has_appid_exclude = false;

    // PIN/UV auth parameters
    uint8_t pin_uv_auth_param[64] = {0};
    size_t pin_uv_auth_param_len = 0;
    uint8_t pin_uv_auth_protocol = 0;

    bool has_client_data = false;
    bool has_rp = false;
    bool has_user = false;
    bool has_alg = false;

    for (int i = 0; i < map_count; i++) {
        uint64_t key;
        if (!cbor_read_uint(&r, &key)) {
            response[0] = CTAP2_ERR_INVALID_CBOR;
            *response_len = 1;
            return CTAP2_ERR_INVALID_CBOR;
        }

        switch (key) {
            case 0x01:  // clientDataHash
                {
                    size_t len;
                    if (!cbor_read_bytes(&r, client_data_hash, 32, &len) || len != 32) {
                        response[0] = CTAP2_ERR_INVALID_CBOR;
                        *response_len = 1;
                        return CTAP2_ERR_INVALID_CBOR;
                    }
                    has_client_data = true;
                }
                break;

            case 0x02:  // rp
                {
                    int rp_count = cbor_read_map(&r);
                    if (rp_count < 0) {
                        response[0] = CTAP2_ERR_INVALID_CBOR;
                        *response_len = 1;
                        return CTAP2_ERR_INVALID_CBOR;
                    }
                    for (int j = 0; j < rp_count; j++) {
                        char rp_key[16];
                        size_t key_len;
                        if (!cbor_read_text(&r, rp_key, sizeof(rp_key), &key_len)) {
                            cbor_skip_item(&r);
                            continue;
                        }
                        if (strcmp(rp_key, "id") == 0) {
                            size_t id_len;
                            cbor_read_text(&r, rp_id, sizeof(rp_id), &id_len);
                            sha256_str(rp_id, rp_id_hash);
                            has_rp = true;
                        } else {
                            cbor_skip_item(&r);
                        }
                    }
                }
                break;

            case 0x03:  // user
                {
                    int user_count = cbor_read_map(&r);
                    if (user_count < 0) {
                        response[0] = CTAP2_ERR_INVALID_CBOR;
                        *response_len = 1;
                        return CTAP2_ERR_INVALID_CBOR;
                    }
                    for (int j = 0; j < user_count; j++) {
                        char user_key[16];
                        size_t key_len;
                        if (!cbor_read_text(&r, user_key, sizeof(user_key), &key_len)) {
                            cbor_skip_item(&r);
                            continue;
                        }
                        if (strcmp(user_key, "id") == 0) {
                            size_t id_len;
                            cbor_read_bytes(&r, user_id, sizeof(user_id), &id_len);
                            user_id_len = id_len;
                            has_user = true;
                        } else if (strcmp(user_key, "name") == 0) {
                            size_t name_len;
                            cbor_read_text(&r, user_name, sizeof(user_name), &name_len);
                        } else {
                            cbor_skip_item(&r);
                        }
                    }
                }
                break;

            case 0x04:  // pubKeyCredParams
                {
                    int params_count = cbor_read_array(&r);
                    if (params_count < 0) {
                        response[0] = CTAP2_ERR_INVALID_CBOR;
                        *response_len = 1;
                        return CTAP2_ERR_INVALID_CBOR;
                    }
                    for (int j = 0; j < params_count; j++) {
                        int param_count = cbor_read_map(&r);
                        int64_t param_alg = 0;
                        for (int k = 0; k < param_count; k++) {
                            char param_key[8];
                            size_t key_len;
                            if (!cbor_read_text(&r, param_key, sizeof(param_key), &key_len)) {
                                cbor_skip_item(&r);
                                continue;
                            }
                            if (strcmp(param_key, "alg") == 0) {
                                cbor_read_int(&r, &param_alg);
                            } else {
                                cbor_skip_item(&r);
                            }
                        }
                        // We support ES256 (P-256/ECDSA) and EdDSA (Ed25519)
                        if (!has_alg && (param_alg == COSE_ALG_ES256 || param_alg == COSE_ALG_EDDSA)) {
                            alg = param_alg;
                            has_alg = true;
                        }
                    }
                }
                break;

            case 0x06:  // extensions
                {
                    int ext_count = cbor_read_map(&r);
                    if (ext_count < 0) {
                        response[0] = CTAP2_ERR_INVALID_CBOR;
                        *response_len = 1;
                        return CTAP2_ERR_INVALID_CBOR;
                    }
                    for (int j = 0; j < ext_count; j++) {
                        char ext_key[16];
                        size_t key_len;
                        if (!cbor_read_text(&r, ext_key, sizeof(ext_key), &key_len)) {
                            cbor_skip_item(&r);
                            continue;
                        }
                        if (strcmp(ext_key, "appidExclude") == 0) {
                            size_t len;
                            if (cbor_read_text(&r, appid_exclude, sizeof(appid_exclude), &len)) {
                                has_appid_exclude = (len > 0);
                            }
                        } else {
                            cbor_skip_item(&r);
                        }
                    }
                }
                break;

            case 0x07:  // options
                {
                    int opt_count = cbor_read_map(&r);
                    for (int j = 0; j < opt_count; j++) {
                        char opt_key[8];
                        size_t key_len;
                        if (!cbor_read_text(&r, opt_key, sizeof(opt_key), &key_len)) {
                            cbor_skip_item(&r);
                            continue;
                        }
                        if (strcmp(opt_key, "rk") == 0) {
                            cbor_read_bool(&r, &rk);
                        } else if (strcmp(opt_key, "uv") == 0) {
                            cbor_read_bool(&r, &option_uv);
                        } else if (strcmp(opt_key, "up") == 0) {
                            cbor_read_bool(&r, &option_up);
                        } else {
                            cbor_skip_item(&r);
                        }
                    }
                }
                break;

            case 0x08:  // pinUvAuthParam
                cbor_read_bytes(&r, pin_uv_auth_param, sizeof(pin_uv_auth_param), &pin_uv_auth_param_len);
                break;

            case 0x09:  // pinUvAuthProtocol
                {
                    uint64_t proto;
                    if (cbor_read_uint(&r, &proto)) {
                        pin_uv_auth_protocol = (uint8_t)proto;
                    }
                }
                break;

            default:
                cbor_skip_item(&r);
                break;
        }
    }

    LOG_I("CTAP2", "makeCredential rp_id=%s rk=%d uv=%d up=%d alg=%d pinProto=%d pinAuthLen=%zu",
          rp_id[0] ? rp_id : "(none)", rk, option_uv, option_up, alg,
          pin_uv_auth_protocol, pin_uv_auth_param_len);

    // Validate required parameters
    if (!has_client_data || !has_rp || !has_user || !has_alg) {
        response[0] = CTAP2_ERR_MISSING_PARAMETER;
        *response_len = 1;
        return CTAP2_ERR_MISSING_PARAMETER;
    }

    if (has_appid_exclude && appid_exclude[0] != '\0') {
        uint8_t appid_hash[32];
        sha256_str(appid_exclude, appid_hash);
        if (fido2_storage_find_by_rp(appid_hash, g_ctap2.assertion_creds, FIDO2_MAX_CREDENTIALS) > 0) {
            response[0] = CTAP2_ERR_CREDENTIAL_EXCLUDED;
            *response_len = 1;
            return CTAP2_ERR_CREDENTIAL_EXCLUDED;
        }
    }

    // Verify pinUvAuthParam if provided (required for UV flag in CTAP 2.1)
    LOG_D("CTAP2", "pinToken valid=%d", g_client_pin.pin_token_valid);
    if (pin_uv_auth_param_len > 0) {
        if (!g_client_pin.pin_token_valid) {
            LOG_W("CTAP2", "makeCredential: pinUvAuthParam provided but no valid pinToken");
            response[0] = CTAP2_ERR_PIN_AUTH_INVALID;
            *response_len = 1;
            return CTAP2_ERR_PIN_AUTH_INVALID;
        }

        // Verify HMAC-SHA-256(pinToken, clientDataHash)
        uint8_t expected_hmac[32];
        mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                        g_client_pin.pin_token, sizeof(g_client_pin.pin_token),
                        client_data_hash, 32,
                        expected_hmac);

        // Protocol 2 uses first 32 bytes of HMAC
        size_t compare_len = (pin_uv_auth_protocol == 2) ? 32 : 16;
        if (pin_uv_auth_param_len < compare_len ||
            memcmp(pin_uv_auth_param, expected_hmac, compare_len) != 0) {
            LOG_W("CTAP2", "makeCredential: pinUvAuthParam verification failed");
            response[0] = CTAP2_ERR_PIN_AUTH_INVALID;
            *response_len = 1;
            return CTAP2_ERR_PIN_AUTH_INVALID;
        }

        LOG_I("CTAP2", "makeCredential: pinUvAuthParam verified - UV=1");
        fido2_set_pin_verified(true);  // Set UV flag for authData
    }

    // Handle browser probing/selection requests - wait for user, return dummy attestation
    // Firefox: "make.me.blink", Chrome: ".dummy" - used to identify/select authenticators
    // FIDO2_ACTION_SELECT = no PIN required, just Y/N confirmation
    if (strcmp(rp_id, "make.me.blink") == 0 || strcmp(rp_id, ".dummy") == 0) {
        LOG_I("CTAP2", "Browser probe request (%s) - waiting for user selection", rp_id);
        // Wait for user to confirm this authenticator (no PIN needed)
        if (!wait_for_user_presence(rp_id, FIDO2_ACTION_SELECT, NULL)) {
            response[0] = CTAP2_ERR_OPERATION_DENIED;
            *response_len = 1;
            return CTAP2_ERR_OPERATION_DENIED;
        }
        // User confirmed - return a valid packed attestation response without storing a credential
        uint8_t dummy_pubkey[64];
        uint8_t dummy_cred_id[FIDO2_CRED_ID_LEN];
        if (ctap2_random(NULL, dummy_cred_id, sizeof(dummy_cred_id)) != 0) {
            response[0] = CTAP2_ERR_OTHER;
            *response_len = 1;
            return CTAP2_ERR_OTHER;
        }

        uint8_t attested_cred[256];
        uint16_t attested_len = 0;
        uint8_t auth_data[256];
        uint16_t auth_data_len = 0;

        uint8_t to_sign[512];
        uint8_t signature[128];
        size_t sig_len = 0;

        mbedtls_ecp_keypair ephemeral_key;
        if (!ctap2_generate_ephemeral_keypair(&ephemeral_key, dummy_pubkey)) {
            response[0] = CTAP2_ERR_OTHER;
            *response_len = 1;
            return CTAP2_ERR_OTHER;
        }

        if (!ctap2_build_attested_cred(dummy_cred_id, FIDO2_CRED_ID_LEN, dummy_pubkey,
                                       CDC_CURVE_P256, attested_cred, &attested_len)) {
            mbedtls_ecp_keypair_free(&ephemeral_key);
            response[0] = CTAP2_ERR_OTHER;
            *response_len = 1;
            return CTAP2_ERR_OTHER;
        }
        if (!ctap2_build_auth_data_for_cred(rp_id_hash, attested_cred, attested_len,
                                            auth_data, &auth_data_len)) {
            mbedtls_ecp_keypair_free(&ephemeral_key);
            response[0] = CTAP2_ERR_OTHER;
            *response_len = 1;
            return CTAP2_ERR_OTHER;
        }
        if (auth_data_len + 32 > sizeof(to_sign)) {
            mbedtls_ecp_keypair_free(&ephemeral_key);
            response[0] = CTAP2_ERR_OTHER;
            *response_len = 1;
            return CTAP2_ERR_OTHER;
        }
        memcpy(to_sign, auth_data, auth_data_len);
        memcpy(to_sign + auth_data_len, client_data_hash, 32);
        uint16_t to_sign_len = auth_data_len + 32;

        if (!ctap2_sign_with_keypair(&ephemeral_key, to_sign, to_sign_len,
                                     signature, sizeof(signature), &sig_len)) {
            mbedtls_ecp_keypair_free(&ephemeral_key);
            response[0] = CTAP2_ERR_OTHER;
            *response_len = 1;
            return CTAP2_ERR_OTHER;
        }
        mbedtls_ecp_keypair_free(&ephemeral_key);
        LOG_I("CTAP2", "User selected this authenticator");
        // Probe response uses self-attestation (no certificate)
        uint8_t status = ctap2_build_make_credential_response_packed(
            auth_data, auth_data_len, signature, (uint8_t)sig_len,
            NULL, 0, response, response_len);
        LOG_I("CTAP2", "Probe makeCredential status=0x%02X resp_len=%u", status, *response_len);
        return status;
    }

    // Check if we support the algorithm and determine curve
    uint8_t curve;
    if (alg == COSE_ALG_ES256) {
        curve = CDC_CURVE_P256;
    } else if (alg == COSE_ALG_EDDSA) {
        curve = CDC_CURVE_ED25519;
    } else {
        response[0] = CTAP2_ERR_UNSUPPORTED_ALGORITHM;
        *response_len = 1;
        return CTAP2_ERR_UNSUPPORTED_ALGORITHM;
    }

    if (option_uv) {
        response[0] = CTAP2_ERR_UNSUPPORTED_OPTION;
        *response_len = 1;
        return CTAP2_ERR_UNSUPPORTED_OPTION;
    }
    if (!option_up) {
        response[0] = CTAP2_ERR_INVALID_OPTION;
        *response_len = 1;
        return CTAP2_ERR_INVALID_OPTION;
    }

    // Request user presence
    if (!wait_for_user_presence(rp_id, FIDO2_ACTION_REGISTER, user_name)) {
        response[0] = CTAP2_ERR_OPERATION_DENIED;
        *response_len = 1;
        return CTAP2_ERR_OPERATION_DENIED;
    }

    LOG_I("CTAP2", "User presence OK, creating credential (curve=%d)...", curve);

    // Create credential
    uint8_t slot;
    uint8_t cred_id[FIDO2_CRED_ID_LEN];
    uint8_t pubkey[64];  // P-256: X||Y, Ed25519: 32 bytes (only first half used)

    LOG_I("CTAP2", "Calling fido2_storage_create_credential...");
    if (!fido2_storage_create_credential(
            rp_id, rp_id_hash, user_id, user_id_len, user_name,
            rk, cred_protect, curve, &slot, cred_id, pubkey)) {
        response[0] = CTAP2_ERR_KEY_STORE_FULL;
        *response_len = 1;
        return CTAP2_ERR_KEY_STORE_FULL;
    }

    uint8_t attested_cred[256];
    uint16_t attested_len = 0;
    uint8_t auth_data[256];
    uint16_t auth_data_len = 0;

    if (!ctap2_build_attested_cred(cred_id, FIDO2_CRED_ID_LEN, pubkey, curve, attested_cred, &attested_len) ||
        !ctap2_build_auth_data_for_cred(rp_id_hash, attested_cred, attested_len,
                                        auth_data, &auth_data_len)) {
        response[0] = CTAP2_ERR_OTHER;
        *response_len = 1;
        return CTAP2_ERR_OTHER;
    }

    uint8_t to_sign[512];
    if (auth_data_len + 32 > sizeof(to_sign)) {
        response[0] = CTAP2_ERR_OTHER;
        *response_len = 1;
        return CTAP2_ERR_OTHER;
    }
    memcpy(to_sign, auth_data, auth_data_len);
    memcpy(to_sign + auth_data_len, client_data_hash, 32);
    uint16_t to_sign_len = auth_data_len + 32;

    // Sign with attestation key for basic attestation
    uint8_t signature[128];
    uint8_t sig_len = 0;
    const uint8_t *att_cert = NULL;
    uint16_t att_cert_len = 0;

    LOG_I("CTAP2", "PIN state: pinToken_valid=%d, is_pin_verified=%d",
          g_client_pin.pin_token_valid, fido2_is_pin_verified());

    // Use "none" attestation until packed attestation is fixed
    bool use_none_attestation = true;

    if (!use_none_attestation && u2f_get_attestation_cert(&att_cert, &att_cert_len) &&
        u2f_attestation_sign(to_sign, to_sign_len, signature, &sig_len)) {
        // Basic attestation with certificate
        LOG_I("CTAP2", "Using basic attestation with certificate");
    } else if (use_none_attestation) {
        // None attestation - simplest format for debugging
        LOG_I("CTAP2", "Using NONE attestation (debug)");
        att_cert = NULL;
        att_cert_len = 0;
        sig_len = 0;  // No signature for "none" attestation
    } else {
        // Fallback to self attestation with credential key
        // CTAP2 packed attestation requires DER-encoded signature!
        LOG_W("CTAP2", "Attestation not available, using self attestation");
        att_cert = NULL;
        att_cert_len = 0;
        if (!fido2_storage_sign_der(slot, to_sign, to_sign_len, signature, &sig_len)) {
            response[0] = CTAP2_ERR_OTHER;
            *response_len = 1;
            return CTAP2_ERR_OTHER;
        }
    }

    uint8_t status = ctap2_build_make_credential_response_packed(
        auth_data, auth_data_len, signature, sig_len,
        att_cert, att_cert_len, response, response_len);
    if (status == CTAP2_OK) {
        LOG_I("CTAP2", "Created credential for %s (slot %d)", rp_id, slot);
    }

#if CTAP2_DEBUG
    LOG_I("CTAP2", "makeCredential status=0x%02X resp_len=%u", status, *response_len);
    for (uint16_t offset = 0; offset < 256 && offset < *response_len; offset += 16) {
        char hex[50] = {0};
        int dump_len = ((*response_len - offset) < 16) ? (*response_len - offset) : 16;
        for (int i = 0; i < dump_len; i++) {
            sprintf(hex + (i * 3), "%02X ", response[offset + i]);
        }
        LOG_D("CTAP2", "%03u: %s", offset, hex);
    }
#endif

    return status;
}

// ============================================================================
// getAssertion (0x02)
// ============================================================================

uint8_t ctap2_get_assertion(const uint8_t *params, uint16_t params_len,
                             uint8_t *response, uint16_t *response_len) {
    cbor_reader_t r;
    cbor_reader_init(&r, params, params_len);

    int map_count = cbor_read_map(&r);
    if (map_count < 0) {
        response[0] = CTAP2_ERR_INVALID_CBOR;
        *response_len = 1;
        return CTAP2_ERR_INVALID_CBOR;
    }

    char rp_id[FIDO2_RP_ID_MAX_LEN] = {0};
    uint8_t rp_id_hash[32] = {0};
    uint8_t client_data_hash[32] = {0};
    bool has_rp = false;
    bool has_client_data = false;
    bool allow_list_present = false;
    uint8_t allow_list_slots[FIDO2_MAX_CREDENTIALS] = {0};
    uint8_t allow_list_count = 0;
    bool option_uv = false;
    bool option_up = true;
    char appid[256] = {0};
    bool has_appid = false;
    uint8_t appid_hash[32] = {0};
    uint8_t pin_uv_auth_param[32] = {0};
    size_t pin_uv_auth_param_len = 0;
    uint8_t pin_uv_auth_protocol = 0;

    for (int i = 0; i < map_count; i++) {
        uint64_t key;
        if (!cbor_read_uint(&r, &key)) {
            response[0] = CTAP2_ERR_INVALID_CBOR;
            *response_len = 1;
            return CTAP2_ERR_INVALID_CBOR;
        }

        switch (key) {
            case 0x01:  // rpId
                {
                    size_t len;
                    cbor_read_text(&r, rp_id, sizeof(rp_id), &len);
                    sha256_str(rp_id, rp_id_hash);
                    has_rp = true;
                }
                break;

            case 0x02:  // clientDataHash
                {
                    size_t len;
                    if (!cbor_read_bytes(&r, client_data_hash, 32, &len) || len != 32) {
                        response[0] = CTAP2_ERR_INVALID_CBOR;
                        *response_len = 1;
                        return CTAP2_ERR_INVALID_CBOR;
                    }
                    has_client_data = true;
                }
                break;

            case 0x03:  // allowList
                {
                    int list_count = cbor_read_array(&r);
                    if (list_count < 0) {
                        response[0] = CTAP2_ERR_INVALID_CBOR;
                        *response_len = 1;
                        return CTAP2_ERR_INVALID_CBOR;
                    }
                    allow_list_present = true;
                    for (int j = 0; j < list_count; j++) {
                        int cred_map = cbor_read_map(&r);
                        if (cred_map < 0) {
                            response[0] = CTAP2_ERR_INVALID_CBOR;
                            *response_len = 1;
                            return CTAP2_ERR_INVALID_CBOR;
                        }
                        uint8_t cred_id[FIDO2_CRED_ID_LEN];
                        size_t cred_id_len = 0;
                        bool have_id = false;

                        for (int k = 0; k < cred_map; k++) {
                            char cred_key[16];
                            size_t key_len;
                            if (!cbor_read_text(&r, cred_key, sizeof(cred_key), &key_len)) {
                                cbor_skip_item(&r);
                                continue;
                            }
                            if (strcmp(cred_key, "id") == 0) {
                                if (!cbor_read_bytes(&r, cred_id, sizeof(cred_id), &cred_id_len)) {
                                    response[0] = CTAP2_ERR_INVALID_CBOR;
                                    *response_len = 1;
                                    return CTAP2_ERR_INVALID_CBOR;
                                }
                                have_id = true;
                            } else {
                                cbor_skip_item(&r);
                            }
                        }

                        if (!have_id || cred_id_len != FIDO2_CRED_ID_LEN) {
                            continue;
                        }

                        int8_t slot = fido2_storage_find_slot_by_cred_id(cred_id, cred_id_len);
                        if (slot < 0) {
                            continue;
                        }

                        // Avoid duplicates
                        bool exists = false;
                        for (uint8_t m = 0; m < allow_list_count; m++) {
                            if (allow_list_slots[m] == (uint8_t)slot) {
                                exists = true;
                                break;
                            }
                        }
                        if (!exists && allow_list_count < FIDO2_MAX_CREDENTIALS) {
                            allow_list_slots[allow_list_count++] = (uint8_t)slot;
                        }
                    }
                }
                break;

            case 0x04:  // extensions
                {
                    int ext_count = cbor_read_map(&r);
                    if (ext_count < 0) {
                        response[0] = CTAP2_ERR_INVALID_CBOR;
                        *response_len = 1;
                        return CTAP2_ERR_INVALID_CBOR;
                    }
                    for (int j = 0; j < ext_count; j++) {
                        char ext_key[16];
                        size_t key_len;
                        if (!cbor_read_text(&r, ext_key, sizeof(ext_key), &key_len)) {
                            cbor_skip_item(&r);
                            continue;
                        }
                        if (strcmp(ext_key, "appid") == 0) {
                            size_t len;
                            if (cbor_read_text(&r, appid, sizeof(appid), &len)) {
                                has_appid = (len > 0);
                                if (has_appid) {
                                    sha256_str(appid, appid_hash);
                                }
                            }
                        } else {
                            cbor_skip_item(&r);
                        }
                    }
                }
                break;

            case 0x05:  // options
                {
                    int opt_count = cbor_read_map(&r);
                    if (opt_count < 0) {
                        response[0] = CTAP2_ERR_INVALID_CBOR;
                        *response_len = 1;
                        return CTAP2_ERR_INVALID_CBOR;
                    }
                    for (int j = 0; j < opt_count; j++) {
                        char opt_key[8];
                        size_t key_len;
                        if (!cbor_read_text(&r, opt_key, sizeof(opt_key), &key_len)) {
                            cbor_skip_item(&r);
                            continue;
                        }
                        if (strcmp(opt_key, "uv") == 0) {
                            cbor_read_bool(&r, &option_uv);
                        } else if (strcmp(opt_key, "up") == 0) {
                            cbor_read_bool(&r, &option_up);
                        } else {
                            cbor_skip_item(&r);
                        }
                    }
                }
                break;

            case 0x06:  // pinUvAuthParam
                cbor_read_bytes(&r, pin_uv_auth_param, sizeof(pin_uv_auth_param), &pin_uv_auth_param_len);
                break;

            case 0x07:  // pinUvAuthProtocol
                {
                    uint64_t proto;
                    if (cbor_read_uint(&r, &proto)) {
                        pin_uv_auth_protocol = (uint8_t)proto;
                    }
                }
                break;

            default:
                cbor_skip_item(&r);
                break;
        }
    }

    LOG_I("CTAP2", "getAssertion rp_id=%s allowList=%d count=%u uv=%d up=%d appid=%s",
          rp_id[0] ? rp_id : "(none)", allow_list_present ? 1 : 0,
          allow_list_count, option_uv, option_up, has_appid ? appid : "(none)");

    if (!has_rp || !has_client_data) {
        response[0] = CTAP2_ERR_MISSING_PARAMETER;
        *response_len = 1;
        return CTAP2_ERR_MISSING_PARAMETER;
    }

    if (option_uv) {
        response[0] = CTAP2_ERR_UNSUPPORTED_OPTION;
        *response_len = 1;
        return CTAP2_ERR_UNSUPPORTED_OPTION;
    }
    // Note: up=false is allowed for CTAP 2.1 "silent discovery" requests
    // The client checks if credentials exist without user interaction

    // Verify pinUvAuthParam if provided (required for UV flag)
    bool uv_verified = false;
    if (pin_uv_auth_param_len > 0) {
        if (!g_client_pin.pin_token_valid) {
            LOG_W("CTAP2", "pinUvAuthParam provided but no valid pinToken");
            response[0] = CTAP2_ERR_PIN_AUTH_INVALID;
            *response_len = 1;
            return CTAP2_ERR_PIN_AUTH_INVALID;
        }

        // Compute HMAC-SHA256(pinToken, clientDataHash)
        uint8_t expected_hmac[32];
        mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                        g_client_pin.pin_token, PIN_TOKEN_SIZE,
                        client_data_hash, 32,
                        expected_hmac);

        // Protocol 2 uses first 32 bytes of HMAC
        size_t compare_len = (pin_uv_auth_protocol == 2) ? 32 : 16;
        if (pin_uv_auth_param_len < compare_len) {
            LOG_W("CTAP2", "pinUvAuthParam too short: %zu < %zu", pin_uv_auth_param_len, compare_len);
            response[0] = CTAP2_ERR_PIN_AUTH_INVALID;
            *response_len = 1;
            return CTAP2_ERR_PIN_AUTH_INVALID;
        }

#if DEBUG_MODE
        LOG_D("CTAP2", "pinUvAuthParam received (%zu bytes):", pin_uv_auth_param_len);
        LOG_D("CTAP2", "  %02X%02X%02X%02X %02X%02X%02X%02X...",
              pin_uv_auth_param[0], pin_uv_auth_param[1], pin_uv_auth_param[2], pin_uv_auth_param[3],
              pin_uv_auth_param[4], pin_uv_auth_param[5], pin_uv_auth_param[6], pin_uv_auth_param[7]);
        LOG_D("CTAP2", "Expected HMAC (first %zu bytes):", compare_len);
        LOG_D("CTAP2", "  %02X%02X%02X%02X %02X%02X%02X%02X...",
              expected_hmac[0], expected_hmac[1], expected_hmac[2], expected_hmac[3],
              expected_hmac[4], expected_hmac[5], expected_hmac[6], expected_hmac[7]);
#endif

        if (memcmp(pin_uv_auth_param, expected_hmac, compare_len) != 0) {
            LOG_W("CTAP2", "pinUvAuthParam verification failed");
            response[0] = CTAP2_ERR_PIN_AUTH_INVALID;
            *response_len = 1;
            return CTAP2_ERR_PIN_AUTH_INVALID;
        }

        LOG_I("CTAP2", "pinUvAuthParam verified - UV=1");
        uv_verified = true;
        fido2_set_pin_verified(true);  // Skip device PIN entry
    }

    // Find matching credentials (prefer appid hash if present and matches exist)
    uint8_t *hash_in_use = rp_id_hash;
    bool appid_used = false;
    uint8_t temp_slots[FIDO2_MAX_CREDENTIALS] = {0};
    uint8_t temp_count = 0;

    if (allow_list_present && allow_list_count > 0) {
        uint8_t filtered = 0;
        for (uint8_t i = 0; i < allow_list_count; i++) {
            fido2_credential_info_t info;
            if (fido2_storage_get_credential(allow_list_slots[i], &info) &&
                memcmp(info.rp_id_hash, rp_id_hash, 32) == 0) {
                g_ctap2.assertion_creds[filtered++] = allow_list_slots[i];
            }
        }
        g_ctap2.assertion_count = filtered;
        g_ctap2.assertion_include_user = false;
        LOG_I("CTAP2", "getAssertion using allowList, matches=%u", g_ctap2.assertion_count);

        if (has_appid) {
            uint8_t filtered_appid = 0;
            for (uint8_t i = 0; i < allow_list_count; i++) {
                fido2_credential_info_t info;
                if (fido2_storage_get_credential(allow_list_slots[i], &info) &&
                    memcmp(info.rp_id_hash, appid_hash, 32) == 0) {
                    temp_slots[filtered_appid++] = allow_list_slots[i];
                }
            }
            if (filtered_appid > 0) {
                memcpy(g_ctap2.assertion_creds, temp_slots, filtered_appid);
                g_ctap2.assertion_count = filtered_appid;
                appid_used = true;
                hash_in_use = appid_hash;
            }
        }
    } else {
        // No allowList - search ALL credentials for this RP (not just resident)
        // This provides better UX when sites don't send allowList
        g_ctap2.assertion_count = fido2_storage_find_by_rp(
            rp_id_hash, g_ctap2.assertion_creds, FIDO2_MAX_CREDENTIALS);
        g_ctap2.assertion_include_user = true;
        LOG_I("CTAP2", "getAssertion using all RP creds, matches=%u", g_ctap2.assertion_count);

        if (has_appid) {
            temp_count = fido2_storage_find_by_rp(
                appid_hash, temp_slots, FIDO2_MAX_CREDENTIALS);
            if (temp_count > 0) {
                memcpy(g_ctap2.assertion_creds, temp_slots, temp_count);
                g_ctap2.assertion_count = temp_count;
                appid_used = true;
                hash_in_use = appid_hash;
            }
        }
    }

    if (g_ctap2.assertion_count == 0) {
        response[0] = CTAP2_ERR_NO_CREDENTIALS;
        *response_len = 1;
        return CTAP2_ERR_NO_CREDENTIALS;
    }

    // Request user presence (only if up=true)
    if (option_up) {
        if (!wait_for_user_presence(rp_id, FIDO2_ACTION_AUTHENTICATE, NULL)) {
            response[0] = CTAP2_ERR_OPERATION_DENIED;
            *response_len = 1;
            return CTAP2_ERR_OPERATION_DENIED;
        }
    }

    // Save state for getNextAssertion
    memcpy(g_ctap2.assertion_rp_id_hash, hash_in_use, 32);
    memcpy(g_ctap2.assertion_client_data_hash, client_data_hash, 32);
    g_ctap2.assertion_index = 0;
    g_ctap2.assertion_up_done = option_up;
    g_ctap2.assertion_appid_used = appid_used;

    // Use first credential
    uint8_t slot = g_ctap2.assertion_creds[0];
    fido2_credential_info_t cred;
    if (!fido2_storage_get_credential(slot, &cred)) {
        response[0] = CTAP2_ERR_OTHER;
        *response_len = 1;
        return CTAP2_ERR_OTHER;
    }

    // Increment sign count
    uint32_t sign_count = fido2_storage_increment_sign_count(slot);

    // Build authenticator data
    uint8_t auth_data[128];
    uint16_t auth_data_len;
    uint8_t flags = option_up ? 0x01 : 0x00;  // UP=1 only if user presence was requested
    if (uv_verified) {
        flags |= 0x04;  // UV=1
    }

    uint8_t ext_data[32];
    uint16_t ext_len = 0;
    if (appid_used) {
        ext_len = ctap2_build_appid_extension(ext_data, sizeof(ext_data));
    }

    build_authenticator_data(hash_in_use, flags, sign_count, NULL, 0,
                              ext_data, ext_len,
                              auth_data, &auth_data_len);

    // Sign: authData || clientDataHash (TROPIC01 hashes internally, no pre-hash!)
    uint8_t to_sign[96];  // Max 64 bytes auth_data + 32 bytes clientDataHash
    if (auth_data_len > sizeof(to_sign) - 32) {
        LOG_E("CTAP2", "auth_data too large: %u", auth_data_len);
        response[0] = CTAP2_ERR_OTHER;
        *response_len = 1;
        return CTAP2_ERR_OTHER;
    }
    memcpy(to_sign, auth_data, auth_data_len);
    memcpy(to_sign + auth_data_len, client_data_hash, 32);
    uint16_t to_sign_len = auth_data_len + 32;

    uint8_t signature[128];
    uint8_t sig_len;
    if (!fido2_storage_sign_raw(slot, to_sign, to_sign_len, signature, &sig_len)) {
        response[0] = CTAP2_ERR_OTHER;
        *response_len = 1;
        return CTAP2_ERR_OTHER;
    }

    // Get credential ID
    uint8_t cred_id[FIDO2_CRED_ID_LEN];
    if (!fido2_storage_get_cred_id(slot, cred_id)) {
        LOG_E("CTAP2", "Failed to get credential ID for slot %d", slot);
        response[0] = CTAP2_ERR_OTHER;
        *response_len = 1;
        return CTAP2_ERR_OTHER;
    }

    // Build response
    cbor_writer_t w;
    cbor_writer_init(&w, response + 1, *response_len - 1);

    int resp_fields = 3;
    if (g_ctap2.assertion_include_user) resp_fields++;
    if (g_ctap2.assertion_count > 1) resp_fields++;

    cbor_encode_map(&w, resp_fields);

    // 0x01: credential
    cbor_encode_uint(&w, 0x01);
    cbor_encode_map(&w, 2);
    // Canonical order: "id" (len 2) before "type" (len 4)
    cbor_encode_text(&w, "id");
    cbor_encode_bytes(&w, cred_id, FIDO2_CRED_ID_LEN);
    cbor_encode_text(&w, "type");
    cbor_encode_text(&w, "public-key");

    // 0x02: authData
    cbor_encode_uint(&w, 0x02);
    cbor_encode_bytes(&w, auth_data, auth_data_len);

    // 0x03: signature
    cbor_encode_uint(&w, 0x03);
    cbor_encode_bytes(&w, signature, sig_len);

    // 0x04: user (only for discoverable credentials)
    if (g_ctap2.assertion_include_user) {
        cbor_encode_uint(&w, 0x04);
        int user_fields = 1;
        if (cred.user_name[0] != '\0') user_fields++;
        cbor_encode_map(&w, user_fields);

        cbor_encode_text(&w, "id");
        cbor_encode_bytes(&w, cred.user_id, cred.user_id_len);

        if (cred.user_name[0] != '\0') {
            cbor_encode_text(&w, "name");
            cbor_encode_text(&w, cred.user_name);
        }
    }

    // 0x05: numberOfCredentials (if multiple)
    if (g_ctap2.assertion_count > 1) {
        cbor_encode_uint(&w, 0x05);
        cbor_encode_uint(&w, g_ctap2.assertion_count);
    }

    if (cbor_writer_error(&w)) {
        response[0] = CTAP2_ERR_OTHER;
        *response_len = 1;
        return CTAP2_ERR_OTHER;
    }

    fido2_increment_auth_counter();

    response[0] = CTAP2_OK;
    *response_len = 1 + cbor_writer_length(&w);

    LOG_I("CTAP2", "Assertion for %s (slot %d)", rp_id, slot);
    return CTAP2_OK;
}

// ============================================================================
// getNextAssertion (0x08)
// ============================================================================

uint8_t ctap2_get_next_assertion(uint8_t *response, uint16_t *response_len) {
    if (g_ctap2.assertion_count == 0 || !g_ctap2.assertion_up_done) {
        response[0] = CTAP2_ERR_NOT_ALLOWED;
        *response_len = 1;
        return CTAP2_ERR_NOT_ALLOWED;
    }

    g_ctap2.assertion_index++;
    if (g_ctap2.assertion_index >= g_ctap2.assertion_count) {
        response[0] = CTAP2_ERR_NOT_ALLOWED;
        *response_len = 1;
        return CTAP2_ERR_NOT_ALLOWED;
    }

    // Similar to getAssertion but without user presence check
    uint8_t slot = g_ctap2.assertion_creds[g_ctap2.assertion_index];
    fido2_credential_info_t cred;
    if (!fido2_storage_get_credential(slot, &cred)) {
        response[0] = CTAP2_ERR_OTHER;
        *response_len = 1;
        return CTAP2_ERR_OTHER;
    }

    uint32_t sign_count = fido2_storage_increment_sign_count(slot);

    uint8_t auth_data[128];
    uint16_t auth_data_len;
    uint8_t ext_data[32];
    uint16_t ext_len = 0;
    if (g_ctap2.assertion_appid_used) {
        ext_len = ctap2_build_appid_extension(ext_data, sizeof(ext_data));
    }
    build_authenticator_data(g_ctap2.assertion_rp_id_hash, 0x01, sign_count,
                              NULL, 0, ext_data, ext_len, auth_data, &auth_data_len);

    // Sign: authData || clientDataHash (TROPIC01 hashes internally)
    uint8_t to_sign[96];
    if (auth_data_len > sizeof(to_sign) - 32) {
        LOG_E("CTAP2", "auth_data too large: %u", auth_data_len);
        response[0] = CTAP2_ERR_OTHER;
        *response_len = 1;
        return CTAP2_ERR_OTHER;
    }
    memcpy(to_sign, auth_data, auth_data_len);
    memcpy(to_sign + auth_data_len, g_ctap2.assertion_client_data_hash, 32);
    uint16_t to_sign_len = auth_data_len + 32;

    uint8_t signature[128];
    uint8_t sig_len;
    if (!fido2_storage_sign_raw(slot, to_sign, to_sign_len, signature, &sig_len)) {
        response[0] = CTAP2_ERR_OTHER;
        *response_len = 1;
        return CTAP2_ERR_OTHER;
    }

    // Get credential ID
    uint8_t cred_id[FIDO2_CRED_ID_LEN];
    if (!fido2_storage_get_cred_id(slot, cred_id)) {
        LOG_E("CTAP2", "Failed to get credential ID for slot %d", slot);
        response[0] = CTAP2_ERR_OTHER;
        *response_len = 1;
        return CTAP2_ERR_OTHER;
    }

    cbor_writer_t w;
    cbor_writer_init(&w, response + 1, *response_len - 1);

    cbor_encode_map(&w, 3);

    // 0x01: credential (type + id)
    cbor_encode_uint(&w, 0x01);
    cbor_encode_map(&w, 2);
    // Canonical order: "id" (len 2) before "type" (len 4)
    cbor_encode_text(&w, "id");
    cbor_encode_bytes(&w, cred_id, FIDO2_CRED_ID_LEN);
    cbor_encode_text(&w, "type");
    cbor_encode_text(&w, "public-key");

    // 0x02: authData
    cbor_encode_uint(&w, 0x02);
    cbor_encode_bytes(&w, auth_data, auth_data_len);

    // 0x03: signature
    cbor_encode_uint(&w, 0x03);
    cbor_encode_bytes(&w, signature, sig_len);

    response[0] = CTAP2_OK;
    *response_len = 1 + cbor_writer_length(&w);
    return CTAP2_OK;
}

// ============================================================================
// clientPIN (0x06) - Full Implementation
// ============================================================================

// Initialize ClientPIN ECDH key pair
static bool client_pin_init_ecdh(void) {
    if (g_client_pin.ecdh_valid) return true;

    mbedtls_ecp_keypair_init(&g_client_pin.ecdh_key);

    int ret = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1,
                                   &g_client_pin.ecdh_key,
                                   ctap2_random, NULL);
    if (ret != 0) {
        LOG_E("PIN", "ECDH key generation failed: %d", ret);
        return false;
    }

    g_client_pin.ecdh_valid = true;
    g_client_pin.pin_retries = PIN_RETRIES_MAX;
    g_client_pin.uv_retries = PIN_UV_RETRIES_MAX;
    LOG_I("PIN", "ECDH key pair generated");
    return true;
}

// Compute shared secret from platform's public key
// Protocol 1: sharedSecret = SHA256(Z)
// Protocol 2: sharedSecret = HKDF-SHA256(salt=0, IKM=Z, info="CTAP2 AES key")
static bool client_pin_compute_shared_secret(const uint8_t *platform_key_x,
                                              const uint8_t *platform_key_y,
                                              uint8_t pin_protocol,
                                              uint8_t *shared_secret) {
    if (!g_client_pin.ecdh_valid) return false;

    mbedtls_ecp_point platform_point;
    mbedtls_mpi shared_x;

    mbedtls_ecp_point_init(&platform_point);
    mbedtls_mpi_init(&shared_x);

    int ret = 0;

    // Load platform public key
    ret = mbedtls_mpi_read_binary(&platform_point.MBEDTLS_PRIVATE(X), platform_key_x, 32);
    if (ret != 0) goto cleanup;

    ret = mbedtls_mpi_read_binary(&platform_point.MBEDTLS_PRIVATE(Y), platform_key_y, 32);
    if (ret != 0) goto cleanup;

    ret = mbedtls_mpi_lset(&platform_point.MBEDTLS_PRIVATE(Z), 1);
    if (ret != 0) goto cleanup;

    // Compute ECDH: shared_x = (platformPubKey * authenticatorPrivKey).x
    ret = mbedtls_ecdh_compute_shared(&g_client_pin.ecdh_key.MBEDTLS_PRIVATE(grp),
                                       &shared_x,
                                       &platform_point,
                                       &g_client_pin.ecdh_key.MBEDTLS_PRIVATE(d),
                                       ctap2_random, NULL);
    if (ret != 0) {
        LOG_E("PIN", "ECDH compute failed: %d", ret);
        goto cleanup;
    }

    // Extract x coordinate as bytes (Z = ECDH shared secret)
    uint8_t ecdh_z[32];
    ret = mbedtls_mpi_write_binary(&shared_x, ecdh_z, 32);
    if (ret != 0) goto cleanup;

#if DEBUG_MODE
    // Full debug output for manual verification
    LOG_I("PIN", "=== ECDH DEBUG (full 32-byte values) ===");
    LOG_I("PIN", "Z (ECDH x-coord):");
    LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          ecdh_z[0], ecdh_z[1], ecdh_z[2], ecdh_z[3], ecdh_z[4], ecdh_z[5], ecdh_z[6], ecdh_z[7],
          ecdh_z[8], ecdh_z[9], ecdh_z[10], ecdh_z[11], ecdh_z[12], ecdh_z[13], ecdh_z[14], ecdh_z[15]);
    LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          ecdh_z[16], ecdh_z[17], ecdh_z[18], ecdh_z[19], ecdh_z[20], ecdh_z[21], ecdh_z[22], ecdh_z[23],
          ecdh_z[24], ecdh_z[25], ecdh_z[26], ecdh_z[27], ecdh_z[28], ecdh_z[29], ecdh_z[30], ecdh_z[31]);
#endif // DEBUG_MODE

    if (pin_protocol == 1) {
        // Protocol 1: sharedSecret = SHA256(Z)
        LOG_D("PIN", "Using Protocol 1: SHA256(Z)");
        mbedtls_sha256(ecdh_z, 32, shared_secret, 0);  // 0 = SHA256 (not SHA224)
    } else {
        // Protocol 2: Use HKDF-SHA256 to derive AES key
        // AES_key = HKDF-SHA256(salt=32zeros, IKM=Z, L=32, info="CTAP2 AES key")
        LOG_D("PIN", "Using Protocol 2: HKDF(Z)");
        const char *info = "CTAP2 AES key";
        uint8_t prk[32];
        uint8_t zero_salt[32] = {0};

        // HKDF Extract: PRK = HMAC-SHA256(salt, IKM=Z)
        mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                        zero_salt, 32, ecdh_z, 32, prk);

        // HKDF Expand: OKM = HMAC-SHA256(PRK, info || 0x01)
        uint8_t expand_input[32];
        size_t info_len = strlen(info);
        memcpy(expand_input, info, info_len);
        expand_input[info_len] = 0x01;

#if DEBUG_MODE
        LOG_I("PIN", "HKDF PRK (HMAC-SHA256(salt=0, IKM=Z)):");
        LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
              prk[0], prk[1], prk[2], prk[3], prk[4], prk[5], prk[6], prk[7],
              prk[8], prk[9], prk[10], prk[11], prk[12], prk[13], prk[14], prk[15]);
        LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
              prk[16], prk[17], prk[18], prk[19], prk[20], prk[21], prk[22], prk[23],
              prk[24], prk[25], prk[26], prk[27], prk[28], prk[29], prk[30], prk[31]);
        LOG_I("PIN", "HKDF info: '%s' || 0x01 (len=%zu)", info, info_len + 1);
#endif

        mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                        prk, 32, expand_input, info_len + 1, shared_secret);
    }

#if DEBUG_MODE
    LOG_I("PIN", "AES key (shared secret):");
    LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          shared_secret[0], shared_secret[1], shared_secret[2], shared_secret[3],
          shared_secret[4], shared_secret[5], shared_secret[6], shared_secret[7],
          shared_secret[8], shared_secret[9], shared_secret[10], shared_secret[11],
          shared_secret[12], shared_secret[13], shared_secret[14], shared_secret[15]);
    LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          shared_secret[16], shared_secret[17], shared_secret[18], shared_secret[19],
          shared_secret[20], shared_secret[21], shared_secret[22], shared_secret[23],
          shared_secret[24], shared_secret[25], shared_secret[26], shared_secret[27],
          shared_secret[28], shared_secret[29], shared_secret[30], shared_secret[31]);
    LOG_I("PIN", "=== END ECDH DEBUG ===");
#endif

cleanup:
    mbedtls_ecp_point_free(&platform_point);
    mbedtls_mpi_free(&shared_x);
    return ret == 0;
}

// AES-256-CBC decrypt with custom IV
static bool aes_256_cbc_decrypt_iv(const uint8_t *key, const uint8_t *iv,
                                    const uint8_t *input, size_t len, uint8_t *output) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    uint8_t iv_copy[16];
    memcpy(iv_copy, iv, 16);  // mbedtls modifies IV during decrypt

    int ret = mbedtls_aes_setkey_dec(&aes, key, 256);
    if (ret != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }

    ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, len, iv_copy, input, output);
    mbedtls_aes_free(&aes);
    return ret == 0;
}

// AES-256-CBC decrypt (IV = 0) - for Protocol 1
static bool aes_256_cbc_decrypt(const uint8_t *key, const uint8_t *input,
                                 size_t len, uint8_t *output) {
    uint8_t iv[16] = {0};
    return aes_256_cbc_decrypt_iv(key, iv, input, len, output);
}

// AES-256-CBC encrypt (IV = 0) - for Protocol 1
static bool aes_256_cbc_encrypt(const uint8_t *key, const uint8_t *input,
                                 size_t len, uint8_t *output) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    uint8_t iv[16] = {0};  // IV is all zeros for PIN protocol 1

    int ret = mbedtls_aes_setkey_enc(&aes, key, 256);
    if (ret != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }

    ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, len, iv, input, output);
    mbedtls_aes_free(&aes);
    return ret == 0;
}

// AES-256-CBC encrypt for Protocol 2: returns IV || ciphertext
// output must have space for len + 16 bytes (IV prefix)
static bool aes_256_cbc_encrypt_p2(const uint8_t *key, const uint8_t *input,
                                    size_t len, uint8_t *output) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    // Generate random IV using TROPIC01 TRNG
    uint8_t iv[16];
    secure_random_fill(iv, 16);

    // Copy IV to output first
    memcpy(output, iv, 16);

    int ret = mbedtls_aes_setkey_enc(&aes, key, 256);
    if (ret != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }

    // Encrypt after the IV prefix
    ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, len, iv, input, output + 16);
    mbedtls_aes_free(&aes);
    return ret == 0;
}

// getPINRetries (subCommand 0x01)
static uint8_t client_pin_get_retries(uint8_t *response, uint16_t *response_len) {
    cbor_writer_t w;
    cbor_writer_init(&w, response + 1, *response_len - 1);

    cbor_encode_map(&w, 2);

    // 0x03: pinRetries
    cbor_encode_uint(&w, 0x03);
    cbor_encode_uint(&w, g_client_pin.pin_retries);

    // 0x04: powerCycleState (optional, not used)
    cbor_encode_uint(&w, 0x05);
    cbor_encode_uint(&w, g_client_pin.uv_retries);

    response[0] = CTAP2_OK;
    *response_len = 1 + cbor_writer_length(&w);
    return CTAP2_OK;
}

// getKeyAgreement (subCommand 0x02)
static uint8_t client_pin_get_key_agreement(uint8_t *response, uint16_t *response_len) {
    if (!client_pin_init_ecdh()) {
        response[0] = CTAP2_ERR_OTHER;
        *response_len = 1;
        return CTAP2_ERR_OTHER;
    }

    // Extract public key coordinates
    uint8_t pub_x[32], pub_y[32];
    mbedtls_mpi_write_binary(&g_client_pin.ecdh_key.MBEDTLS_PRIVATE(Q).MBEDTLS_PRIVATE(X), pub_x, 32);
    mbedtls_mpi_write_binary(&g_client_pin.ecdh_key.MBEDTLS_PRIVATE(Q).MBEDTLS_PRIVATE(Y), pub_y, 32);

#if DEBUG_MODE
    LOG_I("PIN", "=== Our ECDH public key ===");
    LOG_I("PIN", "X: %02X%02X%02X%02X %02X%02X%02X%02X...",
          pub_x[0], pub_x[1], pub_x[2], pub_x[3], pub_x[4], pub_x[5], pub_x[6], pub_x[7]);
    LOG_I("PIN", "Y: %02X%02X%02X%02X %02X%02X%02X%02X...",
          pub_y[0], pub_y[1], pub_y[2], pub_y[3], pub_y[4], pub_y[5], pub_y[6], pub_y[7]);
#endif

    cbor_writer_t w;
    cbor_writer_init(&w, response + 1, *response_len - 1);

    cbor_encode_map(&w, 1);

    // 0x01: keyAgreement (COSE_Key)
    cbor_encode_uint(&w, 0x01);
    cbor_encode_map(&w, 5);

    // kty: EC2 (2)
    cbor_encode_uint(&w, 1);
    cbor_encode_uint(&w, 2);

    // alg: ECDH-ES+HKDF-256 (-25)
    cbor_encode_uint(&w, 3);
    cbor_encode_int(&w, -25);

    // crv: P-256 (1)
    cbor_encode_int(&w, -1);
    cbor_encode_uint(&w, 1);

    // x coordinate
    cbor_encode_int(&w, -2);
    cbor_encode_bytes(&w, pub_x, 32);

    // y coordinate
    cbor_encode_int(&w, -3);
    cbor_encode_bytes(&w, pub_y, 32);

    response[0] = CTAP2_OK;
    *response_len = 1 + cbor_writer_length(&w);
    LOG_I("PIN", "Sent key agreement");
    return CTAP2_OK;
}

// getPinToken (subCommand 0x05)
static uint8_t client_pin_get_pin_token(const uint8_t *params, uint16_t params_len,
                                         uint8_t *response, uint16_t *response_len) {
    // Check if PIN is blocked
    if (g_client_pin.pin_retries == 0) {
        response[0] = CTAP2_ERR_PIN_BLOCKED;
        *response_len = 1;
        return CTAP2_ERR_PIN_BLOCKED;
    }

    // Check if FIDO2 PIN hash is available
    if (!pin_storage_fido2_available()) {
        LOG_E("PIN", "FIDO2 hash not available - user must reset PIN");
        response[0] = CTAP2_ERR_PIN_NOT_SET;
        *response_len = 1;
        return CTAP2_ERR_PIN_NOT_SET;
    }

    // Parse parameters
    cbor_reader_t r;
    cbor_reader_init(&r, params, params_len);

    uint8_t platform_key_x[32] = {0};
    uint8_t platform_key_y[32] = {0};
    uint8_t pin_hash_enc[64] = {0};  // Can be 16 or 32 bytes (with padding)
    size_t pin_hash_enc_len = 0;
    uint8_t pin_protocol = 2;  // Default to Protocol 2
    bool has_key = false, has_pin = false;

    int map_size = cbor_read_map(&r);
    if (map_size < 0) {
        response[0] = CTAP2_ERR_INVALID_CBOR;
        *response_len = 1;
        return CTAP2_ERR_INVALID_CBOR;
    }

    for (int i = 0; i < map_size; i++) {
        // Read key - could be positive or negative, so use cbor_read_item
        cbor_item_t item;
        if (!cbor_read_item(&r, &item)) {
            LOG_E("PIN", "Failed to read map key %d", i);
            break;
        }

        int64_t key;
        if (item.type == CBOR_UNSIGNED) {
            key = (int64_t)item.value;
        } else if (item.type == CBOR_NEGATIVE) {
            key = -1 - (int64_t)item.value;
        } else {
            LOG_E("PIN", "Unexpected key type: %d", item.type);
            cbor_skip_item(&r);
            continue;
        }

        LOG_D("PIN", "Parsing key: %lld", key);

        switch (key) {
            case 0x01: {  // pinUvAuthProtocol
                uint64_t proto;
                if (cbor_read_uint(&r, &proto)) {
                    pin_protocol = (uint8_t)proto;
                    LOG_I("PIN", "Client requested protocol: %d", pin_protocol);
                }
                break;
            }
            case 0x03: {  // keyAgreement (COSE_Key)
                int cose_size = cbor_read_map(&r);
                LOG_D("PIN", "COSE_Key map size: %d", cose_size);
                if (cose_size < 0) break;
                for (int j = 0; j < cose_size; j++) {
                    // COSE keys can be positive (1, 3) or negative (-1, -2, -3)
                    cbor_item_t cose_item;
                    if (!cbor_read_item(&r, &cose_item)) {
                        LOG_E("PIN", "Failed to read COSE key %d", j);
                        break;
                    }

                    int64_t cose_key;
                    if (cose_item.type == CBOR_UNSIGNED) {
                        cose_key = (int64_t)cose_item.value;
                    } else if (cose_item.type == CBOR_NEGATIVE) {
                        cose_key = -1 - (int64_t)cose_item.value;
                    } else {
                        LOG_E("PIN", "Unexpected COSE key type: %d", cose_item.type);
                        cbor_skip_item(&r);
                        continue;
                    }

                    LOG_D("PIN", "COSE key: %lld", cose_key);

                    if (cose_key == -2) {  // x coordinate
                        size_t x_len;
                        if (cbor_read_bytes(&r, platform_key_x, 32, &x_len) && x_len == 32) {
                            has_key = true;
                            LOG_D("PIN", "Got x coordinate");
                        }
                    } else if (cose_key == -3) {  // y coordinate
                        size_t y_len;
                        cbor_read_bytes(&r, platform_key_y, 32, &y_len);
                        LOG_D("PIN", "Got y coordinate");
                    } else {
                        cbor_skip_item(&r);
                    }
                }
                break;
            }
            case 0x06: {  // pinHashEnc (16 or 32 bytes with PKCS7 padding)
                if (cbor_read_bytes(&r, pin_hash_enc, sizeof(pin_hash_enc), &pin_hash_enc_len)) {
                    LOG_D("PIN", "pinHashEnc read OK, len=%zu", pin_hash_enc_len);
                    if (pin_hash_enc_len == 16 || pin_hash_enc_len == 32 || pin_hash_enc_len == 64) {
                        has_pin = true;
                        LOG_D("PIN", "Got pinHashEnc (%zu bytes)", pin_hash_enc_len);
                    } else {
                        LOG_E("PIN", "pinHashEnc unexpected size: %zu", pin_hash_enc_len);
                    }
                } else {
                    LOG_E("PIN", "Failed to read pinHashEnc bytes");
                }
                break;
            }
            default:
                cbor_skip_item(&r);
                break;
        }
    }

    if (!has_key || !has_pin) {
        LOG_E("PIN", "Missing keyAgreement or pinHashEnc");
        response[0] = CTAP2_ERR_MISSING_PARAMETER;
        *response_len = 1;
        return CTAP2_ERR_MISSING_PARAMETER;
    }

#if DEBUG_MODE
    // Log received pinHashEnc for debugging
    LOG_I("PIN", "Received pinHashEnc (%zu bytes):", pin_hash_enc_len);
    for (size_t i = 0; i < pin_hash_enc_len; i += 16) {
        size_t row_len = (pin_hash_enc_len - i < 16) ? (pin_hash_enc_len - i) : 16;
        char hex[64];
        char *p = hex;
        for (size_t j = 0; j < row_len; j++) {
            p += sprintf(p, "%02X ", pin_hash_enc[i + j]);
        }
        LOG_I("PIN", "  %s", hex);
    }
#endif

    // Compute shared secret
    uint8_t shared_secret[32];
    if (!client_pin_compute_shared_secret(platform_key_x, platform_key_y, pin_protocol, shared_secret)) {
        response[0] = CTAP2_ERR_OTHER;
        *response_len = 1;
        return CTAP2_ERR_OTHER;
    }

#if DEBUG_MODE
    // Full platform key for verification
    LOG_I("PIN", "Platform (Chrome) public key X:");
    LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          platform_key_x[0], platform_key_x[1], platform_key_x[2], platform_key_x[3],
          platform_key_x[4], platform_key_x[5], platform_key_x[6], platform_key_x[7],
          platform_key_x[8], platform_key_x[9], platform_key_x[10], platform_key_x[11],
          platform_key_x[12], platform_key_x[13], platform_key_x[14], platform_key_x[15]);
    LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          platform_key_x[16], platform_key_x[17], platform_key_x[18], platform_key_x[19],
          platform_key_x[20], platform_key_x[21], platform_key_x[22], platform_key_x[23],
          platform_key_x[24], platform_key_x[25], platform_key_x[26], platform_key_x[27],
          platform_key_x[28], platform_key_x[29], platform_key_x[30], platform_key_x[31]);
    LOG_I("PIN", "Platform (Chrome) public key Y:");
    LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          platform_key_y[0], platform_key_y[1], platform_key_y[2], platform_key_y[3],
          platform_key_y[4], platform_key_y[5], platform_key_y[6], platform_key_y[7],
          platform_key_y[8], platform_key_y[9], platform_key_y[10], platform_key_y[11],
          platform_key_y[12], platform_key_y[13], platform_key_y[14], platform_key_y[15]);
    LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          platform_key_y[16], platform_key_y[17], platform_key_y[18], platform_key_y[19],
          platform_key_y[20], platform_key_y[21], platform_key_y[22], platform_key_y[23],
          platform_key_y[24], platform_key_y[25], platform_key_y[26], platform_key_y[27],
          platform_key_y[28], platform_key_y[29], platform_key_y[30], platform_key_y[31]);
#endif

    // Decrypt pinHashEnc
    // Protocol 1: 16 bytes ciphertext with IV=0
    // Protocol 2: 32 bytes = IV (16) || ciphertext (16)
    uint8_t decrypted_pin_hash[16];

    if (pin_protocol == 2 && pin_hash_enc_len == 32) {
        // Protocol 2: first 16 bytes are IV, next 16 are ciphertext
        const uint8_t *iv = pin_hash_enc;
        const uint8_t *ciphertext = pin_hash_enc + 16;

        LOG_D("PIN", "Protocol 2 IV: %02X%02X%02X%02X %02X%02X%02X%02X...",
              iv[0], iv[1], iv[2], iv[3], iv[4], iv[5], iv[6], iv[7]);
        LOG_D("PIN", "Ciphertext:    %02X%02X%02X%02X %02X%02X%02X%02X...",
              ciphertext[0], ciphertext[1], ciphertext[2], ciphertext[3],
              ciphertext[4], ciphertext[5], ciphertext[6], ciphertext[7]);

        if (!aes_256_cbc_decrypt_iv(shared_secret, iv, ciphertext, 16, decrypted_pin_hash)) {
            LOG_E("PIN", "PIN decryption failed");
            response[0] = CTAP2_ERR_OTHER;
            *response_len = 1;
            return CTAP2_ERR_OTHER;
        }
    } else {
        // Protocol 1: IV is all zeros
        uint8_t decrypted[64];
        if (!aes_256_cbc_decrypt(shared_secret, pin_hash_enc, pin_hash_enc_len, decrypted)) {
            LOG_E("PIN", "PIN decryption failed");
            response[0] = CTAP2_ERR_OTHER;
            *response_len = 1;
            return CTAP2_ERR_OTHER;
        }
        memcpy(decrypted_pin_hash, decrypted, 16);
    }

#if DEBUG_MODE
    LOG_D("PIN", "Decrypted PIN hash: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          decrypted_pin_hash[0], decrypted_pin_hash[1], decrypted_pin_hash[2], decrypted_pin_hash[3],
          decrypted_pin_hash[4], decrypted_pin_hash[5], decrypted_pin_hash[6], decrypted_pin_hash[7],
          decrypted_pin_hash[8], decrypted_pin_hash[9], decrypted_pin_hash[10], decrypted_pin_hash[11],
          decrypted_pin_hash[12], decrypted_pin_hash[13], decrypted_pin_hash[14], decrypted_pin_hash[15]);

    // Get stored hash for comparison
    uint8_t stored_hash[16];
    pin_storage_get_fido2_hash(stored_hash);
    LOG_D("PIN", "Stored FIDO2 hash:  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          stored_hash[0], stored_hash[1], stored_hash[2], stored_hash[3],
          stored_hash[4], stored_hash[5], stored_hash[6], stored_hash[7],
          stored_hash[8], stored_hash[9], stored_hash[10], stored_hash[11],
          stored_hash[12], stored_hash[13], stored_hash[14], stored_hash[15]);

    // Debug: compute expected hash for "0000"
    uint8_t test_full[32];
    sha256((const uint8_t*)"0000", 4, test_full);
    LOG_D("PIN", "Expected for 0000: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          test_full[0], test_full[1], test_full[2], test_full[3],
          test_full[4], test_full[5], test_full[6], test_full[7],
          test_full[8], test_full[9], test_full[10], test_full[11],
          test_full[12], test_full[13], test_full[14], test_full[15]);
#endif

    // Verify PIN hash
    if (!pin_storage_verify_fido2_hash(decrypted_pin_hash)) {
        g_client_pin.pin_retries--;
        LOG_W("PIN", "Invalid PIN, retries left: %d", g_client_pin.pin_retries);

        if (g_client_pin.pin_retries == 0) {
            response[0] = CTAP2_ERR_PIN_BLOCKED;
        } else {
            response[0] = CTAP2_ERR_PIN_INVALID;
        }
        *response_len = 1;
        return response[0];
    }

    // PIN correct - reset retries and generate pinToken
    g_client_pin.pin_retries = PIN_RETRIES_MAX;
    secure_random_fill(g_client_pin.pin_token, PIN_TOKEN_SIZE);
    g_client_pin.pin_token_valid = true;

    // Encrypt pinToken with shared secret
    // Protocol 1: IV=0, returns ciphertext only (32 bytes)
    // Protocol 2: returns IV || ciphertext (16 + 32 = 48 bytes)
    uint8_t encrypted_token[PIN_TOKEN_SIZE + 16];  // Extra space for IV in Protocol 2
    size_t encrypted_len;

    if (pin_protocol == 2) {
        if (!aes_256_cbc_encrypt_p2(shared_secret, g_client_pin.pin_token, PIN_TOKEN_SIZE, encrypted_token)) {
            response[0] = CTAP2_ERR_OTHER;
            *response_len = 1;
            return CTAP2_ERR_OTHER;
        }
        encrypted_len = PIN_TOKEN_SIZE + 16;  // IV + ciphertext
        LOG_D("PIN", "Encrypted pinToken (Protocol 2, %zu bytes with IV)", encrypted_len);
    } else {
        if (!aes_256_cbc_encrypt(shared_secret, g_client_pin.pin_token, PIN_TOKEN_SIZE, encrypted_token)) {
            response[0] = CTAP2_ERR_OTHER;
            *response_len = 1;
            return CTAP2_ERR_OTHER;
        }
        encrypted_len = PIN_TOKEN_SIZE;
        LOG_D("PIN", "Encrypted pinToken (Protocol 1, %zu bytes)", encrypted_len);
    }

    // Build response
    cbor_writer_t w;
    cbor_writer_init(&w, response + 1, *response_len - 1);

    cbor_encode_map(&w, 1);

    // 0x02: pinUvAuthToken (encrypted)
    cbor_encode_uint(&w, 0x02);
    cbor_encode_bytes(&w, encrypted_token, encrypted_len);

    response[0] = CTAP2_OK;
    *response_len = 1 + cbor_writer_length(&w);

    // Legacy token (0x05) has all permissions
    g_client_pin.token_permissions = 0xFF;
    g_client_pin.token_rp_id_set = false;

    LOG_I("PIN", "PIN verified, token issued (legacy, all permissions)");
    return CTAP2_OK;
}

// getPinUvAuthTokenUsingPinWithPermissions (0x09) - CTAP 2.1
static uint8_t client_pin_get_pin_uv_auth_token(const uint8_t *params, uint16_t params_len,
                                                 uint8_t *response, uint16_t *response_len) {
    // Check if PIN is blocked
    if (g_client_pin.pin_retries == 0) {
        response[0] = CTAP2_ERR_PIN_BLOCKED;
        *response_len = 1;
        return CTAP2_ERR_PIN_BLOCKED;
    }

    // Check if FIDO2 PIN hash is available
    if (!pin_storage_fido2_available()) {
        LOG_E("PIN", "FIDO2 hash not available - user must reset PIN");
        response[0] = CTAP2_ERR_PIN_NOT_SET;
        *response_len = 1;
        return CTAP2_ERR_PIN_NOT_SET;
    }

    // Parse parameters
    cbor_reader_t r;
    cbor_reader_init(&r, params, params_len);

    uint8_t platform_key_x[32] = {0};
    uint8_t platform_key_y[32] = {0};
    uint8_t pin_hash_enc[64] = {0};
    size_t pin_hash_enc_len = 0;
    uint8_t pin_protocol = 2;
    uint8_t permissions = 0;
    char rp_id[64] = {0};
    bool has_key = false, has_pin = false, has_permissions = false;

    int map_size = cbor_read_map(&r);
    if (map_size < 0) {
        response[0] = CTAP2_ERR_INVALID_CBOR;
        *response_len = 1;
        return CTAP2_ERR_INVALID_CBOR;
    }

    for (int i = 0; i < map_size; i++) {
        cbor_item_t item;
        if (!cbor_read_item(&r, &item)) break;

        int64_t key;
        if (item.type == CBOR_UNSIGNED) {
            key = (int64_t)item.value;
        } else if (item.type == CBOR_NEGATIVE) {
            key = -1 - (int64_t)item.value;
        } else {
            cbor_skip_item(&r);
            continue;
        }

        switch (key) {
            case 0x01: {  // pinUvAuthProtocol
                uint64_t proto;
                if (cbor_read_uint(&r, &proto)) {
                    pin_protocol = (uint8_t)proto;
                }
                break;
            }
            case 0x03: {  // keyAgreement (COSE_Key)
                int cose_size = cbor_read_map(&r);
                if (cose_size < 0) break;
                for (int j = 0; j < cose_size; j++) {
                    cbor_item_t cose_item;
                    if (!cbor_read_item(&r, &cose_item)) break;

                    int64_t cose_key;
                    if (cose_item.type == CBOR_UNSIGNED) {
                        cose_key = (int64_t)cose_item.value;
                    } else if (cose_item.type == CBOR_NEGATIVE) {
                        cose_key = -1 - (int64_t)cose_item.value;
                    } else {
                        cbor_skip_item(&r);
                        continue;
                    }

                    if (cose_key == -2) {  // x coordinate
                        size_t x_len;
                        if (cbor_read_bytes(&r, platform_key_x, 32, &x_len) && x_len == 32) {
                            has_key = true;
                        }
                    } else if (cose_key == -3) {  // y coordinate
                        size_t y_len;
                        cbor_read_bytes(&r, platform_key_y, 32, &y_len);
                    } else {
                        cbor_skip_item(&r);
                    }
                }
                break;
            }
            case 0x06: {  // pinHashEnc
                if (cbor_read_bytes(&r, pin_hash_enc, sizeof(pin_hash_enc), &pin_hash_enc_len)) {
                    if (pin_hash_enc_len == 16 || pin_hash_enc_len == 32 || pin_hash_enc_len == 64) {
                        has_pin = true;
                    }
                }
                break;
            }
            case 0x09: {  // permissions
                uint64_t perm;
                if (cbor_read_uint(&r, &perm)) {
                    permissions = (uint8_t)perm;
                    has_permissions = true;
                    LOG_I("PIN", "Requested permissions: 0x%02X", permissions);
                }
                break;
            }
            case 0x0A: {  // rpId
                size_t rp_len;
                if (cbor_read_text(&r, rp_id, sizeof(rp_id) - 1, &rp_len)) {
                    LOG_I("PIN", "Requested rpId: %s", rp_id);
                }
                break;
            }
            default:
                cbor_skip_item(&r);
                break;
        }
    }

    if (!has_key || !has_pin) {
        LOG_E("PIN", "Missing keyAgreement or pinHashEnc");
        response[0] = CTAP2_ERR_MISSING_PARAMETER;
        *response_len = 1;
        return CTAP2_ERR_MISSING_PARAMETER;
    }

    if (!has_permissions) {
        LOG_E("PIN", "Missing permissions parameter");
        response[0] = CTAP2_ERR_MISSING_PARAMETER;
        *response_len = 1;
        return CTAP2_ERR_MISSING_PARAMETER;
    }

    // Compute shared secret
    uint8_t shared_secret[32];
    if (!client_pin_compute_shared_secret(platform_key_x, platform_key_y, pin_protocol, shared_secret)) {
        response[0] = CTAP2_ERR_OTHER;
        *response_len = 1;
        return CTAP2_ERR_OTHER;
    }

    // Decrypt pinHashEnc
    uint8_t decrypted_pin_hash[16];
    if (pin_protocol == 2 && pin_hash_enc_len == 32) {
        const uint8_t *iv = pin_hash_enc;
        const uint8_t *ciphertext = pin_hash_enc + 16;
        if (!aes_256_cbc_decrypt_iv(shared_secret, iv, ciphertext, 16, decrypted_pin_hash)) {
            LOG_E("PIN", "PIN decryption failed");
            response[0] = CTAP2_ERR_OTHER;
            *response_len = 1;
            return CTAP2_ERR_OTHER;
        }
    } else {
        uint8_t decrypted[64];
        if (!aes_256_cbc_decrypt(shared_secret, pin_hash_enc, pin_hash_enc_len, decrypted)) {
            LOG_E("PIN", "PIN decryption failed");
            response[0] = CTAP2_ERR_OTHER;
            *response_len = 1;
            return CTAP2_ERR_OTHER;
        }
        memcpy(decrypted_pin_hash, decrypted, 16);
    }

    // Verify PIN hash
    if (!pin_storage_verify_fido2_hash(decrypted_pin_hash)) {
        g_client_pin.pin_retries--;
        LOG_W("PIN", "Invalid PIN, retries left: %d", g_client_pin.pin_retries);
        response[0] = (g_client_pin.pin_retries == 0) ? CTAP2_ERR_PIN_BLOCKED : CTAP2_ERR_PIN_INVALID;
        *response_len = 1;
        return response[0];
    }

    // PIN correct - reset retries and generate pinToken
    g_client_pin.pin_retries = PIN_RETRIES_MAX;
    secure_random_fill(g_client_pin.pin_token, PIN_TOKEN_SIZE);
    g_client_pin.pin_token_valid = true;

    // Store permissions
    g_client_pin.token_permissions = permissions;
    if (rp_id[0]) {
        sha256_str(rp_id, g_client_pin.token_rp_id_hash);
        g_client_pin.token_rp_id_set = true;
    } else {
        g_client_pin.token_rp_id_set = false;
    }

    // Encrypt pinToken
    uint8_t encrypted_token[PIN_TOKEN_SIZE + 16];
    size_t encrypted_len;

    if (pin_protocol == 2) {
        if (!aes_256_cbc_encrypt_p2(shared_secret, g_client_pin.pin_token, PIN_TOKEN_SIZE, encrypted_token)) {
            response[0] = CTAP2_ERR_OTHER;
            *response_len = 1;
            return CTAP2_ERR_OTHER;
        }
        encrypted_len = PIN_TOKEN_SIZE + 16;
    } else {
        if (!aes_256_cbc_encrypt(shared_secret, g_client_pin.pin_token, PIN_TOKEN_SIZE, encrypted_token)) {
            response[0] = CTAP2_ERR_OTHER;
            *response_len = 1;
            return CTAP2_ERR_OTHER;
        }
        encrypted_len = PIN_TOKEN_SIZE;
    }

    // Build response
    cbor_writer_t w;
    cbor_writer_init(&w, response + 1, *response_len - 1);

    cbor_encode_map(&w, 1);
    cbor_encode_uint(&w, 0x02);  // pinUvAuthToken
    cbor_encode_bytes(&w, encrypted_token, encrypted_len);

    response[0] = CTAP2_OK;
    *response_len = 1 + cbor_writer_length(&w);
    LOG_I("PIN", "PIN verified, token issued with permissions=0x%02X", permissions);
    return CTAP2_OK;
}

uint8_t ctap2_client_pin(const uint8_t *params, uint16_t params_len,
                          uint8_t *response, uint16_t *response_len) {
    // Initialize if needed
    if (!g_client_pin.initialized) {
        g_client_pin.pin_retries = PIN_RETRIES_MAX;
        g_client_pin.uv_retries = PIN_UV_RETRIES_MAX;
        g_client_pin.initialized = true;
    }

    // Parse subCommand
    cbor_reader_t r;
    cbor_reader_init(&r, params, params_len);

    int map_size = cbor_read_map(&r);
    if (map_size < 0) {
        response[0] = CTAP2_ERR_INVALID_CBOR;
        *response_len = 1;
        return CTAP2_ERR_INVALID_CBOR;
    }

    uint64_t pin_protocol = 0;
    uint64_t sub_command = 0;

    for (int i = 0; i < map_size; i++) {
        uint64_t key;
        if (!cbor_read_uint(&r, &key)) break;

        if (key == 0x01) {  // pinUvAuthProtocol
            cbor_read_uint(&r, &pin_protocol);
        } else if (key == 0x02) {  // subCommand
            cbor_read_uint(&r, &sub_command);
        } else {
            cbor_skip_item(&r);
        }
    }

    LOG_I("PIN", "ClientPIN: protocol=%llu, subCommand=0x%02llx", pin_protocol, sub_command);

    // We only support protocol 2
    if (pin_protocol != 0 && pin_protocol != PIN_PROTOCOL_VERSION) {
        response[0] = CTAP1_ERR_INVALID_PARAMETER;
        *response_len = 1;
        return CTAP1_ERR_INVALID_PARAMETER;
    }

    switch (sub_command) {
        case PIN_CMD_GET_RETRIES:
            return client_pin_get_retries(response, response_len);

        case PIN_CMD_GET_KEY_AGREEMENT:
            return client_pin_get_key_agreement(response, response_len);

        case PIN_CMD_GET_PIN_TOKEN:
            return client_pin_get_pin_token(params, params_len, response, response_len);

        case PIN_CMD_GET_PIN_UV_TOKEN:
            return client_pin_get_pin_uv_auth_token(params, params_len, response, response_len);

        case PIN_CMD_SET_PIN:
        case PIN_CMD_CHANGE_PIN:
            // Not supported - PIN is set via badge UI
            response[0] = CTAP2_ERR_UNSUPPORTED_OPTION;
            *response_len = 1;
            return CTAP2_ERR_UNSUPPORTED_OPTION;

        default:
            LOG_W("PIN", "Unknown subCommand: 0x%02lx", sub_command);
            response[0] = CTAP1_ERR_INVALID_COMMAND;
            *response_len = 1;
            return CTAP1_ERR_INVALID_COMMAND;
    }
}

// ============================================================================
// reset (0x07)
// ============================================================================

uint8_t ctap2_reset(uint8_t *response, uint16_t *response_len) {
    // Reset requires user presence within 10 seconds of power up
    // For now, just perform the reset
    if (!fido2_factory_reset()) {
        response[0] = CTAP2_ERR_OTHER;
        *response_len = 1;
        return CTAP2_ERR_OTHER;
    }

    response[0] = CTAP2_OK;
    *response_len = 1;
    LOG_I("CTAP2", "Factory reset complete");
    return CTAP2_OK;
}

// ============================================================================
// credentialManagement (0x0A)
// ============================================================================

// Helper: Count unique RPs among resident credentials
static uint8_t cred_mgmt_count_unique_rps(void) {
    uint8_t unique_hashes[FIDO2_MAX_CREDENTIALS][32];
    uint8_t count = 0;

    for (uint8_t slot = 0; slot < FIDO2_MAX_CREDENTIALS; slot++) {
        if (!fido2_storage_is_resident(slot)) continue;

        fido2_credential_info_t info;
        if (!fido2_storage_get_credential(slot, &info)) continue;

        // Check if this RP hash is already in our list
        bool found = false;
        for (uint8_t j = 0; j < count; j++) {
            if (memcmp(unique_hashes[j], info.rp_id_hash, 32) == 0) {
                found = true;
                break;
            }
        }

        if (!found && count < FIDO2_MAX_CREDENTIALS) {
            memcpy(unique_hashes[count], info.rp_id_hash, 32);
            g_cred_mgmt.rp_slots[count] = slot;  // Store a representative slot
            count++;
        }
    }

    return count;
}

// Helper: Find all resident credentials for an RP
static uint8_t cred_mgmt_find_creds_for_rp(const uint8_t *rp_id_hash) {
    uint8_t count = 0;

    for (uint8_t slot = 0; slot < FIDO2_MAX_CREDENTIALS && count < FIDO2_MAX_CREDENTIALS; slot++) {
        if (!fido2_storage_is_resident(slot)) continue;

        fido2_credential_info_t info;
        if (!fido2_storage_get_credential(slot, &info)) continue;

        if (memcmp(info.rp_id_hash, rp_id_hash, 32) == 0) {
            g_cred_mgmt.cred_slots[count++] = slot;
        }
    }

    return count;
}

// Helper: Encode RP response
static void cred_mgmt_encode_rp(cbor_writer_t *w, uint8_t slot, bool include_total) {
    fido2_credential_info_t info;
    if (!fido2_storage_get_credential(slot, &info)) return;

    // Map with 2 or 3 entries
    cbor_encode_map(w, include_total ? 3 : 2);

    // 0x03: rp (map with id)
    cbor_encode_uint(w, 0x03);
    cbor_encode_map(w, 1);
    cbor_encode_text(w, "id");
    cbor_encode_text(w, info.rp_id);

    // 0x04: rpIDHash
    cbor_encode_uint(w, 0x04);
    cbor_encode_bytes(w, info.rp_id_hash, 32);

    // 0x05: totalRPs (only in first response)
    if (include_total) {
        cbor_encode_uint(w, 0x05);
        cbor_encode_uint(w, g_cred_mgmt.rp_count);
    }
}

// Helper: Encode credential response
static void cred_mgmt_encode_credential(cbor_writer_t *w, uint8_t slot, bool include_total) {
    fido2_credential_info_t info;
    if (!fido2_storage_get_credential(slot, &info)) return;

    uint8_t cred_id[FIDO2_CRED_ID_LEN];
    if (!fido2_storage_get_cred_id(slot, cred_id)) return;

    uint8_t pubkey[65];
    if (!fido2_storage_get_pubkey(slot, pubkey)) return;

    // Map with 4 or 5 entries
    cbor_encode_map(w, include_total ? 5 : 4);

    // 0x06: user
    cbor_encode_uint(w, 0x06);
    cbor_encode_map(w, info.user_name[0] ? 2 : 1);
    cbor_encode_text(w, "id");
    cbor_encode_bytes(w, info.user_id, info.user_id_len);
    if (info.user_name[0]) {
        cbor_encode_text(w, "name");
        cbor_encode_text(w, info.user_name);
    }

    // 0x07: credentialID (PublicKeyCredentialDescriptor)
    cbor_encode_uint(w, 0x07);
    cbor_encode_map(w, 2);
    cbor_encode_text(w, "type");
    cbor_encode_text(w, "public-key");
    cbor_encode_text(w, "id");
    cbor_encode_bytes(w, cred_id, FIDO2_CRED_ID_LEN);

    // 0x08: publicKey (COSE_Key)
    cbor_encode_uint(w, 0x08);
    if (info.curve == 2) {  // Ed25519
        cbor_encode_map(w, 4);
        cbor_encode_int(w, 1);   // kty
        cbor_encode_int(w, 1);   // OKP
        cbor_encode_int(w, 3);   // alg
        cbor_encode_int(w, -8);  // EdDSA
        cbor_encode_int(w, -1);  // crv
        cbor_encode_int(w, 6);   // Ed25519
        cbor_encode_int(w, -2);  // x
        cbor_encode_bytes(w, pubkey + 1, 32);  // Skip 0x04 prefix
    } else {  // P-256
        cbor_encode_map(w, 5);
        cbor_encode_int(w, 1);   // kty
        cbor_encode_int(w, 2);   // EC2
        cbor_encode_int(w, 3);   // alg
        cbor_encode_int(w, -7);  // ES256
        cbor_encode_int(w, -1);  // crv
        cbor_encode_int(w, 1);   // P-256
        cbor_encode_int(w, -2);  // x
        cbor_encode_bytes(w, pubkey + 1, 32);
        cbor_encode_int(w, -3);  // y
        cbor_encode_bytes(w, pubkey + 33, 32);
    }

    // 0x09: totalCredentials (only in first response)
    if (include_total) {
        cbor_encode_uint(w, 0x09);
        cbor_encode_uint(w, g_cred_mgmt.cred_count);
    }

    // 0x0A: credProtect
    cbor_encode_uint(w, 0x0A);
    cbor_encode_uint(w, info.cred_protect ? info.cred_protect : 1);
}

uint8_t ctap2_cred_management(const uint8_t *params, uint16_t params_len,
                               uint8_t *response, uint16_t *response_len) {
    // Parse parameters
    if (params_len < 1) {
        response[0] = CTAP2_ERR_INVALID_CBOR;
        *response_len = 1;
        return CTAP2_ERR_INVALID_CBOR;
    }

    cbor_reader_t r;
    cbor_reader_init(&r, params, params_len);

    int map_count = cbor_read_map(&r);
    if (map_count < 1) {
        response[0] = CTAP2_ERR_INVALID_CBOR;
        *response_len = 1;
        return CTAP2_ERR_INVALID_CBOR;
    }

    uint8_t subcommand = 0;
    uint8_t rp_id_hash[32] = {0};
    bool has_rp_id_hash = false;
    uint8_t cred_id[FIDO2_CRED_ID_LEN] = {0};
    uint16_t cred_id_len = 0;
    bool has_cred_id = false;

    // Parse map entries
    for (int i = 0; i < map_count; i++) {
        uint64_t key;
        if (!cbor_read_uint(&r, &key)) {
            cbor_skip_item(&r);
            continue;
        }

        switch (key) {
            case 0x01:  // subCommand
                {
                    uint64_t cmd;
                    if (cbor_read_uint(&r, &cmd)) {
                        subcommand = (uint8_t)cmd;
                    }
                }
                break;

            case 0x02:  // subCommandParams
                {
                    int sub_count = cbor_read_map(&r);
                    for (int j = 0; j < sub_count; j++) {
                        uint64_t sub_key;
                        if (!cbor_read_uint(&r, &sub_key)) {
                            cbor_skip_item(&r);
                            cbor_skip_item(&r);
                            continue;
                        }

                        if (sub_key == 0x01) {  // rpIDHash
                            size_t len;
                            if (cbor_read_bytes(&r, rp_id_hash, 32, &len) && len == 32) {
                                has_rp_id_hash = true;
                            }
                        } else if (sub_key == 0x02) {  // credentialID
                            int cred_map = cbor_read_map(&r);
                            for (int k = 0; k < cred_map; k++) {
                                char cred_key[16];
                                size_t key_len;
                                if (cbor_read_text(&r, cred_key, sizeof(cred_key), &key_len)) {
                                    if (strcmp(cred_key, "id") == 0) {
                                        size_t len;
                                        if (cbor_read_bytes(&r, cred_id, FIDO2_CRED_ID_LEN, &len)) {
                                            cred_id_len = len;
                                            has_cred_id = true;
                                        }
                                    } else {
                                        cbor_skip_item(&r);
                                    }
                                } else {
                                    cbor_skip_item(&r);
                                    cbor_skip_item(&r);
                                }
                            }
                        } else {
                            cbor_skip_item(&r);
                        }
                    }
                }
                break;

            case 0x03:  // pinUvAuthProtocol
            case 0x04:  // pinUvAuthParam
                // We skip PIN auth verification for now
                // In production, should verify pinUvAuthParam
                cbor_skip_item(&r);
                break;

            default:
                cbor_skip_item(&r);
                break;
        }
    }

    LOG_I("CTAP2", "credMgmt subCmd=0x%02X", subcommand);

    cbor_writer_t w;
    cbor_writer_init(&w, response + 1, *response_len - 1);

    switch (subcommand) {
        case CRED_MGMT_GET_CREDS_METADATA:
            {
                // Count resident credentials
                uint8_t existing = 0;
                for (uint8_t slot = 0; slot < FIDO2_MAX_CREDENTIALS; slot++) {
                    if (fido2_storage_is_resident(slot)) existing++;
                }

                cbor_encode_map(&w, 2);

                // 0x01: existingResidentCredentialsCount
                cbor_encode_uint(&w, 0x01);
                cbor_encode_uint(&w, existing);

                // 0x02: maxPossibleRemainingResidentCredentialsCount
                cbor_encode_uint(&w, 0x02);
                cbor_encode_uint(&w, FIDO2_MAX_CREDENTIALS - existing);

                LOG_I("CTAP2", "credMgmt metadata: %d existing, %d remaining",
                      existing, FIDO2_MAX_CREDENTIALS - existing);
            }
            break;

        case CRED_MGMT_ENUMERATE_RPS_BEGIN:
            {
                g_cred_mgmt.rp_count = cred_mgmt_count_unique_rps();
                g_cred_mgmt.rp_index = 0;

                if (g_cred_mgmt.rp_count == 0) {
                    response[0] = CTAP2_ERR_NO_CREDENTIALS;
                    *response_len = 1;
                    return CTAP2_ERR_NO_CREDENTIALS;
                }

                cred_mgmt_encode_rp(&w, g_cred_mgmt.rp_slots[0], true);
                g_cred_mgmt.rp_index = 1;

                LOG_I("CTAP2", "credMgmt enumerateRPs: %d unique RPs", g_cred_mgmt.rp_count);
            }
            break;

        case CRED_MGMT_ENUMERATE_RPS_GET_NEXT:
            {
                if (g_cred_mgmt.rp_index >= g_cred_mgmt.rp_count) {
                    response[0] = CTAP2_ERR_NO_CREDENTIALS;
                    *response_len = 1;
                    return CTAP2_ERR_NO_CREDENTIALS;
                }

                cred_mgmt_encode_rp(&w, g_cred_mgmt.rp_slots[g_cred_mgmt.rp_index], false);
                g_cred_mgmt.rp_index++;
            }
            break;

        case CRED_MGMT_ENUMERATE_CREDS_BEGIN:
            {
                if (!has_rp_id_hash) {
                    response[0] = CTAP2_ERR_MISSING_PARAMETER;
                    *response_len = 1;
                    return CTAP2_ERR_MISSING_PARAMETER;
                }

                memcpy(g_cred_mgmt.current_rp_id_hash, rp_id_hash, 32);
                g_cred_mgmt.cred_count = cred_mgmt_find_creds_for_rp(rp_id_hash);
                g_cred_mgmt.cred_index = 0;

                if (g_cred_mgmt.cred_count == 0) {
                    response[0] = CTAP2_ERR_NO_CREDENTIALS;
                    *response_len = 1;
                    return CTAP2_ERR_NO_CREDENTIALS;
                }

                cred_mgmt_encode_credential(&w, g_cred_mgmt.cred_slots[0], true);
                g_cred_mgmt.cred_index = 1;

                LOG_I("CTAP2", "credMgmt enumerateCreds: %d credentials for RP", g_cred_mgmt.cred_count);
            }
            break;

        case CRED_MGMT_ENUMERATE_CREDS_GET_NEXT:
            {
                if (g_cred_mgmt.cred_index >= g_cred_mgmt.cred_count) {
                    response[0] = CTAP2_ERR_NO_CREDENTIALS;
                    *response_len = 1;
                    return CTAP2_ERR_NO_CREDENTIALS;
                }

                cred_mgmt_encode_credential(&w, g_cred_mgmt.cred_slots[g_cred_mgmt.cred_index], false);
                g_cred_mgmt.cred_index++;
            }
            break;

        case CRED_MGMT_DELETE_CREDENTIAL:
            {
                if (!has_cred_id) {
                    response[0] = CTAP2_ERR_MISSING_PARAMETER;
                    *response_len = 1;
                    return CTAP2_ERR_MISSING_PARAMETER;
                }

                // Find credential by ID
                int8_t slot = fido2_storage_find_slot_by_cred_id(cred_id, cred_id_len);
                if (slot < 0) {
                    response[0] = CTAP2_ERR_NO_CREDENTIALS;
                    *response_len = 1;
                    return CTAP2_ERR_NO_CREDENTIALS;
                }

                // Delete it
                if (!fido2_storage_delete_credential(slot)) {
                    response[0] = CTAP2_ERR_OTHER;
                    *response_len = 1;
                    return CTAP2_ERR_OTHER;
                }

                LOG_I("CTAP2", "credMgmt deleted credential slot %d", slot);

                // Success - empty response
                response[0] = CTAP2_OK;
                *response_len = 1;
                return CTAP2_OK;
            }

        default:
            response[0] = CTAP2_ERR_UNSUPPORTED_OPTION;
            *response_len = 1;
            return CTAP2_ERR_UNSUPPORTED_OPTION;
    }

    if (cbor_writer_error(&w)) {
        response[0] = CTAP2_ERR_OTHER;
        *response_len = 1;
        return CTAP2_ERR_OTHER;
    }

    response[0] = CTAP2_OK;
    *response_len = 1 + cbor_writer_length(&w);
    return CTAP2_OK;
}

// ============================================================================
// selection (0x0B)
// ============================================================================

uint8_t ctap2_selection(uint8_t *response, uint16_t *response_len) {
    // Selection just requires user presence
    if (!wait_for_user_presence(NULL, FIDO2_ACTION_AUTHENTICATE, NULL)) {
        response[0] = CTAP2_ERR_OPERATION_DENIED;
        *response_len = 1;
        return CTAP2_ERR_OPERATION_DENIED;
    }

    response[0] = CTAP2_OK;
    *response_len = 1;
    return CTAP2_OK;
}

// ============================================================================
// Main Command Processor
// ============================================================================

bool ctap2_init(void) {
    LOG_I("CTAP2", "Initializing...");
    memset(&g_ctap2, 0, sizeof(g_ctap2));
    g_ctap2.initialized = true;
    LOG_I("CTAP2", "Initialized");
    return true;
}

uint8_t ctap2_process_command(const uint8_t *cmd, uint16_t cmd_len,
                               uint8_t *response, uint16_t *response_len) {
    if (!g_ctap2.initialized || cmd_len < 1) {
        response[0] = CTAP1_ERR_INVALID_COMMAND;
        *response_len = 1;
        return CTAP1_ERR_INVALID_COMMAND;
    }

    uint8_t command = cmd[0];
    const uint8_t *params = cmd + 1;
    uint16_t params_len = cmd_len - 1;

    // Always log command type (helpful for debugging protocol issues)
    const char *cmd_name = "?";
    switch (command) {
        case 0x01: cmd_name = "makeCredential"; break;
        case 0x02: cmd_name = "getAssertion"; break;
        case 0x04: cmd_name = "getInfo"; break;
        case 0x06: cmd_name = "clientPIN"; break;
        case 0x07: cmd_name = "reset"; break;
        case 0x08: cmd_name = "getNextAssertion"; break;
        case 0x0A: cmd_name = "credMgmt"; break;
        case 0x0B: cmd_name = "selection"; break;
    }
    LOG_I("CTAP2", "CMD 0x%02X (%s) %d bytes", command, cmd_name, params_len);

    g_ctap2.operation_pending = true;
    g_ctap2.cancelled = false;

    uint8_t status;
    switch (command) {
        case CTAP2_CMD_GET_INFO:
            status = ctap2_get_info(response, response_len);
            break;

        case CTAP2_CMD_MAKE_CREDENTIAL:
            status = ctap2_make_credential(params, params_len, response, response_len);
            break;

        case CTAP2_CMD_GET_ASSERTION:
            status = ctap2_get_assertion(params, params_len, response, response_len);
            break;

        case CTAP2_CMD_GET_NEXT_ASSERTION:
            status = ctap2_get_next_assertion(response, response_len);
            break;

        case CTAP2_CMD_CLIENT_PIN:
            status = ctap2_client_pin(params, params_len, response, response_len);
            break;

        case CTAP2_CMD_RESET:
            status = ctap2_reset(response, response_len);
            break;

        case CTAP2_CMD_CRED_MANAGEMENT:
            status = ctap2_cred_management(params, params_len, response, response_len);
            break;

        case CTAP2_CMD_SELECTION:
            status = ctap2_selection(response, response_len);
            break;

        case CTAP2_CMD_LARGE_BLOBS:
        case CTAP2_CMD_CONFIG:
            response[0] = CTAP2_ERR_UNSUPPORTED_OPTION;
            *response_len = 1;
            status = CTAP2_ERR_UNSUPPORTED_OPTION;
            break;

        default:
            response[0] = CTAP1_ERR_INVALID_COMMAND;
            *response_len = 1;
            status = CTAP1_ERR_INVALID_COMMAND;
            break;
    }

    g_ctap2.operation_pending = false;
    return status;
}

void ctap2_send_keepalive(uint8_t status) {
    uint32_t cid = ctaphid_get_current_cid();
    if (cid != 0) {
        ctaphid_send_keepalive(cid, status);
    }
}

void ctap2_cancel(void) {
    g_ctap2.cancelled = true;
    if (CTAP2_DEBUG_COMMANDS) LOG_D("CTAP2", "Operation cancelled");
}
