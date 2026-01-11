/*
 * USB CCID (Chip Card Interface Device) for CDC Badge
 *
 * Based on pico-openpgp (https://github.com/polhenarejos/pico-openpgp)
 * Original: Copyright (c) 2022 Pol Henarejos, AGPLv3
 */

#include "ccid.h"
#include "openpgp.h"
#include "apdu.h"
#include <string.h>
#include <esp_log.h>

static const char *TAG = "CCID";

// CCID Functional Descriptor (54 bytes)
// From OpenPGP 3.4.1 specification
const uint8_t CCID_DESCRIPTOR[] = {
    0x36,       // bLength: 54 bytes
    0x21,       // bDescriptorType: Functional Descriptor
    0x10, 0x01, // bcdCCID: CCID version 1.10
    0x00,       // bMaxSlotIndex: 1 slot (index 0)
    0x07,       // bVoltageSupport: 5V, 3V, 1.8V
    0x02, 0x00, 0x00, 0x00, // dwProtocols: T=1 only
    0xA0, 0x0F, 0x00, 0x00, // dwDefaultClock: 4000 kHz
    0xA0, 0x0F, 0x00, 0x00, // dwMaximumClock: 4000 kHz
    0x00,       // bNumClockSupported
    0xB0, 0x04, 0x00, 0x00, // dwDataRate: 1200 bps
    0xB0, 0x04, 0x00, 0x00, // dwMaxDataRate: 1200 bps
    0x00,       // bNumDataRatesSupported
    0xFE, 0x00, 0x00, 0x00, // dwMaxIFSD: 254 bytes
    0x00, 0x00, 0x00, 0x00, // dwSynchProtocols: none
    0x00, 0x00, 0x00, 0x00, // dwMechanical: none
    // dwFeatures:
    // - Auto ICC clock frequency change
    // - Auto baud rate change
    // - Auto parameter negotiation
    // - Short and Extended APDU level exchange
    0x42, 0x08, 0x04, 0x00,
    0x00, 0x08, 0x00, 0x00, // dwMaxCCIDMessageLength: 2048
    0xFF,       // bClassGetResponse: echo
    0xFF,       // bClassEnvelope: echo
    0x00, 0x00, // wLcdLayout: none
    0x00,       // bPINSupport: none
    0x01        // bMaxCCIDBusySlots: 1
};

const size_t CCID_DESCRIPTOR_LEN = sizeof(CCID_DESCRIPTOR);

// ATR (Answer To Reset) for CDC Badge OpenPGP card
// Format: TS T0 Historical bytes... TCK
static const uint8_t ATR[] = {
    0x3B,                   // TS: Direct convention
    0xDA,                   // T0: TD1 follows, 10 historical bytes
    0x18,                   // TD1: T=1, TD2 follows
    0xFF,                   // TD2: TA3 follows
    0x81,                   // TA3: IFSC = 129
    0xB1,                   // Historical: Category indicator (0x80 | 0x31)
    0xFE,                   // Historical: TLV
    0x75,                   // Historical: Card issuer data length
    0x1F,                   // Historical: Card issuer data
    0x03,                   // Historical: Card issuer data
    // Historical bytes: "CDC" identifier
    'C', 'D', 'C',
    0x00,                   // Historical: Status indicator
    0x90, 0x00,             // Historical: SW1-SW2 (OK)
    0x00                    // TCK: Check character (XOR of all bytes after TS)
};

static bool initialized = false;
static uint8_t current_slot = 0;
static uint8_t current_seq = 0;

bool ccid_init(void) {
    if (!openpgp_init()) {
        ESP_LOGE(TAG, "Failed to initialize OpenPGP");
        return false;
    }

    initialized = true;
    ESP_LOGI(TAG, "CCID initialized");
    return true;
}

const uint8_t* ccid_get_atr(size_t *len) {
    if (len) {
        *len = sizeof(ATR);
    }
    return ATR;
}

bool ccid_card_present(void) {
    return initialized;
}

// Build CCID response header
static void ccid_build_header(uint8_t *resp, uint8_t msg_type, uint32_t data_len,
                              uint8_t slot, uint8_t seq, uint8_t status, uint8_t error) {
    resp[0] = msg_type;
    resp[1] = data_len & 0xFF;
    resp[2] = (data_len >> 8) & 0xFF;
    resp[3] = (data_len >> 16) & 0xFF;
    resp[4] = (data_len >> 24) & 0xFF;
    resp[5] = slot;
    resp[6] = seq;
    resp[7] = status;
    resp[8] = error;
    resp[9] = 0;  // Chain parameter
}

