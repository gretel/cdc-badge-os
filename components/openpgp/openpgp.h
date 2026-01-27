/*
 * OpenPGP SmartCard Application for CDC Badge
 *
 * Based on pico-openpgp (https://github.com/polhenarejos/pico-openpgp)
 * Original: Copyright (c) 2022 Pol Henarejos, AGPLv3
 * Adapted for CDC Badge with TROPIC01 Secure Element
 *
 * This implementation follows OpenPGP 3.4.1 specification:
 * https://gnupg.org/ftp/specs/OpenPGP-smart-card-application-3.4.pdf
 */

#ifndef OPENPGP_H
#define OPENPGP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// OpenPGP Application ID (AID) - initialized dynamically in openpgp_init()
extern const uint8_t* OPENPGP_AID;
extern const uint8_t OPENPGP_AID_LEN;

// NOTE: ATR is defined in ccid.cpp and accessed via ccid_get_atr()
// Do not use OPENPGP_ATR - use ccid_get_atr() instead!

// Algorithm identifiers
#define ALGO_RSA        0x01
#define ALGO_ECDH       0x12
#define ALGO_ECDSA      0x13
#define ALGO_EDDSA      0x16  // Ed25519

// Key slots
#define KEY_SIG         0xB6  // Signature key
#define KEY_DEC         0xB8  // Decryption key
#define KEY_AUT         0xA4  // Authentication key

// Data Object tags (selected)
#define DO_AID              0x004F  // Application Identifier
#define DO_HIST_BYTES       0x5F52  // Historical bytes
#define DO_CARDHOLDER       0x0065  // Cardholder Related Data
#define DO_APP_RELATED      0x006E  // Application Related Data
#define DO_DISCRET_DO       0x0073  // Discretionary Data Objects
#define DO_EXT_CAP          0x00C0  // Extended Capabilities
#define DO_ALGO_SIG         0x00C1  // Algorithm Attributes: Signature
#define DO_ALGO_DEC         0x00C2  // Algorithm Attributes: Decryption
#define DO_ALGO_AUT         0x00C3  // Algorithm Attributes: Authentication
#define DO_PW_STATUS        0x00C4  // PW Status Bytes
#define DO_FP_SIG           0x00C7  // Fingerprint Signature key
#define DO_FP_DEC           0x00C8  // Fingerprint Decryption key
#define DO_FP_AUT           0x00C9  // Fingerprint Authentication key
#define DO_CA_FP_1          0x00CA  // CA Fingerprint 1
#define DO_CA_FP_2          0x00CB  // CA Fingerprint 2
#define DO_CA_FP_3          0x00CC  // CA Fingerprint 3
#define DO_GEN_TIME_SIG     0x00CD  // Generation time: Signature
#define DO_GEN_TIME_DEC     0x00CE  // Generation time: Decryption
#define DO_GEN_TIME_AUT     0x00CF  // Generation time: Authentication
#define DO_SIG_COUNT        0x0093  // Digital Signature Counter
#define DO_URL              0x5F50  // URL for public key retrieval
#define DO_LOGIN            0x005E  // Login data
#define DO_NAME             0x005B  // Name (Cardholder)
#define DO_LANG_PREF        0x5F2D  // Language preference
#define DO_SEX              0x5F35  // Sex
#define DO_UIF_SIG          0x00D6  // User Interaction Flag: Signature
#define DO_UIF_DEC          0x00D7  // User Interaction Flag: Decryption
#define DO_UIF_AUT          0x00D8  // User Interaction Flag: Authentication
#define DO_KEY_INFO         0x00DE  // Key Information
#define DO_SEC_TPL          0x007A  // Security Support Template
#define DO_KDF              0x00F9  // KDF (Key Derivation Function)

// Status Words (SW1-SW2)
#define SW_OK                           0x9000
#define SW_WRONG_LENGTH                 0x6700
#define SW_SECURITY_NOT_SATISFIED       0x6982
#define SW_AUTH_METHOD_BLOCKED          0x6983
#define SW_CONDITIONS_NOT_SATISFIED     0x6985
#define SW_WRONG_DATA                   0x6A80
#define SW_FILE_NOT_FOUND               0x6A82
#define SW_INCORRECT_P1P2               0x6A86
#define SW_WRONG_P1P2                   0x6B00
#define SW_INS_NOT_SUPPORTED            0x6D00
#define SW_CLA_NOT_SUPPORTED            0x6E00
#define SW_UNKNOWN                      0x6F00

// Initialize OpenPGP application
bool openpgp_init(void);

// Process incoming APDU command
// Returns response length (including SW1-SW2)
int openpgp_process_apdu(const uint8_t *cmd, size_t cmd_len,
                         uint8_t *resp, size_t resp_max);

// Check if OpenPGP application is selected
bool openpgp_is_selected(void);

// Get current signature count
uint32_t openpgp_get_sig_count(void);

// Update fingerprint for a key type (call after key generation via serial cmd)
// key_type: KEY_SIG (0xB6), KEY_DEC (0xB8), KEY_AUT (0xA4)
// fingerprint: 20-byte SHA-1 fingerprint (V4 format)
// gen_time: Unix timestamp of key generation (big-endian)
bool openpgp_set_key_fingerprint(uint8_t key_type, const uint8_t *fingerprint,
                                  uint32_t gen_time);

#ifdef __cplusplus
}
#endif

#endif // OPENPGP_H
