/**
 * \file
 * \brief CTAPHID transport layer for USB HID framed CTAP2/U2F traffic.
 */

#include "mod_fido2/ctaphid.h"
#include "mod_fido2/ctap2.h"
#include "mod_fido2/u2f.h"
#include "cdc_log.h"
#include "cdc_core/feature_flags.h"
#include <esp_attr.h>

namespace cdc::mod_fido2 {
    // USB transport callback implemented in Fido2Module.cpp.
    bool fido2_usb_write(const uint8_t* buffer);
}
using cdc::mod_fido2::fido2_usb_write;
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

static const char* TAG = "CTAPHID";

/** \brief Enable verbose packet-level debug logging when feature flags allow it. */

#ifndef CTAPHID_DEBUG_PACKETS
#define CTAPHID_DEBUG_PACKETS       DEBUG_MODE  // Controlled by feature_flags.h
#endif
#define CTAPHID_MSG_TIMEOUT_MS      500     // Message assembly timeout
#define CTAPHID_MAX_CHANNELS        8       // Max concurrent channels
#define CTAPHID_RESPONSE_QUEUE_SIZE 8       // Response packet queue

/** \brief Runtime state and channel bookkeeping for CTAPHID transport. */

/** \brief Per-window command rate limiting configuration. */
#define CTAPHID_RATE_LIMIT_WINDOW_MS  1000
#define CTAPHID_RATE_LIMIT_MAX_CMDS   200

/** \brief Message and response buffers located in PSRAM to save internal RAM. */
EXT_RAM_BSS_ATTR static uint8_t s_msg_buffers[CTAPHID_MAX_CHANNELS][CTAPHID_MAX_MSG_SIZE];
EXT_RAM_BSS_ATTR static uint8_t s_response_buffer[CTAPHID_MAX_MSG_SIZE];

static struct {
    bool initialized;
    uint32_t next_cid;                      // Next channel ID to allocate
    ctaphid_channel_t channels[CTAPHID_MAX_CHANNELS];
    uint8_t init_buffer[64];                // Small buffer for INIT command only
    uint16_t response_len;
    uint16_t response_offset;
    uint32_t response_cid;
    uint8_t response_cmd;
    bool response_pending;
    SemaphoreHandle_t mutex;
    uint32_t current_cid;                   // CID of active transaction
    // Rate limiting state
    uint32_t rate_window_start;             // Start of current rate window (ms)
    uint8_t rate_cmd_count;                 // Commands in current window
    // Command counters
    uint32_t cbor_cmd_count;                // CTAPHID_CBOR commands
    uint32_t msg_cmd_count;                 // CTAPHID_MSG (U2F) commands
} g_ctaphid = {};

/**
 * \brief Returns the channel record for a given channel identifier.
 * \param cid CTAPHID channel identifier.
 * \return Pointer to the channel entry, or `NULL` when not found.
 */
static ctaphid_channel_t *find_channel(uint32_t cid) {
    for (int i = 0; i < CTAPHID_MAX_CHANNELS; i++) {
        if (g_ctaphid.channels[i].cid == cid) {
            return &g_ctaphid.channels[i];
        }
    }
    return NULL;
}

/**
 * \brief Allocates or reuses a channel slot for the provided channel identifier.
 * \param cid CTAPHID channel identifier to allocate.
 * \return Pointer to the allocated channel entry, or `NULL` when no slot is available.
 */
static ctaphid_channel_t *alloc_channel(uint32_t cid) {
    // First check if already exists
    ctaphid_channel_t *ch = find_channel(cid);
    if (ch) return ch;

    // Find free slot
    for (int i = 0; i < CTAPHID_MAX_CHANNELS; i++) {
        if (g_ctaphid.channels[i].cid == 0) {
            g_ctaphid.channels[i].cid = cid;
            g_ctaphid.channels[i].active = false;
            g_ctaphid.channels[i].buffer = s_msg_buffers[i];  // Per-channel buffer
            g_ctaphid.channels[i].buffer_size = CTAPHID_MAX_MSG_SIZE;
            return &g_ctaphid.channels[i];
        }
    }

    // All channels full - evict oldest inactive channel
    int oldest_idx = -1;
    uint32_t oldest_time = UINT32_MAX;
    for (int i = 0; i < CTAPHID_MAX_CHANNELS; i++) {
        if (!g_ctaphid.channels[i].active &&
            g_ctaphid.channels[i].last_activity < oldest_time) {
            oldest_time = g_ctaphid.channels[i].last_activity;
            oldest_idx = i;
        }
    }

    if (oldest_idx >= 0) {
        LOG_W(TAG, "Evicting oldest channel 0x%08lX", g_ctaphid.channels[oldest_idx].cid);
        g_ctaphid.channels[oldest_idx].cid = cid;
        g_ctaphid.channels[oldest_idx].active = false;
        g_ctaphid.channels[oldest_idx].buffer = s_msg_buffers[oldest_idx];
        g_ctaphid.channels[oldest_idx].buffer_size = CTAPHID_MAX_MSG_SIZE;
        return &g_ctaphid.channels[oldest_idx];
    }

    return NULL;  // All channels actively in use
}

