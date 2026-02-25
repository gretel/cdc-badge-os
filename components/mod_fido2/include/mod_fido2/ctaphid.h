#pragma once

#include <cstdint>
#include <cstddef>

// CTAPHID Transport Layer
// Handles USB HID framing for CTAP2 messages

#ifdef __cplusplus
extern "C" {
#endif

#define CTAPHID_PACKET_SIZE      64
#define CTAPHID_INIT_DATA        57
#define CTAPHID_CONT_DATA        59
#define CTAPHID_MAX_MSG_SIZE     2048

#define CTAPHID_BROADCAST_CID    0xFFFFFFFF

// CTAPHID Commands
#define CTAPHID_PING        0x01
#define CTAPHID_MSG         0x03
#define CTAPHID_LOCK        0x04
#define CTAPHID_INIT        0x06
#define CTAPHID_WINK        0x08
#define CTAPHID_CBOR        0x10
#define CTAPHID_CANCEL      0x11
#define CTAPHID_KEEPALIVE   0x3B
#define CTAPHID_ERROR       0x3F

// CTAPHID Errors
#define CTAPHID_ERR_INVALID_CMD     0x01
#define CTAPHID_ERR_INVALID_PAR     0x02
#define CTAPHID_ERR_INVALID_LEN     0x03
#define CTAPHID_ERR_INVALID_SEQ     0x04
#define CTAPHID_ERR_MSG_TIMEOUT     0x05
#define CTAPHID_ERR_CHANNEL_BUSY    0x06
#define CTAPHID_ERR_LOCK_REQUIRED   0x0A
#define CTAPHID_ERR_INVALID_CHANNEL 0x0B
#define CTAPHID_ERR_OTHER           0x7F

// Vendor command range
#define CTAPHID_VENDOR_FIRST    0x40
#define CTAPHID_VENDOR_LAST     0x7F

// CTAPHID Status
#define CTAPHID_STATUS_PROCESSING  0x01
#define CTAPHID_STATUS_UPNEEDED    0x02

// Capabilities
#define CTAPHID_CAP_WINK   0x01
#define CTAPHID_CAP_CBOR   0x04

#ifdef __DOXYGEN__
namespace cdc::mod_fido2 {
#endif

typedef struct {
    uint32_t cid;
    uint8_t cmd;
    uint16_t bcnt;
    uint8_t seq;
    uint16_t offset;
    uint8_t* buffer;
    uint16_t buffer_size;
    bool active;
    uint32_t last_activity;
} ctaphid_channel_t;

#ifdef __DOXYGEN__
} // namespace cdc::mod_fido2
#endif

bool ctaphid_init(void);
bool ctaphid_process_packet(const uint8_t* packet);
bool ctaphid_has_response(void);
bool ctaphid_get_response_packet(uint8_t* packet);
void ctaphid_send_keepalive(uint32_t cid, uint8_t status);
void ctaphid_send_error(uint32_t cid, uint8_t error);
void ctaphid_check_timeout(void);
uint32_t ctaphid_get_current_cid(void);
bool ctaphid_is_busy(void);

void ctaphid_get_cmd_counts(uint32_t* cbor_count, uint32_t* msg_count);
void ctaphid_reset_cmd_counts(void);

#ifdef __cplusplus
}
#endif
