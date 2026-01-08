// CTAPHID Transport Layer
// Handles USB HID framing for CTAP2 messages

#include "ctaphid.h"
#include "ctap2.h"
#include "u2f.h"
#include "cdc_log.h"
#include "usb_hid.h"
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// ============================================================================
// Configuration
// ============================================================================

#define CTAPHID_DEBUG_PACKETS       true    // Enable verbose packet logging
#define CTAPHID_MSG_TIMEOUT_MS      500     // Message assembly timeout
#define CTAPHID_MAX_CHANNELS        8       // Max concurrent channels
#define CTAPHID_RESPONSE_QUEUE_SIZE 8       // Response packet queue

// ============================================================================
// State
// ============================================================================

// Rate limiting: max commands per second
#define CTAPHID_RATE_LIMIT_WINDOW_MS  1000
#define CTAPHID_RATE_LIMIT_MAX_CMDS   200

static struct {
    bool initialized;
    uint32_t next_cid;                      // Next channel ID to allocate
    ctaphid_channel_t channels[CTAPHID_MAX_CHANNELS];
    uint8_t msg_buffers[CTAPHID_MAX_CHANNELS][CTAPHID_MAX_MSG_SIZE];  // Per-channel buffers
    uint8_t init_buffer[64];                // Small buffer for INIT command only
    uint8_t response_buffer[CTAPHID_MAX_MSG_SIZE];
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

// ============================================================================
// Internal Helpers
// ============================================================================

static ctaphid_channel_t *find_channel(uint32_t cid) {
    for (int i = 0; i < CTAPHID_MAX_CHANNELS; i++) {
        if (g_ctaphid.channels[i].cid == cid) {
            return &g_ctaphid.channels[i];
        }
    }
    return NULL;
}

static ctaphid_channel_t *alloc_channel(uint32_t cid) {
    // First check if already exists
    ctaphid_channel_t *ch = find_channel(cid);
    if (ch) return ch;

    // Find free slot
    for (int i = 0; i < CTAPHID_MAX_CHANNELS; i++) {
        if (g_ctaphid.channels[i].cid == 0) {
            g_ctaphid.channels[i].cid = cid;
            g_ctaphid.channels[i].active = false;
            g_ctaphid.channels[i].buffer = g_ctaphid.msg_buffers[i];  // Per-channel buffer
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
        LOG_W("CTAPHID", "Evicting oldest channel 0x%08lX", g_ctaphid.channels[oldest_idx].cid);
        g_ctaphid.channels[oldest_idx].cid = cid;
        g_ctaphid.channels[oldest_idx].active = false;
        g_ctaphid.channels[oldest_idx].buffer = g_ctaphid.msg_buffers[oldest_idx];
        g_ctaphid.channels[oldest_idx].buffer_size = CTAPHID_MAX_MSG_SIZE;
        return &g_ctaphid.channels[oldest_idx];
    }

    return NULL;  // All channels actively in use
}

static uint32_t allocate_cid(void) {
    // Start from 1, avoid broadcast CID
    if (g_ctaphid.next_cid == 0 || g_ctaphid.next_cid == CTAPHID_BROADCAST_CID) {
        g_ctaphid.next_cid = 1;
    }
    return g_ctaphid.next_cid++;
}

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

static void prepare_response(uint32_t cid, uint8_t cmd, const uint8_t *data, uint16_t len) {
    g_ctaphid.response_cid = cid;
    g_ctaphid.response_cmd = cmd;
    if (data && len > 0) {
        // Bounds check to prevent buffer overflow
        if (len > CTAPHID_MAX_MSG_SIZE) {
            LOG_E("CTAPHID", "Response too large: %u > %u", len, CTAPHID_MAX_MSG_SIZE);
            len = CTAPHID_MAX_MSG_SIZE;
        }
        memcpy(g_ctaphid.response_buffer, data, len);
    }
    g_ctaphid.response_len = len;
    g_ctaphid.response_offset = 0;
    g_ctaphid.response_pending = true;
    if (cmd == CTAPHID_CBOR && len > 0) {
        LOG_I("CTAPHID", "Prepared CBOR response len=%u status=0x%02X", len, data[0]);
    }
}

// ============================================================================
// Command Handlers
// ============================================================================

static void handle_init(uint32_t cid, const uint8_t *data, uint16_t len) {
    if (len < 8) {
        ctaphid_send_error(cid, CTAPHID_ERR_INVALID_LEN);
        return;
    }

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
    if (CTAPHID_DEBUG_PACKETS) LOG_D("CTAPHID", "INIT: allocated CID 0x%08lX", new_cid);
}

static void handle_ping(uint32_t cid, const uint8_t *data, uint16_t len) {
    // Echo back the data
    prepare_response(cid, CTAPHID_PING, data, len);
    if (CTAPHID_DEBUG_PACKETS) LOG_D("CTAPHID", "PING: echoing %d bytes", len);
}

static void handle_wink(uint32_t cid) {
    // TODO: Visual feedback (LED blink or similar)
    prepare_response(cid, CTAPHID_WINK, NULL, 0);
    if (CTAPHID_DEBUG_PACKETS) LOG_D("CTAPHID", "WINK");
}

static void handle_cancel(uint32_t cid) {
    ctap2_cancel();
    // No response for CANCEL
    if (CTAPHID_DEBUG_PACKETS) LOG_D("CTAPHID", "CANCEL");
}

static void handle_cbor(uint32_t cid, const uint8_t *data, uint16_t len) {
    if (len < 1) {
        ctaphid_send_error(cid, CTAPHID_ERR_INVALID_LEN);
        return;
    }

    g_ctaphid.current_cid = cid;

    // Process CTAP2 command
    uint16_t response_len = sizeof(g_ctaphid.response_buffer);
    uint8_t status = ctap2_process_command(data, len,
                                            g_ctaphid.response_buffer,
                                            &response_len);

    // Prepare response (status byte + data)
    // Note: ctap2_process_command already includes status in response_buffer[0]
    prepare_response(cid, CTAPHID_CBOR, g_ctaphid.response_buffer, response_len);
    if (CTAPHID_DEBUG_PACKETS) LOG_D("CTAPHID", "CBOR: cmd=0x%02X status=0x%02X len=%d", data[0], status, response_len);
}

static void process_complete_message(ctaphid_channel_t *ch) {
    uint32_t cid = ch->cid;
    uint8_t cmd = ch->cmd;
    uint8_t *data = ch->buffer;
    uint16_t len = ch->bcnt;

    if (CTAPHID_DEBUG_PACKETS) LOG_D("CTAPHID", "Processing cmd=0x%02X len=%d", cmd, len);

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
                uint8_t u2f_response[512];
                uint16_t u2f_response_len = u2f_process_apdu(data, len, u2f_response, sizeof(u2f_response));
                prepare_response(cid, CTAPHID_MSG, u2f_response, u2f_response_len);
                if (CTAPHID_DEBUG_PACKETS) LOG_D("CTAPHID", "U2F: response len=%d", u2f_response_len);
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

// ============================================================================
// Public API
// ============================================================================

bool ctaphid_init(void) {
    LOG_I("CTAPHID", "Initializing...");

    memset(&g_ctaphid, 0, sizeof(g_ctaphid));
    g_ctaphid.next_cid = 1;

    g_ctaphid.mutex = xSemaphoreCreateMutex();
    if (!g_ctaphid.mutex) {
        LOG_E("CTAPHID", "Mutex creation failed");
        return false;
    }

    g_ctaphid.initialized = true;
    LOG_I("CTAPHID", "Initialized");
    return true;
}

void ctaphid_get_cmd_counts(uint32_t *cbor_count, uint32_t *msg_count) {
    if (cbor_count) *cbor_count = g_ctaphid.cbor_cmd_count;
    if (msg_count) *msg_count = g_ctaphid.msg_cmd_count;
}

void ctaphid_reset_cmd_counts(void) {
    g_ctaphid.cbor_cmd_count = 0;
    g_ctaphid.msg_cmd_count = 0;
}

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
            LOG_W("CTAPHID", "Rate limit exceeded (%d cmd/s)", g_ctaphid.rate_cmd_count);
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

        if (CTAPHID_DEBUG_PACKETS) LOG_D("CTAPHID", "Init packet: CID=0x%08lX CMD=0x%02X BCNT=%d", cid, cmd, bcnt);

        // Validate bcnt to prevent buffer overflow
        if (bcnt > CTAPHID_MAX_MSG_SIZE) {
            LOG_W("CTAPHID", "bcnt exceeds max: %u > %u", bcnt, CTAPHID_MAX_MSG_SIZE);
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
            LOG_W("CTAPHID", "Unknown CID 0x%08lX", cid);
            ctaphid_send_error(cid, CTAPHID_ERR_INVALID_CHANNEL);
            xSemaphoreGive(g_ctaphid.mutex);
            return true;
        }

        // Check if another transaction is active on this channel
        if (ch->active) {
            LOG_W("CTAPHID", "Channel busy");
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
            LOG_W("CTAPHID", "Continuation for inactive channel");
            ctaphid_send_error(cid, CTAPHID_ERR_INVALID_CHANNEL);
            xSemaphoreGive(g_ctaphid.mutex);
            return true;
        }

        // Check sequence
        if (seq != ch->seq) {
            LOG_W("CTAPHID", "Invalid sequence: got %d, expected %d", seq, ch->seq);
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

bool ctaphid_has_response(void) {
    return g_ctaphid.response_pending;
}

bool ctaphid_get_response_packet(uint8_t *packet) {
    if (!g_ctaphid.response_pending || !packet) return false;

    xSemaphoreTake(g_ctaphid.mutex, portMAX_DELAY);

    uint16_t remaining = g_ctaphid.response_len - g_ctaphid.response_offset;

    if (g_ctaphid.response_offset == 0) {
        // Send init packet
        build_init_packet(packet, g_ctaphid.response_cid, g_ctaphid.response_cmd,
                          g_ctaphid.response_len,
                          g_ctaphid.response_buffer, remaining);
        uint16_t sent = (remaining > CTAPHID_INIT_DATA) ? CTAPHID_INIT_DATA : remaining;
        g_ctaphid.response_offset = sent;
    } else {
        // Send continuation packet
        uint8_t seq = (g_ctaphid.response_offset - CTAPHID_INIT_DATA) / CTAPHID_CONT_DATA;
        build_cont_packet(packet, g_ctaphid.response_cid, seq,
                          g_ctaphid.response_buffer + g_ctaphid.response_offset,
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

void ctaphid_send_keepalive(uint32_t cid, uint8_t status) {
    uint8_t packet[CTAPHID_PACKET_SIZE];
    uint8_t data = status;
    build_init_packet(packet, cid, CTAPHID_KEEPALIVE, 1, &data, 1);

    // Send directly via USB
    usb_fido::write(packet);
}

void ctaphid_send_error(uint32_t cid, uint8_t error) {
    uint8_t data = error;
    prepare_response(cid, CTAPHID_ERROR, &data, 1);
    LOG_W("CTAPHID", "Sending error 0x%02X to CID 0x%08lX", error, cid);
}

void ctaphid_check_timeout(void) {
    if (!g_ctaphid.initialized) return;

    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

    xSemaphoreTake(g_ctaphid.mutex, portMAX_DELAY);

    for (int i = 0; i < CTAPHID_MAX_CHANNELS; i++) {
        ctaphid_channel_t *ch = &g_ctaphid.channels[i];
        if (ch->active && (now - ch->last_activity) > CTAPHID_MSG_TIMEOUT_MS) {
            LOG_W("CTAPHID", "Channel 0x%08lX timed out", ch->cid);
            ctaphid_send_error(ch->cid, CTAPHID_ERR_MSG_TIMEOUT);
            ch->active = false;
        }
    }

    xSemaphoreGive(g_ctaphid.mutex);
}

uint32_t ctaphid_get_current_cid(void) {
    return g_ctaphid.current_cid;
}

bool ctaphid_is_busy(void) {
    for (int i = 0; i < CTAPHID_MAX_CHANNELS; i++) {
        if (g_ctaphid.channels[i].active) return true;
    }
    return false;
}
