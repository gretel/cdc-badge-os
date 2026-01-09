// U2F/CTAP1 Protocol Implementation
// Legacy U2F support for Chrome compatibility

#include "u2f.h"
#include "fido2.h"
#include "fido2_storage.h"
#include "tropic01.h"
#include "cdc_log.h"
#include <mbedtls/sha256.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Attestation Certificate
// ============================================================================

#define U2F_ATTEST_SLOT TR01_ECC_SLOT_ATTEST  // Slot 30

// Cached attestation certificate (DER encoded)
static uint8_t g_attest_cert[U2F_MAX_ATT_CERT_SIZE];
static uint16_t g_attest_cert_len = 0;
static uint8_t g_attest_pubkey[65];  // 0x04 || X || Y
static bool g_attest_initialized = false;

// ============================================================================
// Helper Functions
// ============================================================================

static void sha256(const uint8_t *data, size_t len, uint8_t *hash) {
    mbedtls_sha256(data, len, hash, 0);
}

// Sign with attestation key (slot 30)
static bool u2f_attest_sign(const uint8_t *data, size_t data_len,
                             uint8_t *signature, uint8_t *sig_len) {
    // Hash the data
    uint8_t hash[32];
    sha256(data, data_len, hash);

    // Sign with TROPIC01 slot 30
    uint8_t raw_sig[64];  // R || S
    if (!tropic01_ecdsa_sign(U2F_ATTEST_SLOT, hash, 32, raw_sig)) {
        LOG_E("U2F", "Attestation signing failed");
        return false;
    }

    // Convert to DER format
    uint8_t *p = signature;
    *p++ = 0x30;  // SEQUENCE

    // Calculate lengths for R and S
    int r_len = 32, s_len = 32;
    int r_pad = (raw_sig[0] & 0x80) ? 1 : 0;
    int s_pad = (raw_sig[32] & 0x80) ? 1 : 0;

    // Skip leading zeros but keep at least one byte
    while (r_len > 1 && raw_sig[32 - r_len] == 0 && !(raw_sig[33 - r_len] & 0x80)) r_len--;
    while (s_len > 1 && raw_sig[64 - s_len] == 0 && !(raw_sig[65 - s_len] & 0x80)) s_len--;

    int total_len = 2 + r_len + r_pad + 2 + s_len + s_pad;
    *p++ = total_len;

    // R integer
    *p++ = 0x02;
    *p++ = r_len + r_pad;
    if (r_pad) *p++ = 0x00;
    memcpy(p, raw_sig + (32 - r_len), r_len);
    p += r_len;

    // S integer
    *p++ = 0x02;
    *p++ = s_len + s_pad;
    if (s_pad) *p++ = 0x00;
    memcpy(p, raw_sig + 32 + (32 - s_len), s_len);
    p += s_len;

    *sig_len = p - signature;
    return true;
}

// ============================================================================
// Attestation Initialization
// ============================================================================