/**
 * \brief Allocates the next non-broadcast CTAPHID channel identifier.
 * \return Newly allocated channel identifier.
 */
static uint32_t allocate_cid(void) {
    // Start from 1, avoid broadcast CID
    if (g_ctaphid.next_cid == 0 || g_ctaphid.next_cid == CTAPHID_BROADCAST_CID) {
        g_ctaphid.next_cid = 1;
    }
    return g_ctaphid.next_cid++;
}

/**
 * \brief Builds a CTAPHID initialization packet.
 * \param packet Destination buffer for the 64-byte HID packet.
 * \param cid CTAPHID channel identifier.
 * \param cmd CTAPHID command byte without the init bit.
 * \param bcnt Total message byte count.
 * \param data Optional payload pointer.
 * \param data_len Number of payload bytes available in `data`.
 */
static void build_init_packet(uint8_t *packet, uint32_t cid, uint8_t cmd,
                               uint16_t bcnt, const uint8_t *data, uint16_t data_len) {
    memset(packet, 0, CTAPHID_PACKET_SIZE);

    // Header
    packet[0] = (cid >> 24) & 0xFF;
    packet[1] = (cid >> 16) & 0xFF;
    packet[2] = (cid >> 8) & 0xFF;
    packet[3] = cid & 0xFF;
    packet[4] = cmd | 0x80;  // Init packet has bit 7 set
    packet[5] = (bcnt >> 8) & 0xFF;
    packet[6] = bcnt & 0xFF;

    // Data
    if (data && data_len > 0) {
        uint16_t copy = (data_len > CTAPHID_INIT_DATA) ? CTAPHID_INIT_DATA : data_len;
        memcpy(packet + 7, data, copy);
    }
}

/**
 * \brief Builds a CTAPHID continuation packet.
 * \param packet Destination buffer for the 64-byte HID packet.
 * \param cid CTAPHID channel identifier.
 * \param seq Continuation sequence number.
 * \param data Optional payload pointer.
 * \param data_len Number of payload bytes available in `data`.
 */
static void build_cont_packet(uint8_t *packet, uint32_t cid, uint8_t seq,
                               const uint8_t *data, uint16_t data_len) {
    memset(packet, 0, CTAPHID_PACKET_SIZE);

    // Header
    packet[0] = (cid >> 24) & 0xFF;
    packet[1] = (cid >> 16) & 0xFF;
    packet[2] = (cid >> 8) & 0xFF;
    packet[3] = cid & 0xFF;
    packet[4] = seq & 0x7F;  // Continuation packet has bit 7 clear

    // Data
    if (data && data_len > 0) {
        uint16_t copy = (data_len > CTAPHID_CONT_DATA) ? CTAPHID_CONT_DATA : data_len;
        memcpy(packet + 5, data, copy);
    }
}

/**
 * \brief Stores a command response so it can be packetized and read out later.
 * \param cid Channel identifier for the response.
 * \param cmd CTAPHID response command.
 * \param data Optional response payload pointer.
 * \param len Number of payload bytes.
 */
static void prepare_response(uint32_t cid, uint8_t cmd, const uint8_t *data, uint16_t len) {
    g_ctaphid.response_cid = cid;
    g_ctaphid.response_cmd = cmd;
    if (data && len > 0) {
        // Bounds check to prevent buffer overflow
        if (len > CTAPHID_MAX_MSG_SIZE) {
            LOG_E(TAG, "Response too large: %u > %u", len, CTAPHID_MAX_MSG_SIZE);
            len = CTAPHID_MAX_MSG_SIZE;
        }
        memcpy(s_response_buffer, data, len);
    }
    g_ctaphid.response_len = len;
    g_ctaphid.response_offset = 0;
    g_ctaphid.response_pending = true;
    if (cmd == CTAPHID_CBOR && len > 0) {
        LOG_I(TAG, "Prepared CBOR response len=%u status=0x%02X", len, data[0]);
    }
}

