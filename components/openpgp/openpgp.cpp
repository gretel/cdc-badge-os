/*
 * OpenPGP SmartCard Application for CDC Badge
 *
 * Based on pico-openpgp (https://github.com/polhenarejos/pico-openpgp)
 * Original: Copyright (c) 2022 Pol Henarejos, AGPLv3
 * Adapted for CDC Badge with TROPIC01 Secure Element
 *
 * This implementation follows OpenPGP 3.4.1 specification:
 * https://gnupg.org/ftp/specs/OpenPGP-smart-card-application-3.4.pdf
 *
 * Key differences from pico-openpgp:
 * - Uses TROPIC01 Secure Element for key storage and signing
 * - Keys never leave the secure element
 * - Integrates with existing CDC Badge infrastructure
 */

#include "openpgp.h"
#include "apdu.h"
#include "gpg.h"           // CDC Badge GPG component (TROPIC01 backend)
#include "tropic01.h"      // TROPIC01 API (CDC_CURVE_ED25519, CDC_CURVE_P256, secure_random)
#include <string.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <nvs.h>

static const char *TAG = "OpenPGP";

// OpenPGP Application ID (RID + PIX)
// D2 76 00 01 24 01 = OpenPGP
const uint8_t OPENPGP_AID[] = {
    0xD2, 0x76, 0x00, 0x01, 0x24, 0x01,  // RID + Application
    0x03, 0x04,                           // Version 3.4
    0xFF, 0xFE,                           // Manufacturer: test
    0x00, 0x00, 0x00, 0x01,              // Serial number
    0x00, 0x00                            // RFU
};
const uint8_t OPENPGP_AID_LEN = sizeof(OPENPGP_AID);

// ATR (shared with ccid.cpp)
const uint8_t OPENPGP_ATR[] = {
    0x3B, 0xDA, 0x18, 0xFF, 0x81, 0xB1, 0xFE, 0x75,
    0x1F, 0x03, 'C', 'D', 'C', 0x00, 0x90, 0x00, 0x00
};
const uint8_t OPENPGP_ATR_LEN = sizeof(OPENPGP_ATR);

// Application state
static bool app_selected = false;
static bool pw1_verified = false;
static bool pw3_verified = false;
static uint32_t sig_count = 0;

// PIN retry counters (stored in NVS)
static uint8_t pw1_retries = 3;
static uint8_t pw3_retries = 3;

// NVS namespace for OpenPGP data
#define NVS_NAMESPACE "openpgp"

// Data Object storage (20 bytes fingerprint, etc.)
static uint8_t fingerprint_sig[20] = {0};
static uint8_t fingerprint_dec[20] = {0};
static uint8_t fingerprint_aut[20] = {0};

// Helper: Load state from NVS
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

        nvs_close(nvs);
    }
}

// Helper: Save state to NVS
static void save_state_to_nvs(void) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_u32(nvs, "sig_count", sig_count);
        nvs_set_blob(nvs, "fp_sig", fingerprint_sig, sizeof(fingerprint_sig));
        nvs_set_blob(nvs, "fp_dec", fingerprint_dec, sizeof(fingerprint_dec));
        nvs_set_blob(nvs, "fp_aut", fingerprint_aut, sizeof(fingerprint_aut));
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