bool u2f_init_attestation(void) {
    if (g_attest_initialized) {
        return true;
    }

    LOG_I("U2F", "Initializing attestation...");

    // Check if attestation key exists in slot 30
    uint8_t pubkey[65];
    uint8_t curve, origin;

    if (!tropic01_ecc_key_read(U2F_ATTEST_SLOT, pubkey, 65, &curve, &origin)) {
        // Key doesn't exist - generate it
        LOG_I("U2F", "Generating attestation key in slot %d", U2F_ATTEST_SLOT);

        if (!tropic01_ecc_key_generate(U2F_ATTEST_SLOT, CDC_CURVE_P256)) {
            LOG_E("U2F", "Failed to generate attestation key");
            return false;
        }

        // Read back public key
        if (!tropic01_ecc_key_read(U2F_ATTEST_SLOT, pubkey, 65, &curve, &origin)) {
            LOG_E("U2F", "Failed to read attestation public key");
            return false;
        }
    }

    LOG_I("U2F", "Attestation key ready (curve=%d, origin=%d)", curve, origin);

    // Store public key (with 0x04 prefix)
    g_attest_pubkey[0] = 0x04;
    memcpy(g_attest_pubkey + 1, pubkey, 64);

    // Build self-signed X.509 certificate manually (DER encoded)
    // This is a minimal certificate structure for U2F
    uint8_t *cert = g_attest_cert;
    uint8_t *p = cert;

    // We'll build the TBS (To Be Signed) certificate, sign it, then wrap

    // TBS Certificate structure
    uint8_t tbs[512];
    uint8_t *t = tbs;

    // Version [0] EXPLICIT INTEGER = 2 (v3)
    *t++ = 0xA0; *t++ = 0x03;  // [0] EXPLICIT
    *t++ = 0x02; *t++ = 0x01; *t++ = 0x02;  // INTEGER 2

    // Serial number - random
    uint8_t serial[8];
    tropic01_get_random(serial, 8);
    serial[0] &= 0x7F;  // Ensure positive
    *t++ = 0x02; *t++ = 0x08;  // INTEGER
    memcpy(t, serial, 8);
    t += 8;

    // Signature algorithm: ecdsa-with-SHA256 (1.2.840.10045.4.3.2)
    static const uint8_t ecdsa_sha256_oid[] = {
        0x30, 0x0A,  // SEQUENCE
        0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02  // OID
    };
    memcpy(t, ecdsa_sha256_oid, sizeof(ecdsa_sha256_oid));
    t += sizeof(ecdsa_sha256_oid);

    // Issuer: CN=CDC Badge U2F (13 chars)
    // Structure: SEQUENCE { SET { SEQUENCE { OID(CN), UTF8String } } }
    // Lengths: UTF8=13, inner SEQ=5+2+13=20, SET=22, outer SEQ=24
    static const uint8_t issuer[] = {
        0x30, 0x18,  // SEQUENCE (24 bytes)
        0x31, 0x16,  // SET (22 bytes)
        0x30, 0x14,  // SEQUENCE (20 bytes)
        0x06, 0x03, 0x55, 0x04, 0x03,  // OID: CN (5 bytes)
        0x0C, 0x0D,  // UTF8String length=13
        'C', 'D', 'C', ' ', 'B', 'a', 'd', 'g', 'e', ' ', 'U', '2', 'F'
    };
    memcpy(t, issuer, sizeof(issuer));
    t += sizeof(issuer);

    // Validity (2024-01-01 to 2049-12-31)
    // UTCTime: 00-49 = 2000-2049, 50-99 = 1950-1999
    static const uint8_t validity[] = {
        0x30, 0x1E,  // SEQUENCE
        0x17, 0x0D, '2', '4', '0', '1', '0', '1', '0', '0', '0', '0', '0', '0', 'Z',  // notBefore: 2024-01-01
        0x17, 0x0D, '4', '9', '1', '2', '3', '1', '2', '3', '5', '9', '5', '9', 'Z'   // notAfter:  2049-12-31
    };
    memcpy(t, validity, sizeof(validity));
    t += sizeof(validity);

    // Subject: same as issuer
    memcpy(t, issuer, sizeof(issuer));
    t += sizeof(issuer);

    // Subject Public Key Info
    // AlgorithmIdentifier: ecPublicKey + prime256v1
    static const uint8_t spki_prefix[] = {
        0x30, 0x59,  // SEQUENCE (89 bytes total)
        0x30, 0x13,  // SEQUENCE (algorithm)
        0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01,  // OID: ecPublicKey
        0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07,  // OID: prime256v1
        0x03, 0x42, 0x00  // BIT STRING (66 bytes, 0 unused bits)
    };
    memcpy(t, spki_prefix, sizeof(spki_prefix));
    t += sizeof(spki_prefix);

    // Public key (0x04 || X || Y)
    memcpy(t, g_attest_pubkey, 65);
    t += 65;

    size_t tbs_len = t - tbs;

    // Now wrap TBS in SEQUENCE
    uint8_t tbs_wrapped[600];
    uint8_t *tw = tbs_wrapped;

    *tw++ = 0x30;  // SEQUENCE tag
    if (tbs_len < 128) {
        *tw++ = tbs_len;
    } else {
        *tw++ = 0x82;
        *tw++ = (tbs_len >> 8) & 0xFF;
        *tw++ = tbs_len & 0xFF;
    }
    memcpy(tw, tbs, tbs_len);
    tw += tbs_len;

    size_t tbs_wrapped_len = tw - tbs_wrapped;

    // Sign the TBS
    uint8_t sig[U2F_MAX_EC_SIG_SIZE];
    uint8_t sig_len = 0;

    if (!u2f_attest_sign(tbs_wrapped, tbs_wrapped_len, sig, &sig_len)) {
        LOG_E("U2F", "Failed to sign certificate");
        return false;
    }

    // Build complete certificate:
    // SEQUENCE { TBS, SignatureAlgorithm, Signature }
    size_t cert_content_len = tbs_wrapped_len + sizeof(ecdsa_sha256_oid) + 2 + 1 + sig_len;

    *p++ = 0x30;  // SEQUENCE tag
    if (cert_content_len < 128) {
        *p++ = cert_content_len;
    } else {
        *p++ = 0x82;
        *p++ = (cert_content_len >> 8) & 0xFF;
        *p++ = cert_content_len & 0xFF;
    }

    // TBS Certificate (wrapped)
    memcpy(p, tbs_wrapped, tbs_wrapped_len);
    p += tbs_wrapped_len;

    // Signature Algorithm
    memcpy(p, ecdsa_sha256_oid, sizeof(ecdsa_sha256_oid));
    p += sizeof(ecdsa_sha256_oid);

    // Signature BIT STRING
    *p++ = 0x03;  // BIT STRING tag
    *p++ = sig_len + 1;  // length (signature + unused bits byte)
    *p++ = 0x00;  // unused bits
    memcpy(p, sig, sig_len);
    p += sig_len;

    g_attest_cert_len = p - cert;
    g_attest_initialized = true;

    LOG_I("U2F", "Attestation certificate generated (%d bytes)", g_attest_cert_len);
    return true;
}