/**
 * \brief Handles CTAPHID INIT and returns negotiated channel/capability data.
 * \param cid Source channel identifier from the request.
 * \param data INIT request payload.
 * \param len Length of `data` in bytes.
 */
static void handle_init(uint32_t cid, const uint8_t *data, uint16_t len) {
    if (len < 8) {
        ctaphid_send_error(cid, CTAPHID_ERR_INVALID_LEN);
        return;
    }

    // A new channel is a clean slate: any pending cancel from a previous
    // transaction must not abort this INIT response or the channel's first
    // CBOR exchange.
    ctap2_clear_cancel();

    // Allocate new channel
    uint32_t new_cid = (cid == CTAPHID_BROADCAST_CID) ? allocate_cid() : cid;

    ctaphid_channel_t *ch = alloc_channel(new_cid);
    if (!ch) {
        ctaphid_send_error(cid, CTAPHID_ERR_OTHER);
        return;
    }

    // Build INIT response (17 bytes)
    uint8_t response[17];
    memcpy(response, data, 8);          // Echo nonce
    response[8] = (new_cid >> 24) & 0xFF;
    response[9] = (new_cid >> 16) & 0xFF;
    response[10] = (new_cid >> 8) & 0xFF;
    response[11] = new_cid & 0xFF;
    response[12] = 2;                   // Protocol version
    response[13] = 0;                   // Device version major
    response[14] = 1;                   // Device version minor
    response[15] = 0;                   // Device version build
    response[16] = CTAPHID_CAP_WINK | CTAPHID_CAP_CBOR;

    prepare_response(cid, CTAPHID_INIT, response, 17);
    if (CTAPHID_DEBUG_PACKETS) LOG_D(TAG, "INIT: allocated CID 0x%08lX", new_cid);
}

/**
 * \brief Handles CTAPHID PING by echoing the request payload.
 * \param cid Request channel identifier.
 * \param data PING payload bytes.
 * \param len Length of `data` in bytes.
 */
static void handle_ping(uint32_t cid, const uint8_t *data, uint16_t len) {
    // Echo back the data
    prepare_response(cid, CTAPHID_PING, data, len);
    if (CTAPHID_DEBUG_PACKETS) LOG_D(TAG, "PING: echoing %d bytes", len);
}

/**
 * \brief Handles CTAPHID WINK requests.
 * \param cid Request channel identifier.
 */
static void handle_wink(uint32_t cid) {
    // TODO: Trigger visual identification feedback for the user (CTAPHID WINK).
    // This handler runs in the FIDO2 transport task, so direct display rendering
    // from here is not thread-safe. When implementing, dispatch a short-lived
    // event to the UI task (for example via EventBus or a ToastView request) so
    // the UI thread can show a "WINK" toast or blink an available indicator.
    prepare_response(cid, CTAPHID_WINK, NULL, 0);
    if (CTAPHID_DEBUG_PACKETS) LOG_D(TAG, "WINK");
}

/**
 * \brief Handles CTAPHID CANCEL requests and aborts active CTAP2 work.
 * \param cid Request channel identifier.
 */
static void handle_cancel(uint32_t cid) {
    ctap2_cancel();
    // No response for CANCEL
    if (CTAPHID_DEBUG_PACKETS) LOG_D(TAG, "CANCEL");
}

/**
 * \brief Handles CTAPHID CBOR requests by dispatching to the CTAP2 command processor.
 * \param cid Request channel identifier.
 * \param data CBOR command payload.
 * \param len Length of `data` in bytes.
 */
static void handle_cbor(uint32_t cid, const uint8_t *data, uint16_t len) {
    if (len < 1) {
        ctaphid_send_error(cid, CTAPHID_ERR_INVALID_LEN);
        return;
    }

    g_ctaphid.current_cid = cid;

    // Process CTAP2 command
    uint16_t response_len = sizeof(s_response_buffer);
    uint8_t status = ctap2_process_command(data, len,
                                            s_response_buffer,
                                            &response_len);

    // Prepare response (status byte + data)
    // Note: ctap2_process_command already includes status in response_buffer[0]
    prepare_response(cid, CTAPHID_CBOR, s_response_buffer, response_len);
    if (CTAPHID_DEBUG_PACKETS) LOG_D(TAG, "CBOR: cmd=0x%02X status=0x%02X len=%d", data[0], status, response_len);
}

/**
 * \brief Dispatches a fully assembled channel message to its command handler.
 * \param ch Channel state containing command and assembled payload.
 */
