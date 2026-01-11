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

// OpenPGP Application ID (AID)
extern const uint8_t OPENPGP_AID[];
extern const uint8_t OPENPGP_AID_LEN;

// ATR (Answer To Reset) for OpenPGP card
extern const uint8_t OPENPGP_ATR[];
extern const uint8_t OPENPGP_ATR_LEN;

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
#define DO_FP_SIG       0x00C7  // Fingerprint Signature key
#define DO_FP_DEC       0x00C8  // Fingerprint Decryption key
#define DO_FP_AUT       0x00C9  // Fingerprint Authentication key
#define DO_PW_STATUS    0x00C4  // PW Status Bytes
#define DO_SIG_COUNT    0x0093  // Digital Signature Counter

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

#ifdef __cplusplus
}
#endif

#endif // OPENPGP_H