bool u2f_get_attestation_cert(const uint8_t **cert, uint16_t *cert_len) {
    if (!g_attest_initialized || !cert || !cert_len) {
        return false;
    }
    *cert = g_attest_cert;
    *cert_len = g_attest_cert_len;
    return true;
}

bool u2f_attestation_sign(const uint8_t *data, size_t data_len,
                          uint8_t *signature, uint8_t *sig_len) {
    if (!g_attest_initialized) {
        return false;
    }
    return u2f_attest_sign(data, data_len, signature, sig_len);
}

static uint16_t u2f_response_sw(uint8_t *response, uint16_t sw) {
    response[0] = (sw >> 8) & 0xFF;
    response[1] = sw & 0xFF;
    return 2;
}

static uint16_t u2f_response_error(uint8_t *response, uint16_t sw) {
    return u2f_response_sw(response, sw);
}

// ============================================================================
// U2F Version (INS 0x03)
// ============================================================================

static uint16_t u2f_version(uint8_t *response, uint16_t response_max) {
    const char *version = "U2F_V2";
    size_t len = strlen(version);

    if (response_max < len + 2) {
        return u2f_response_error(response, U2F_SW_WRONG_LENGTH);
    }

    memcpy(response, version, len);
    response[len] = 0x90;
    response[len + 1] = 0x00;

    LOG_I("U2F", "Version request: U2F_V2");
    return len + 2;
}

// ============================================================================
// U2F Register (INS 0x01)
// ============================================================================