static void process_complete_message(ctaphid_channel_t *ch) {
    uint32_t cid = ch->cid;
    uint8_t cmd = ch->cmd;
    uint8_t *data = ch->buffer;
    uint16_t len = ch->bcnt;

    if (CTAPHID_DEBUG_PACKETS) LOG_D(TAG, "Processing cmd=0x%02X len=%d", cmd, len);

    switch (cmd) {
        case CTAPHID_INIT:
            handle_init(cid, data, len);
            break;
        case CTAPHID_PING:
            handle_ping(cid, data, len);
            break;
        case CTAPHID_WINK:
            handle_wink(cid);
            break;
        case CTAPHID_CANCEL:
            handle_cancel(cid);
            break;
        case CTAPHID_CBOR:
            g_ctaphid.cbor_cmd_count++;
            handle_cbor(cid, data, len);
            break;
        case CTAPHID_MSG:
            // U2F/CTAP1 - process via U2F handler
            {
                g_ctaphid.msg_cmd_count++;
                g_ctaphid.current_cid = cid;
                // Use static PSRAM buffer for U2F response
                EXT_RAM_BSS_ATTR static uint8_t u2f_response[512];
                uint16_t u2f_response_len = u2f_process_apdu(data, len, u2f_response, sizeof(u2f_response));
                prepare_response(cid, CTAPHID_MSG, u2f_response, u2f_response_len);
                if (CTAPHID_DEBUG_PACKETS) LOG_D(TAG, "U2F: response len=%d", u2f_response_len);
            }
            break;
        default:
            if (cmd >= CTAPHID_VENDOR_FIRST && cmd <= CTAPHID_VENDOR_LAST) {
                // Vendor command not supported
                ctaphid_send_error(cid, CTAPHID_ERR_INVALID_CMD);
            } else {
                ctaphid_send_error(cid, CTAPHID_ERR_INVALID_CMD);
            }
            break;
    }

    // Clear channel state
    ch->active = false;
    ch->offset = 0;
}

/**
 * \brief Initializes CTAPHID transport state and synchronization primitives.
 * \return `true` on success, otherwise `false`.
 */
bool ctaphid_init(void) {
    LOG_I(TAG, "Initializing...");

    memset(&g_ctaphid, 0, sizeof(g_ctaphid));
    g_ctaphid.next_cid = 1;

    g_ctaphid.mutex = xSemaphoreCreateMutex();
    if (!g_ctaphid.mutex) {
        LOG_E(TAG, "Mutex creation failed");
        return false;
    }

    g_ctaphid.initialized = true;
    LOG_I(TAG, "Initialized");
    return true;
}

/**
 * \brief Returns cumulative counters for CTAPHID CBOR and MSG commands.
 * \param cbor_count Optional destination for CBOR command count.
 * \param msg_count Optional destination for MSG/U2F command count.
 */
void ctaphid_get_cmd_counts(uint32_t *cbor_count, uint32_t *msg_count) {
    if (cbor_count) *cbor_count = g_ctaphid.cbor_cmd_count;
    if (msg_count) *msg_count = g_ctaphid.msg_cmd_count;
}

/**
 * \brief Resets CTAPHID command counters.
 */
void ctaphid_reset_cmd_counts(void) {
    g_ctaphid.cbor_cmd_count = 0;
    g_ctaphid.msg_cmd_count = 0;
}

/**
 * \brief Processes one incoming 64-byte CTAPHID packet.
 * \param packet HID packet buffer.
 * \return `true` when the packet is consumed, `false` on invalid preconditions.
 */