int ccid_process_message(const uint8_t *msg, size_t msg_len,
                         uint8_t *resp, size_t resp_max) {
    if (!msg || msg_len < CCID_HEADER_SIZE || !resp || resp_max < CCID_HEADER_SIZE) {
        return -1;
    }

    const ccid_header_t *hdr = (const ccid_header_t *)msg;
    current_slot = hdr->bSlot;
    current_seq = hdr->bSeq;

    uint8_t status = CCID_ICC_PRESENT_ACTIVE;
    uint8_t error = 0;

    switch (hdr->bMessageType) {
        case CCID_PC_TO_RDR_ICC_POWER_ON: {
            // Return ATR
            ESP_LOGI(TAG, "ICC Power On");
            size_t atr_len;
            const uint8_t *atr = ccid_get_atr(&atr_len);

            ccid_build_header(resp, CCID_RDR_TO_PC_DATA_BLOCK, atr_len,
                             current_slot, current_seq, status, error);
            memcpy(resp + CCID_HEADER_SIZE, atr, atr_len);
            return CCID_HEADER_SIZE + atr_len;
        }

        case CCID_PC_TO_RDR_ICC_POWER_OFF: {
            ESP_LOGI(TAG, "ICC Power Off");
            status = CCID_ICC_PRESENT_INACTIVE;
            ccid_build_header(resp, CCID_RDR_TO_PC_SLOT_STATUS, 0,
                             current_slot, current_seq, status, error);
            return CCID_HEADER_SIZE;
        }

        case CCID_PC_TO_RDR_GET_SLOT_STATUS: {
            ccid_build_header(resp, CCID_RDR_TO_PC_SLOT_STATUS, 0,
                             current_slot, current_seq, status, error);
            return CCID_HEADER_SIZE;
        }

        case CCID_PC_TO_RDR_XFR_BLOCK: {
            // APDU exchange
            uint32_t apdu_len = hdr->dwLength;
            if (msg_len < CCID_HEADER_SIZE + apdu_len) {
                error = CCID_ERROR_XFR_OVERRUN;
                status = CCID_CMD_STATUS_FAILED;
                ccid_build_header(resp, CCID_RDR_TO_PC_DATA_BLOCK, 0,
                                 current_slot, current_seq, status, error);
                return CCID_HEADER_SIZE;
            }

            const uint8_t *apdu_data = msg + CCID_HEADER_SIZE;

            // Process APDU through OpenPGP application
            uint8_t *resp_data = resp + CCID_HEADER_SIZE;
            size_t resp_data_max = resp_max - CCID_HEADER_SIZE;

            int resp_len = openpgp_process_apdu(apdu_data, apdu_len,
                                                resp_data, resp_data_max);
            if (resp_len < 0) {
                error = CCID_ERROR_HW_ERROR;
                status = CCID_CMD_STATUS_FAILED;
                ccid_build_header(resp, CCID_RDR_TO_PC_DATA_BLOCK, 0,
                                 current_slot, current_seq, status, error);
                return CCID_HEADER_SIZE;
            }

            ccid_build_header(resp, CCID_RDR_TO_PC_DATA_BLOCK, resp_len,
                             current_slot, current_seq, status, error);
            return CCID_HEADER_SIZE + resp_len;
        }

        case CCID_PC_TO_RDR_GET_PARAMETERS:
        case CCID_PC_TO_RDR_RESET_PARAMETERS: {
            // Return default T=1 parameters
            uint8_t params[] = {
                0x01,  // bmFindexDindex
                0x00,  // bmTCCKST1
                0x00,  // bGuardTimeT1
                0xFE,  // bmWaitingIntegersT1 (BWI=15, CWI=14)
                0x00,  // bClockStop
                0xFE,  // bIFSC
                0x00   // bNadValue
            };
            ccid_build_header(resp, CCID_RDR_TO_PC_PARAMETERS, sizeof(params),
                             current_slot, current_seq, status, error);
            resp[9] = 0x01;  // Protocol T=1
            memcpy(resp + CCID_HEADER_SIZE, params, sizeof(params));
            return CCID_HEADER_SIZE + sizeof(params);
        }

        default:
            ESP_LOGW(TAG, "Unknown CCID message type: 0x%02X", hdr->bMessageType);
            error = CCID_ERROR_CMD_NOT_SUPPORTED;
            status = CCID_CMD_STATUS_FAILED;
            ccid_build_header(resp, CCID_RDR_TO_PC_SLOT_STATUS, 0,
                             current_slot, current_seq, status, error);
            return CCID_HEADER_SIZE;
    }
}