bool openpgp_init(void) {
    // Initialize GPG component (TROPIC01 backend)
    if (!gpg_init()) {
        ESP_LOGE(TAG, "Failed to initialize GPG/TROPIC01");
        return false;
    }

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

// Process SELECT command
static int cmd_select(const apdu_t *apdu, uint8_t *resp, size_t resp_max) {
    // Check for OpenPGP AID
    if (apdu->lc >= 6 && memcmp(apdu->data, OPENPGP_AID, 6) == 0) {
        app_selected = true;
        pw1_verified = false;
        pw3_verified = false;
        ESP_LOGI(TAG, "OpenPGP application selected");
        return apdu_sw(resp, SW_OK);
    }

    return apdu_sw(resp, SW_FILE_NOT_FOUND);
}

// Process GET DATA command
static int cmd_get_data(const apdu_t *apdu, uint8_t *resp, size_t resp_max) {
    uint16_t tag = (apdu->p1 << 8) | apdu->p2;

    switch (tag) {
        case 0x004F:  // Full AID
            return apdu_build_response(resp, resp_max, OPENPGP_AID, OPENPGP_AID_LEN, SW_OK);

        case DO_FP_SIG:  // Fingerprint SIG
            return apdu_build_response(resp, resp_max, fingerprint_sig, 20, SW_OK);

        case DO_FP_DEC:  // Fingerprint DEC
            return apdu_build_response(resp, resp_max, fingerprint_dec, 20, SW_OK);

        case DO_FP_AUT:  // Fingerprint AUT
            return apdu_build_response(resp, resp_max, fingerprint_aut, 20, SW_OK);

        case DO_SIG_COUNT: {  // Signature counter
            uint8_t count[3] = {
                (uint8_t)((sig_count >> 16) & 0xFF),
                (uint8_t)((sig_count >> 8) & 0xFF),
                (uint8_t)(sig_count & 0xFF)
            };
            return apdu_build_response(resp, resp_max, count, 3, SW_OK);
        }

        case DO_PW_STATUS: {  // PW Status
            uint8_t status[7] = {
                0x01,       // PW1 valid for multiple signatures
                127,        // Max length PW1 (UTF-8)
                127,        // Max length RC
                127,        // Max length PW3
                pw1_retries,
                0,          // RC retries
                pw3_retries
            };
            return apdu_build_response(resp, resp_max, status, 7, SW_OK);
        }

        default:
            ESP_LOGW(TAG, "GET DATA: Unknown tag 0x%04X", tag);
            return apdu_sw(resp, SW_FILE_NOT_FOUND);
    }
}

// Process PUT DATA command
static int cmd_put_data(const apdu_t *apdu, uint8_t *resp, size_t resp_max) {
    if (!pw3_verified) {
        return apdu_sw(resp, SW_SECURITY_NOT_SATISFIED);
    }

    uint16_t tag = (apdu->p1 << 8) | apdu->p2;

    switch (tag) {
        case DO_FP_SIG:
            if (apdu->lc == 20) {
                memcpy(fingerprint_sig, apdu->data, 20);
                save_state_to_nvs();
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

        default:
            ESP_LOGW(TAG, "PUT DATA: Unknown tag 0x%04X", tag);
            return apdu_sw(resp, SW_FILE_NOT_FOUND);
    }
}

// Process VERIFY command (PIN verification)
static int cmd_verify(const apdu_t *apdu, uint8_t *resp, size_t resp_max) {
    uint8_t pw_ref = apdu->p2;

    // Check remaining retries
    if (apdu->lc == 0) {
        uint8_t retries = (pw_ref == 0x81 || pw_ref == 0x82) ? pw1_retries : pw3_retries;
        if (retries == 0) {
            return apdu_sw(resp, SW_AUTH_METHOD_BLOCKED);
        }
        return apdu_sw(resp, 0x63C0 | retries);
    }

    // Verify PIN using GPG component (which uses TROPIC01)
    // Note: In production, this would verify against stored PIN hash
    bool verified = false;

    if (pw_ref == 0x81 || pw_ref == 0x82) {
        // PW1 verification
        // For now, accept any PIN (demo mode)
        // TODO: Implement proper PIN verification via TROPIC01
        verified = (apdu->lc >= 6);  // Minimum 6 digits
        if (verified) {
            pw1_verified = true;
            pw1_retries = 3;
        } else {
            pw1_retries--;
        }
    } else if (pw_ref == 0x83) {
        // PW3 (Admin PIN) verification
        verified = (apdu->lc >= 8);  // Minimum 8 digits
        if (verified) {
            pw3_verified = true;
            pw3_retries = 3;
        } else {
            pw3_retries--;
        }
    }

    if (verified) {
        return apdu_sw(resp, SW_OK);
    }

    uint8_t retries = (pw_ref == 0x81 || pw_ref == 0x82) ? pw1_retries : pw3_retries;
    if (retries == 0) {
        return apdu_sw(resp, SW_AUTH_METHOD_BLOCKED);
    }
    return apdu_sw(resp, 0x63C0 | retries);
}

// Process PSO (Perform Security Operation) - Compute Digital Signature
static int cmd_pso_cds(const apdu_t *apdu, uint8_t *resp, size_t resp_max) {
    if (!pw1_verified) {
        return apdu_sw(resp, SW_SECURITY_NOT_SATISFIED);
    }

    // Check if GPG key is configured
    if (!gpg_is_initialized()) {
        return apdu_sw(resp, SW_CONDITIONS_NOT_SATISFIED);
    }

    // Sign hash using TROPIC01 via GPG component
    uint8_t signature[64];  // Max for Ed25519/P-256
    size_t sig_len = sizeof(signature);

    if (!gpg_sign_hash(apdu->data, apdu->lc, signature, &sig_len)) {
        ESP_LOGE(TAG, "Signature failed");
        return apdu_sw(resp, SW_UNKNOWN);
    }

    // Increment signature counter
    sig_count++;
    save_state_to_nvs();

    ESP_LOGI(TAG, "Signature created, count=%lu", sig_count);
    return apdu_build_response(resp, resp_max, signature, sig_len, SW_OK);
}

// Process GENERATE ASYMMETRIC KEY PAIR
static int cmd_generate_keypair(const apdu_t *apdu, uint8_t *resp, size_t resp_max) {
    if (apdu->p1 == 0x80) {
        // Generate new key
        if (!pw3_verified) {
            return apdu_sw(resp, SW_SECURITY_NOT_SATISFIED);
        }

        // Determine key slot from data
        if (apdu->lc < 2) {
            return apdu_sw(resp, SW_WRONG_LENGTH);
        }

        uint8_t key_ref = apdu->data[0];
        (void)key_ref;  // For future use
        uint8_t curve = CDC_CURVE_ED25519;  // Default to Ed25519

        // Generate key using TROPIC01 via GPG component
        if (!gpg_generate_key(curve)) {
            ESP_LOGE(TAG, "Key generation failed");
            return apdu_sw(resp, SW_UNKNOWN);
        }

        ESP_LOGI(TAG, "Key pair generated");
    }

    // Read public key (P1=0x81 or after generation)
    if (!gpg_is_initialized()) {
        return apdu_sw(resp, SW_CONDITIONS_NOT_SATISFIED);
    }

    // Export public key
    char pubkey_pem[512];
    size_t pubkey_len = sizeof(pubkey_pem);
    if (!gpg_export_pubkey_pem(pubkey_pem, pubkey_len, &pubkey_len)) {
        return apdu_sw(resp, SW_UNKNOWN);
    }

    // Build response with public key
    // Format: 7F49 <len> 86 <len> <pubkey>
    // For simplicity, return raw public key for now
    // TODO: Proper TLV encoding
    return apdu_build_response(resp, resp_max,
                               (const uint8_t *)pubkey_pem, pubkey_len, SW_OK);
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

        case INS_PSO:
            if (apdu.p1 == 0x9E && apdu.p2 == 0x9A) {
                return cmd_pso_cds(&apdu, resp, resp_max);
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
            secure_random_fill(challenge, len);
            return apdu_build_response(resp, resp_max, challenge, len, SW_OK);
        }

        default:
            ESP_LOGW(TAG, "Unknown instruction: 0x%02X", apdu.ins);
            return apdu_sw(resp, SW_INS_NOT_SUPPORTED);
    }
}