bool ctaphid_process_packet(const uint8_t *packet) {
    if (!g_ctaphid.initialized || !packet) return false;

    xSemaphoreTake(g_ctaphid.mutex, portMAX_DELAY);

    // Rate limiting check
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (now - g_ctaphid.rate_window_start >= CTAPHID_RATE_LIMIT_WINDOW_MS) {
        // New window
        g_ctaphid.rate_window_start = now;
        g_ctaphid.rate_cmd_count = 1;
    } else {
        g_ctaphid.rate_cmd_count++;
        if (g_ctaphid.rate_cmd_count > CTAPHID_RATE_LIMIT_MAX_CMDS) {
            LOG_W(TAG, "Rate limit exceeded (%d cmd/s)", g_ctaphid.rate_cmd_count);
            // Don't send error - just drop silently to avoid amplification
            xSemaphoreGive(g_ctaphid.mutex);
            return true;
        }
    }

    // Parse header
    uint32_t cid = ((uint32_t)packet[0] << 24) | ((uint32_t)packet[1] << 16) |
                   ((uint32_t)packet[2] << 8) | packet[3];
    bool is_init = (packet[4] & 0x80) != 0;

    if (is_init) {
        // Initialization packet
        uint8_t cmd = packet[4] & 0x7F;
        uint16_t bcnt = ((uint16_t)packet[5] << 8) | packet[6];

        if (CTAPHID_DEBUG_PACKETS) LOG_D(TAG, "Init packet: CID=0x%08lX CMD=0x%02X BCNT=%d", cid, cmd, bcnt);

        // Validate bcnt to prevent buffer overflow
        if (bcnt > CTAPHID_MAX_MSG_SIZE) {
            LOG_W(TAG, "bcnt exceeds max: %u > %u", bcnt, CTAPHID_MAX_MSG_SIZE);
            ctaphid_send_error(cid, CTAPHID_ERR_INVALID_LEN);
            xSemaphoreGive(g_ctaphid.mutex);
            return true;
        }

        // INIT command is special - handle on any CID
        if (cmd == CTAPHID_INIT) {
            ctaphid_channel_t temp = {
                .cid = cid,
                .cmd = cmd,
                .bcnt = bcnt,
                .seq = 0,
                .offset = 0,
                .buffer = g_ctaphid.init_buffer,  // Use dedicated init buffer
                .buffer_size = CTAPHID_INIT_DATA,
                .active = false,
                .last_activity = now,
            };
            memcpy(g_ctaphid.init_buffer, packet + 7, CTAPHID_INIT_DATA);
            process_complete_message(&temp);
            xSemaphoreGive(g_ctaphid.mutex);
            return true;
        }

        // Find or create channel
        ctaphid_channel_t *ch = find_channel(cid);
        if (!ch) {
            LOG_W(TAG, "Unknown CID 0x%08lX", cid);
            ctaphid_send_error(cid, CTAPHID_ERR_INVALID_CHANNEL);
            xSemaphoreGive(g_ctaphid.mutex);
            return true;
        }

        // Check if another transaction is active on this channel
        if (ch->active) {
            LOG_W(TAG, "Channel busy");
            ctaphid_send_error(cid, CTAPHID_ERR_CHANNEL_BUSY);
            xSemaphoreGive(g_ctaphid.mutex);
            return true;
        }

        // Start new transaction
        ch->cmd = cmd;
        ch->bcnt = bcnt;
        ch->seq = 0;
        ch->offset = 0;
        ch->active = true;
        ch->last_activity = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // Copy data
        uint16_t copy = (bcnt > CTAPHID_INIT_DATA) ? CTAPHID_INIT_DATA : bcnt;
        memcpy(ch->buffer, packet + 7, copy);
        ch->offset = copy;

        // If message complete, process it
        if (ch->offset >= ch->bcnt) {
            process_complete_message(ch);
        }
    } else {
        // Continuation packet
        uint8_t seq = packet[4] & 0x7F;

        ctaphid_channel_t *ch = find_channel(cid);
        if (!ch || !ch->active) {
            LOG_W(TAG, "Continuation for inactive channel");
            ctaphid_send_error(cid, CTAPHID_ERR_INVALID_CHANNEL);
            xSemaphoreGive(g_ctaphid.mutex);
            return true;
        }

        // Check sequence
        if (seq != ch->seq) {
            LOG_W(TAG, "Invalid sequence: got %d, expected %d", seq, ch->seq);
            ch->active = false;
            ctaphid_send_error(cid, CTAPHID_ERR_INVALID_SEQ);
            xSemaphoreGive(g_ctaphid.mutex);
            return true;
        }

        // Copy data
        uint16_t remaining = ch->bcnt - ch->offset;
        uint16_t copy = (remaining > CTAPHID_CONT_DATA) ? CTAPHID_CONT_DATA : remaining;
        memcpy(ch->buffer + ch->offset, packet + 5, copy);
        ch->offset += copy;
        ch->seq++;
        ch->last_activity = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // If message complete, process it
        if (ch->offset >= ch->bcnt) {
            process_complete_message(ch);
        }
    }

    xSemaphoreGive(g_ctaphid.mutex);
    return true;
}

/**
 * \brief Indicates whether a response is queued for host retrieval.
 * \return `true` when a response is pending, otherwise `false`.
 */
bool ctaphid_has_response(void) {
    return g_ctaphid.response_pending;
}