// Check if this is a dummy/blink request (Chrome sends these for device selection)
// Dummy requests have repetitive application hashes like 0x41414141... (AAAA...)
static bool is_dummy_application(const uint8_t *application) {
    uint8_t first = application[0];
    // Check if all 32 bytes are the same (dummy pattern)
    for (int i = 1; i < 32; i++) {
        if (application[i] != first) {
            return false;
        }
    }
    LOG_I("U2F", "Detected dummy/blink request (app=0x%02x...)", first);
    return true;
}

static uint16_t u2f_register(const uint8_t *challenge, const uint8_t *application,
                              uint8_t *response, uint16_t response_max) {
    LOG_I("U2F", "Register request");

    bool is_dummy = is_dummy_application(application);

    // For dummy/blink requests (app hash = 0x41414141... or similar),
    // Chrome is probing for device presence before real registration.
    // We need to wait for actual user touch, then return a valid-looking response.
    // Chrome will discard the result but recognize the touch happened.
    if (is_dummy) {
        // Show identifier based on the dummy byte pattern
        char dummy_id[16];
        snprintf(dummy_id, sizeof(dummy_id), "U2F:%02x%02x%02x%02x",
                 application[0], application[1], application[2], application[3]);

        // Request user presence - SELECT action for device selection
        fido2_user_presence_result_t up_result = fido2_request_user_presence(
            dummy_id, FIDO2_ACTION_SELECT, NULL);

        if (up_result != FIDO2_UP_APPROVED) {
            LOG_D("U2F", "Dummy: no user presence yet");
            return u2f_response_error(response, U2F_SW_CONDITIONS_NOT_SATISFIED);
        }

        // User touched - generate a dummy response (random data, not stored)
        LOG_I("U2F", "Dummy: user touched - generating response");
        uint8_t dummy_cred[U2F_KEY_HANDLE_SIZE];
        uint8_t dummy_pubkey[64];
        tropic01_get_random(dummy_cred, U2F_KEY_HANDLE_SIZE);
        tropic01_get_random(dummy_pubkey, 64);

        // Build minimal response: 0x05 || pubkey || kh_len || kh || cert || sig
        uint16_t offset = 0;
        response[offset++] = U2F_REGISTER_ID;
        response[offset++] = 0x04;
        memcpy(response + offset, dummy_pubkey, 64);
        offset += 64;
        response[offset++] = U2F_KEY_HANDLE_SIZE;
        memcpy(response + offset, dummy_cred, U2F_KEY_HANDLE_SIZE);
        offset += U2F_KEY_HANDLE_SIZE;

        // Add attestation cert
        memcpy(response + offset, g_attest_cert, g_attest_cert_len);
        offset += g_attest_cert_len;

        // Sign with attestation key
        uint8_t to_sign[1 + 32 + 32 + U2F_KEY_HANDLE_SIZE + 65];
        size_t to_sign_len = 0;
        to_sign[to_sign_len++] = 0x00;
        memcpy(to_sign + to_sign_len, application, 32);
        to_sign_len += 32;
        memcpy(to_sign + to_sign_len, challenge, 32);
        to_sign_len += 32;
        memcpy(to_sign + to_sign_len, dummy_cred, U2F_KEY_HANDLE_SIZE);
        to_sign_len += U2F_KEY_HANDLE_SIZE;
        to_sign[to_sign_len++] = 0x04;
        memcpy(to_sign + to_sign_len, dummy_pubkey, 64);
        to_sign_len += 64;

        uint8_t signature[U2F_MAX_EC_SIG_SIZE];
        uint8_t sig_len = 0;
        if (!u2f_attest_sign(to_sign, to_sign_len, signature, &sig_len)) {
            return u2f_response_error(response, U2F_SW_WRONG_DATA);
        }
        memcpy(response + offset, signature, sig_len);
        offset += sig_len;

        response[offset++] = 0x90;
        response[offset++] = 0x00;

        LOG_I("U2F", "Dummy response complete, len=%u", offset);
        return offset;
    }

    // Create unique identifier from application hash (first 4 bytes as hex)
    char rp_id[16];
    snprintf(rp_id, sizeof(rp_id), "U2F:%02x%02x%02x%02x",
             application[0], application[1], application[2], application[3]);

    // Request user presence for real registration
    fido2_user_presence_result_t up_result = fido2_request_user_presence(
        rp_id, FIDO2_ACTION_REGISTER, NULL);

    if (up_result != FIDO2_UP_APPROVED) {
        LOG_I("U2F", "User presence denied");
        return u2f_response_error(response, U2F_SW_CONDITIONS_NOT_SATISFIED);
    }

    uint8_t cred_id[U2F_KEY_HANDLE_SIZE];
    uint8_t pubkey[64];  // X || Y (no 0x04 prefix)

    {
        // Real registration - create and store credential
        uint8_t slot;
        uint8_t user_id[1] = {0};

        if (!fido2_storage_create_credential(
                rp_id, application, user_id, 1, "U2F",
                false, 0, CDC_CURVE_P256, &slot, cred_id, pubkey)) {
            LOG_E("U2F", "Failed to create credential");
            return u2f_response_error(response, U2F_SW_WRONG_DATA);
        }
        LOG_I("U2F", "Created credential in slot %d", slot);
    }

    // Build registration response:
    // 0x05 || pubkey (65) || keyHandleLen (1) || keyHandle || attestation cert || signature
    uint16_t offset = 0;

    // Reserved byte
    response[offset++] = U2F_REGISTER_ID;

    // Public key (uncompressed: 0x04 || X || Y)
    response[offset++] = 0x04;
    memcpy(response + offset, pubkey, 64);
    offset += 64;

    // Key handle length
    response[offset++] = U2F_KEY_HANDLE_SIZE;

    // Key handle (credential ID)
    memcpy(response + offset, cred_id, U2F_KEY_HANDLE_SIZE);
    offset += U2F_KEY_HANDLE_SIZE;

    // Attestation certificate
    if (!g_attest_initialized) {
        LOG_E("U2F", "Attestation not initialized");
        return u2f_response_error(response, U2F_SW_WRONG_DATA);
    }

    if (offset + g_attest_cert_len + U2F_MAX_EC_SIG_SIZE + 2 > response_max) {
        LOG_E("U2F", "Response buffer too small");
        return u2f_response_error(response, U2F_SW_WRONG_LENGTH);
    }

    memcpy(response + offset, g_attest_cert, g_attest_cert_len);
    offset += g_attest_cert_len;

    // Build data to sign: 0x00 || appParam || challenge || keyHandle || pubkey
    // This is signed with the ATTESTATION key, not the credential key
    uint8_t to_sign[1 + 32 + 32 + U2F_KEY_HANDLE_SIZE + 65];
    size_t to_sign_len = 0;

    to_sign[to_sign_len++] = 0x00;  // Reserved
    memcpy(to_sign + to_sign_len, application, 32);
    to_sign_len += 32;
    memcpy(to_sign + to_sign_len, challenge, 32);
    to_sign_len += 32;
    memcpy(to_sign + to_sign_len, cred_id, U2F_KEY_HANDLE_SIZE);
    to_sign_len += U2F_KEY_HANDLE_SIZE;
    to_sign[to_sign_len++] = 0x04;
    memcpy(to_sign + to_sign_len, pubkey, 64);
    to_sign_len += 64;

    // Sign with attestation key (slot 30)
    uint8_t signature[U2F_MAX_EC_SIG_SIZE];
    uint8_t sig_len = 0;

    if (!u2f_attest_sign(to_sign, to_sign_len, signature, &sig_len)) {
        LOG_E("U2F", "Attestation signing failed");
        return u2f_response_error(response, U2F_SW_WRONG_DATA);
    }

    memcpy(response + offset, signature, sig_len);
    offset += sig_len;

    // Status word
    response[offset++] = 0x90;
    response[offset++] = 0x00;

    LOG_I("U2F", "Register complete, response len=%u", offset);
    return offset;
}

