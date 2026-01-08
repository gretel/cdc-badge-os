// CTAP2 Protocol Implementation (FIDO2)
// Handles CBOR-encoded CTAP2 commands

#ifndef CTAP2_H
#define CTAP2_H

#include <stdint.h>
#include <stdbool.h>
#include "fido2.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// CTAP2 Commands
// ============================================================================

#define CTAP2_CMD_MAKE_CREDENTIAL       0x01
#define CTAP2_CMD_GET_ASSERTION         0x02
#define CTAP2_CMD_GET_INFO              0x04
#define CTAP2_CMD_CLIENT_PIN            0x06
#define CTAP2_CMD_RESET                 0x07
#define CTAP2_CMD_GET_NEXT_ASSERTION    0x08
#define CTAP2_CMD_CRED_MANAGEMENT       0x0A
#define CTAP2_CMD_SELECTION             0x0B
#define CTAP2_CMD_LARGE_BLOBS           0x0C
#define CTAP2_CMD_CONFIG                0x0D

// Vendor commands
#define CTAP2_CMD_VENDOR_FIRST          0x40
#define CTAP2_CMD_VENDOR_LAST           0xBF

// ============================================================================
// CTAP2 Status Codes
// ============================================================================

#define CTAP2_OK                        0x00
#define CTAP1_ERR_INVALID_COMMAND       0x01
#define CTAP1_ERR_INVALID_PARAMETER     0x02
#define CTAP1_ERR_INVALID_LENGTH        0x03
#define CTAP1_ERR_INVALID_SEQ           0x04
#define CTAP1_ERR_TIMEOUT               0x05
#define CTAP1_ERR_CHANNEL_BUSY          0x06
#define CTAP1_ERR_LOCK_REQUIRED         0x0A
#define CTAP1_ERR_INVALID_CHANNEL       0x0B
#define CTAP2_ERR_CBOR_UNEXPECTED_TYPE  0x11
#define CTAP2_ERR_INVALID_CBOR          0x12
#define CTAP2_ERR_MISSING_PARAMETER     0x14
#define CTAP2_ERR_LIMIT_EXCEEDED        0x15
#define CTAP2_ERR_UNSUPPORTED_EXT       0x16
#define CTAP2_ERR_CREDENTIAL_EXCLUDED   0x19
#define CTAP2_ERR_PROCESSING            0x21
#define CTAP2_ERR_INVALID_CREDENTIAL    0x22
#define CTAP2_ERR_USER_ACTION_PENDING   0x23
#define CTAP2_ERR_OPERATION_PENDING     0x24
#define CTAP2_ERR_NO_OPERATIONS         0x25
#define CTAP2_ERR_UNSUPPORTED_ALGORITHM 0x26
#define CTAP2_ERR_OPERATION_DENIED      0x27
#define CTAP2_ERR_KEY_STORE_FULL        0x28
#define CTAP2_ERR_NO_OPERATION_PENDING  0x2A
#define CTAP2_ERR_UNSUPPORTED_OPTION    0x2B
#define CTAP2_ERR_INVALID_OPTION        0x2C
#define CTAP2_ERR_KEEPALIVE_CANCEL      0x2D
#define CTAP2_ERR_NO_CREDENTIALS        0x2E
#define CTAP2_ERR_USER_ACTION_TIMEOUT   0x2F
#define CTAP2_ERR_NOT_ALLOWED           0x30
#define CTAP2_ERR_PIN_INVALID           0x31
#define CTAP2_ERR_PIN_BLOCKED           0x32
#define CTAP2_ERR_PIN_AUTH_INVALID      0x33
#define CTAP2_ERR_PIN_AUTH_BLOCKED      0x34
#define CTAP2_ERR_PIN_NOT_SET           0x35
#define CTAP2_ERR_PIN_REQUIRED          0x36
#define CTAP2_ERR_PIN_POLICY_VIOLATION  0x37
#define CTAP2_ERR_PIN_TOKEN_EXPIRED     0x38
#define CTAP2_ERR_REQUEST_TOO_LARGE     0x39
#define CTAP2_ERR_ACTION_TIMEOUT        0x3A
#define CTAP2_ERR_UP_REQUIRED           0x3B
#define CTAP2_ERR_UV_BLOCKED            0x3C
#define CTAP2_ERR_OTHER                 0x7F

// ============================================================================
// CTAP2 Algorithms
// ============================================================================

#define COSE_ALG_ES256  -7     // ECDSA with SHA-256 (P-256)
#define COSE_ALG_EDDSA  -8     // EdDSA (Ed25519)
#define COSE_ALG_RS256  -257   // RSASSA-PKCS1-v1_5 with SHA-256

// ============================================================================
// Processing Functions
// ============================================================================

/**
 * Initialize CTAP2 protocol handler.
 *
 * @return true on success
 */
bool ctap2_init(void);

/**
 * Process a CTAP2 command.
 *
 * @param cmd Command buffer (first byte is command code)
 * @param cmd_len Command length
 * @param response Output buffer for response
 * @param response_len Input: max size, Output: actual size
 * @return CTAP2 status code (first byte of response)
 */
uint8_t ctap2_process_command(const uint8_t *cmd, uint16_t cmd_len,
                               uint8_t *response, uint16_t *response_len);

/**
 * Send keepalive during long operations.
 * Called periodically to prevent timeout.
 */
void ctap2_send_keepalive(uint8_t status);

/**
 * Cancel any pending operation.
 */
void ctap2_cancel(void);

// ============================================================================
// Individual Command Handlers
// ============================================================================

uint8_t ctap2_make_credential(const uint8_t *params, uint16_t params_len,
                               uint8_t *response, uint16_t *response_len);

uint8_t ctap2_get_assertion(const uint8_t *params, uint16_t params_len,
                             uint8_t *response, uint16_t *response_len);

uint8_t ctap2_get_info(uint8_t *response, uint16_t *response_len);

uint8_t ctap2_client_pin(const uint8_t *params, uint16_t params_len,
                          uint8_t *response, uint16_t *response_len);

uint8_t ctap2_reset(uint8_t *response, uint16_t *response_len);

uint8_t ctap2_get_next_assertion(uint8_t *response, uint16_t *response_len);

uint8_t ctap2_cred_management(const uint8_t *params, uint16_t params_len,
                               uint8_t *response, uint16_t *response_len);

uint8_t ctap2_selection(uint8_t *response, uint16_t *response_len);

#ifdef __cplusplus
}
#endif

#endif // CTAP2_H