/**
 * \brief Retrieves the next response HID packet from the queued response message.
 * \param packet Destination buffer for the response packet.
 * \return `true` when a packet was written, otherwise `false`.
 */
bool ctaphid_get_response_packet(uint8_t *packet) {
    if (!g_ctaphid.response_pending || !packet) return false;

    if (ctap2_is_cancelled()) {
        xSemaphoreTake(g_ctaphid.mutex, portMAX_DELAY);
        LOG_W(TAG, "Send aborted: CTAP2 cancelled by host (offset=%u/%u)",
              g_ctaphid.response_offset, g_ctaphid.response_len);
        g_ctaphid.response_pending = false;
        g_ctaphid.response_offset = 0;
        g_ctaphid.response_len = 0;
        xSemaphoreGive(g_ctaphid.mutex);
        return false;
    }

    xSemaphoreTake(g_ctaphid.mutex, portMAX_DELAY);

    uint16_t remaining = g_ctaphid.response_len - g_ctaphid.response_offset;

    if (g_ctaphid.response_offset == 0) {
        // Send init packet
        build_init_packet(packet, g_ctaphid.response_cid, g_ctaphid.response_cmd,
                          g_ctaphid.response_len,
                          s_response_buffer, remaining);
        uint16_t sent = (remaining > CTAPHID_INIT_DATA) ? CTAPHID_INIT_DATA : remaining;
        g_ctaphid.response_offset = sent;
    } else {
        // Send continuation packet
        uint8_t seq = (g_ctaphid.response_offset - CTAPHID_INIT_DATA) / CTAPHID_CONT_DATA;
        build_cont_packet(packet, g_ctaphid.response_cid, seq,
                          s_response_buffer + g_ctaphid.response_offset,
                          remaining);
        uint16_t sent = (remaining > CTAPHID_CONT_DATA) ? CTAPHID_CONT_DATA : remaining;
        g_ctaphid.response_offset += sent;
    }

    // Check if response complete
    if (g_ctaphid.response_offset >= g_ctaphid.response_len) {
        g_ctaphid.response_pending = false;
    }

    xSemaphoreGive(g_ctaphid.mutex);
    return true;
}

/**
 * \brief Sends a CTAPHID KEEPALIVE packet immediately over USB.
 * \param cid Channel identifier.
 * \param status CTAPHID keepalive status byte.
 */
void ctaphid_send_keepalive(uint32_t cid, uint8_t status) {
    uint8_t packet[CTAPHID_PACKET_SIZE];
    uint8_t data = status;
    build_init_packet(packet, cid, CTAPHID_KEEPALIVE, 1, &data, 1);

    // Send directly via USB
    fido2_usb_write(packet);
}

/**
 * \brief Queues a CTAPHID ERROR response for the given channel.
 * \param cid Channel identifier.
 * \param error CTAPHID error code.
 */
void ctaphid_send_error(uint32_t cid, uint8_t error) {
    uint8_t data = error;
    prepare_response(cid, CTAPHID_ERROR, &data, 1);
    LOG_W(TAG, "Sending error 0x%02X to CID 0x%08lX", error, cid);
}

/**
 * \brief Expires active channels whose message assembly timeout elapsed.
 */
void ctaphid_check_timeout(void) {
    if (!g_ctaphid.initialized) return;

    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

    xSemaphoreTake(g_ctaphid.mutex, portMAX_DELAY);

    for (int i = 0; i < CTAPHID_MAX_CHANNELS; i++) {
        ctaphid_channel_t *ch = &g_ctaphid.channels[i];
        if (ch->active && (now - ch->last_activity) > CTAPHID_MSG_TIMEOUT_MS) {
            LOG_W(TAG, "Channel 0x%08lX timed out", ch->cid);
            ctaphid_send_error(ch->cid, CTAPHID_ERR_MSG_TIMEOUT);
            ch->active = false;
        }
    }

    xSemaphoreGive(g_ctaphid.mutex);
}

/**
 * \brief Returns the channel identifier of the currently processed request.
 * \return Active request channel identifier.
 */
uint32_t ctaphid_get_current_cid(void) {
    return g_ctaphid.current_cid;
}

/**
 * \brief Reports whether any CTAPHID channel currently has an active transaction.
 * \return `true` when at least one channel is active, otherwise `false`.
 */
bool ctaphid_is_busy(void) {
    for (int i = 0; i < CTAPHID_MAX_CHANNELS; i++) {
        if (g_ctaphid.channels[i].active) return true;
    }
    return false;
}