// ============================================================================
// U2F Authenticate (INS 0x02)
// ============================================================================

static uint16_t u2f_authenticate(uint8_t p1, const uint8_t *challenge,
                                  const uint8_t *application,
                                  const uint8_t *key_handle, uint8_t key_handle_len,
                                  uint8_t *response, uint16_t response_max) {
    LOG_I("U2F", "Authenticate request, p1=0x%02X, kh_len=%d", p1, key_handle_len);

    if (key_handle_len != U2F_KEY_HANDLE_SIZE) {
        LOG_W("U2F", "Invalid key handle length: %d", key_handle_len);
        return u2f_response_error(response, U2F_SW_WRONG_DATA);
    }

    // Find credential by key handle
    int8_t slot = fido2_storage_find_slot_by_cred_id(key_handle, key_handle_len);
    if (slot < 0) {
        LOG_W("U2F", "Key handle not found");
        return u2f_response_error(response, U2F_SW_WRONG_DATA);
    }

    // Verify RP ID hash matches
    fido2_credential_info_t cred;
    if (!fido2_storage_get_credential(slot, &cred)) {
        LOG_E("U2F", "Failed to get credential info");
        return u2f_response_error(response, U2F_SW_WRONG_DATA);
    }

    if (memcmp(cred.rp_id_hash, application, 32) != 0) {
        LOG_W("U2F", "Application hash mismatch");
        return u2f_response_error(response, U2F_SW_WRONG_DATA);
    }

    // Check-only mode - just verify key handle is valid
    if (p1 == U2F_AUTH_CHECK_ONLY) {
        LOG_I("U2F", "Check-only: key handle valid");
        return u2f_response_error(response, U2F_SW_CONDITIONS_NOT_SATISFIED);
    }

    // Request user presence (unless dont-enforce)
    if (p1 == U2F_AUTH_ENFORCE) {
        fido2_user_presence_result_t up_result = fido2_request_user_presence(
            cred.rp_id, FIDO2_ACTION_AUTHENTICATE, cred.user_name);

        if (up_result != FIDO2_UP_APPROVED) {
            LOG_I("U2F", "User presence denied");
            return u2f_response_error(response, U2F_SW_CONDITIONS_NOT_SATISFIED);
        }
    }

    // Increment counter
    uint32_t counter = fido2_storage_increment_sign_count(slot);
    fido2_increment_auth_counter();

    // Build authentication response:
    // userPresence (1) || counter (4) || signature
    uint16_t offset = 0;

    // User presence flag
    response[offset++] = 0x01;  // UP=1

    // Counter (big-endian)
    response[offset++] = (counter >> 24) & 0xFF;
    response[offset++] = (counter >> 16) & 0xFF;
    response[offset++] = (counter >> 8) & 0xFF;
    response[offset++] = counter & 0xFF;

    // Build data to sign: appParam || userPresence || counter || challenge
    uint8_t to_sign[32 + 1 + 4 + 32];
    size_t to_sign_len = 0;

    memcpy(to_sign + to_sign_len, application, 32);
    to_sign_len += 32;
    to_sign[to_sign_len++] = 0x01;  // User presence
    to_sign[to_sign_len++] = (counter >> 24) & 0xFF;
    to_sign[to_sign_len++] = (counter >> 16) & 0xFF;
    to_sign[to_sign_len++] = (counter >> 8) & 0xFF;
    to_sign[to_sign_len++] = counter & 0xFF;
    memcpy(to_sign + to_sign_len, challenge, 32);
    to_sign_len += 32;

    // Sign with TROPIC01
    uint8_t signature[U2F_MAX_EC_SIG_SIZE];
    uint8_t sig_len = 0;

    if (!fido2_storage_sign_raw(slot, to_sign, to_sign_len, signature, &sig_len)) {
        LOG_E("U2F", "Signing failed");
        return u2f_response_error(response, U2F_SW_WRONG_DATA);
    }

    memcpy(response + offset, signature, sig_len);
    offset += sig_len;

    // Status word
    response[offset++] = 0x90;
    response[offset++] = 0x00;

    LOG_I("U2F", "Authenticate complete, counter=%u, response len=%u", counter, offset);
    return offset;
}

