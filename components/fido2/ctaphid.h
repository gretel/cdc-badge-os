// CTAPHID Transport Layer
// Handles USB HID framing for CTAP2 messages

#ifndef CTAPHID_H
#define CTAPHID_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// CTAPHID Constants
// ============================================================================

#define CTAPHID_PACKET_SIZE     64      // HID report size
#define CTAPHID_INIT_HEADER     7       // CID(4) + CMD(1) + BCNT(2)
#define CTAPHID_CONT_HEADER     5       // CID(4) + SEQ(1)
#define CTAPHID_INIT_DATA       (CTAPHID_PACKET_SIZE - CTAPHID_INIT_HEADER)  // 57
#define CTAPHID_CONT_DATA       (CTAPHID_PACKET_SIZE - CTAPHID_CONT_HEADER)  // 59
#define CTAPHID_MAX_MSG_SIZE    7609    // Max message size

#define CTAPHID_BROADCAST_CID   0xFFFFFFFF

// ============================================================================
// CTAPHID Commands
// ============================================================================

#define CTAPHID_MSG             0x03    // CTAP message
#define CTAPHID_CBOR            0x10    // CTAP2 CBOR message
#define CTAPHID_INIT            0x06    // Channel initialization
#define CTAPHID_PING            0x01    // Echo test
#define CTAPHID_CANCEL          0x11    // Cancel current operation
#define CTAPHID_ERROR           0x3F    // Error response
#define CTAPHID_KEEPALIVE       0x3B    // Keepalive during operation
#define CTAPHID_WINK            0x08    // Device identification
#define CTAPHID_LOCK            0x04    // Lock channel (optional)

// Vendor commands
#define CTAPHID_VENDOR_FIRST    0x40
#define CTAPHID_VENDOR_LAST     0x7F

// ============================================================================
// CTAPHID Error Codes
// ============================================================================

#define CTAPHID_ERR_INVALID_CMD     0x01
#define CTAPHID_ERR_INVALID_PAR     0x02
#define CTAPHID_ERR_INVALID_LEN     0x03
#define CTAPHID_ERR_INVALID_SEQ     0x04
#define CTAPHID_ERR_MSG_TIMEOUT     0x05
#define CTAPHID_ERR_CHANNEL_BUSY    0x06
#define CTAPHID_ERR_LOCK_REQUIRED   0x0A
#define CTAPHID_ERR_INVALID_CHANNEL 0x0B
#define CTAPHID_ERR_OTHER           0x7F

// ============================================================================
// Keepalive Status
// ============================================================================

#define CTAPHID_STATUS_PROCESSING   0x01    // Still processing
#define CTAPHID_STATUS_UPNEEDED     0x02    // User presence needed

// ============================================================================
// CTAPHID Capabilities (for INIT response)
// ============================================================================

#define CTAPHID_CAP_WINK        0x01    // Supports WINK command
#define CTAPHID_CAP_CBOR        0x04    // Supports CBOR/CTAP2
#define CTAPHID_CAP_NMSG        0x08    // Doesn't support MSG (CTAP1/U2F)

// ============================================================================
// Types
// ============================================================================

typedef struct {
    uint32_t cid;           // Channel ID
    uint8_t cmd;            // Command
    uint16_t bcnt;          // Total byte count
    uint8_t seq;            // Current sequence number
    uint16_t offset;        // Current offset in buffer
    uint8_t *buffer;        // Message buffer
    uint16_t buffer_size;   // Buffer size
    bool active;            // Transaction active
    uint32_t last_activity; // Last packet timestamp (ms)
} ctaphid_channel_t;

// ============================================================================
// Functions
// ============================================================================

/**
 * Initialize CTAPHID layer.
 *
 * @return true on success
 */
bool ctaphid_init(void);

/**
 * Process incoming HID packet.
 *
 * @param packet 64-byte HID report
 * @return true if packet was processed
 */
bool ctaphid_process_packet(const uint8_t *packet);

/**
 * Check if response is ready to send.
 *
 * @return true if response pending
 */
bool ctaphid_has_response(void);

/**
 * Get next response packet.
 *
 * @param packet Output: 64-byte HID report
 * @return true if packet available
 */
bool ctaphid_get_response_packet(uint8_t *packet);

/**
 * Send keepalive packet.
 *
 * @param cid Channel ID
 * @param status Keepalive status code
 */
void ctaphid_send_keepalive(uint32_t cid, uint8_t status);

/**
 * Send error response.
 *
 * @param cid Channel ID
 * @param error Error code
 */
void ctaphid_send_error(uint32_t cid, uint8_t error);

/**
 * Handle timeout check (call periodically).
 * Cleans up stale transactions.
 */
void ctaphid_check_timeout(void);

/**
 * Get current channel ID for keepalive.
 */
uint32_t ctaphid_get_current_cid(void);

/**
 * Check if busy processing a command.
 */
bool ctaphid_is_busy(void);

/**
 * Get counts of processed CTAPHID commands.
 *
 * @param cbor_count Output: number of CTAPHID_CBOR commands
 * @param msg_count Output: number of CTAPHID_MSG commands (U2F)
 */
void ctaphid_get_cmd_counts(uint32_t *cbor_count, uint32_t *msg_count);

/**
 * Reset CTAPHID command counters.
 */
void ctaphid_reset_cmd_counts(void);

#ifdef __cplusplus
}
#endif

#endif // CTAPHID_H