// ============================================================================
// APDU Parser
// ============================================================================

uint16_t u2f_process_apdu(const uint8_t *apdu, uint16_t apdu_len,
                          uint8_t *response, uint16_t response_max) {
    if (apdu_len < 4) {
        LOG_W("U2F", "APDU too short: %d", apdu_len);
        return u2f_response_error(response, U2F_SW_WRONG_LENGTH);
    }

    uint8_t cla = apdu[0];
    uint8_t ins = apdu[1];
    uint8_t p1 = apdu[2];
    uint8_t p2 = apdu[3];

    // Only support CLA=0x00
    if (cla != 0x00) {
        LOG_W("U2F", "Unsupported CLA: 0x%02X", cla);
        return u2f_response_error(response, U2F_SW_CLA_NOT_SUPPORTED);
    }

    LOG_I("U2F", "APDU: CLA=0x%02X INS=0x%02X P1=0x%02X P2=0x%02X len=%d",
          cla, ins, p1, p2, apdu_len);

    // Parse extended length APDU
    // Format: CLA INS P1 P2 [Lc(3)] [DATA] [Le(2)]
    uint32_t data_len = 0;
    const uint8_t *data = NULL;

    if (apdu_len > 4) {
        if (apdu[4] == 0x00 && apdu_len > 6) {
            // Extended length: 00 Lc1 Lc2
            data_len = (apdu[5] << 8) | apdu[6];
            data = apdu + 7;
            if (data_len + 7 > apdu_len) {
                data_len = apdu_len - 7;
            }
        } else {
            // Short length: Lc
            data_len = apdu[4];
            data = apdu + 5;
            if (data_len + 5 > apdu_len) {
                data_len = apdu_len - 5;
            }
        }
    }

    switch (ins) {
        case U2F_INS_VERSION:
            return u2f_version(response, response_max);

        case U2F_INS_REGISTER:
            if (data_len < U2F_CHALLENGE_SIZE + U2F_APPLICATION_SIZE) {
                LOG_W("U2F", "Register: insufficient data: %u", data_len);
                return u2f_response_error(response, U2F_SW_WRONG_LENGTH);
            }
            return u2f_register(data, data + U2F_CHALLENGE_SIZE,
                               response, response_max);

        case U2F_INS_AUTHENTICATE:
            if (data_len < U2F_CHALLENGE_SIZE + U2F_APPLICATION_SIZE + 1) {
                LOG_W("U2F", "Authenticate: insufficient data: %u", data_len);
                return u2f_response_error(response, U2F_SW_WRONG_LENGTH);
            }
            {
                uint8_t kh_len = data[U2F_CHALLENGE_SIZE + U2F_APPLICATION_SIZE];
                const uint8_t *kh = data + U2F_CHALLENGE_SIZE + U2F_APPLICATION_SIZE + 1;

                if (data_len < U2F_CHALLENGE_SIZE + U2F_APPLICATION_SIZE + 1 + kh_len) {
                    LOG_W("U2F", "Authenticate: key handle truncated");
                    return u2f_response_error(response, U2F_SW_WRONG_LENGTH);
                }

                return u2f_authenticate(p1, data, data + U2F_CHALLENGE_SIZE,
                                        kh, kh_len, response, response_max);
            }

        default:
            LOG_W("U2F", "Unsupported INS: 0x%02X", ins);
            return u2f_response_error(response, U2F_SW_INS_NOT_SUPPORTED);
    }
}
