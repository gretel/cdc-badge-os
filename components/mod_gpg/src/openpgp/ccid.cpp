/**
 * \brief USB CCID (Chip Card Interface Device) transport for OpenPGP applet.
 *
 * Based on pico-openpgp (https://github.com/polhenarejos/pico-openpgp).
 * Original project copyright: Pol Henarejos, AGPLv3.
 */

#include "mod_gpg/openpgp/ccid.h"
#include "mod_gpg/openpgp/openpgp.h"
#include "mod_gpg/openpgp/apdu.h"
#include "cdc_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "CCID";

/**
 * \brief CCID functional descriptor (54 bytes) per OpenPGP 3.4.1 profile.
 */
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

/**
 * \brief ATR (Answer To Reset) for CDC Badge OpenPGP card.
 *
 * T=1 protocol layout, OpenPGP 3.x compatible.
 */
static const uint8_t ATR[] = {
    0x3B,                   // TS: Direct convention
    0xDA,                   // T0: Y1=D (TA1,TC1,TD1 present), K=10 historical bytes
    0x18,                   // TA1: FI=1, DI=8
    0xFF,                   // TC1: N=255 (guard time)
    0x81,                   // TD1: Y2=8 (TD2 present), T=1
    0xB1,                   // TD2: Y3=B (TA3,TB3,TD3 present), T=1
    0xFE,                   // TA3: IFSC=254
    0x75,                   // TB3: BWI=7, CWI=5
    0x1F,                   // TD3: Y4=1 (TA4 present), T=15
    0x03,                   // TA4: Class indicator
    // 10 Historical bytes (OpenPGP format)
    0x00, 0x31, 0xF5, 0x73, 0xC0, 0x01, 0x60, 0x00, 0x90, 0x00,
    0x1C                    // TCK: checksum
};

static bool initialized = false;
static uint8_t current_slot = 0;
static uint8_t current_seq = 0;

/**
 * \brief Initializes CCID transport and backing OpenPGP applet.
 * \return `true` if initialization succeeded.
 */
extern "C" void ccid_driver_link_anchor(void);

bool ccid_init(void) {
    ccid_driver_link_anchor();

    if (!openpgp_init()) {
        LOG_E(TAG, "Failed to initialize OpenPGP");
        return false;
    }

    initialized = true;
    LOG_I(TAG, "CCID initialized");
    return true;
}

/**
 * \brief Returns pointer and length of ATR bytes.
 * \param len Optional output receiving ATR length.
 * \return Pointer to static ATR buffer.
 */
const uint8_t* ccid_get_atr(size_t *len) {
    if (len) {
        *len = sizeof(ATR);
    }
    return ATR;
}

/**
 * \brief Returns whether virtual CCID card is available.
 * \return `true` if CCID/OpenPGP stack is initialized.
 */
bool ccid_card_present(void) {
    return initialized;
}

/**
 * \brief Builds a CCID response header in transport byte format.
 * \param resp Output response buffer.
 * \param msg_type CCID response message type.
 * \param data_len Payload length in bytes.
 * \param slot Slot index.
 * \param seq Sequence number.
 * \param status CCID status flags.
 * \param error CCID error code.
 */
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

/**
 * \brief Processes one incoming CCID message and writes corresponding response.
 * \param msg Incoming CCID message bytes.
 * \param msg_len Incoming message length.
 * \param resp Output response buffer.
 * \param resp_max Output buffer capacity.
 * \return Response length in bytes, or negative value on fatal parameter errors.
 */
int ccid_process_message(const uint8_t *msg, size_t msg_len,
                         uint8_t *resp, size_t resp_max) {
    if (!msg || msg_len < CCID_HEADER_SIZE || !resp || resp_max < CCID_HEADER_SIZE) {
        LOG_E(TAG, "Invalid parameters: msg_len=%zu resp_max=%zu", msg_len, resp_max);
        return -1;
    }

    const ccid_header_t *hdr = (const ccid_header_t *)msg;
    current_slot = hdr->bSlot;
    current_seq = hdr->bSeq;

    uint8_t status = CCID_ICC_PRESENT_ACTIVE;
    uint8_t error = 0;
    int result_len = 0;

    switch (hdr->bMessageType) {
        case CCID_PC_TO_RDR_ICC_POWER_ON: {
            size_t atr_len;
            const uint8_t *atr = ccid_get_atr(&atr_len);
            ccid_build_header(resp, CCID_RDR_TO_PC_DATA_BLOCK, atr_len,
                             current_slot, current_seq, status, error);
            memcpy(resp + CCID_HEADER_SIZE, atr, atr_len);
            result_len = CCID_HEADER_SIZE + atr_len;
            break;
        }

        case CCID_PC_TO_RDR_ICC_POWER_OFF: {
            status = CCID_ICC_PRESENT_INACTIVE;
            ccid_build_header(resp, CCID_RDR_TO_PC_SLOT_STATUS, 0,
                             current_slot, current_seq, status, error);
            result_len = CCID_HEADER_SIZE;
            break;
        }

        case CCID_PC_TO_RDR_GET_SLOT_STATUS: {
            ccid_build_header(resp, CCID_RDR_TO_PC_SLOT_STATUS, 0,
                             current_slot, current_seq, status, error);
            result_len = CCID_HEADER_SIZE;
            break;
        }

        case CCID_PC_TO_RDR_XFR_BLOCK: {
            uint32_t apdu_len = hdr->dwLength;

            // Subtractive comparison prevents integer overflow when dwLength is
            // attacker-controlled.
            if (msg_len < CCID_HEADER_SIZE ||
                apdu_len > (uint32_t)(msg_len - CCID_HEADER_SIZE) ||
                apdu_len > (uint32_t)(resp_max - CCID_HEADER_SIZE)) {
                LOG_E(TAG, "XFR length invalid (apdu=%u msg=%zu rmax=%zu)",
                      apdu_len, msg_len, resp_max);
                error = CCID_ERROR_XFR_OVERRUN;
                status = CCID_CMD_STATUS_FAILED;
                ccid_build_header(resp, CCID_RDR_TO_PC_DATA_BLOCK, 0,
                                 current_slot, current_seq, status, error);
                result_len = CCID_HEADER_SIZE;
                break;
            }

            const uint8_t *apdu_data = msg + CCID_HEADER_SIZE;
            uint8_t *resp_data = resp + CCID_HEADER_SIZE;
            size_t resp_data_max = resp_max - CCID_HEADER_SIZE;

            int resp_len = openpgp_process_apdu(apdu_data, apdu_len,
                                                resp_data, resp_data_max);

            if (resp_len < 0) {
                error = CCID_ERROR_HW_ERROR;
                status = CCID_CMD_STATUS_FAILED;
                ccid_build_header(resp, CCID_RDR_TO_PC_DATA_BLOCK, 0,
                                 current_slot, current_seq, status, error);
                result_len = CCID_HEADER_SIZE;
            } else {
                ccid_build_header(resp, CCID_RDR_TO_PC_DATA_BLOCK, resp_len,
                                 current_slot, current_seq, status, error);
                result_len = CCID_HEADER_SIZE + resp_len;
            }

            // One-line summary per APDU. INS = byte 1 of the APDU, SW = last two response bytes.
            uint16_t sw = (resp_len >= 2)
                ? ((uint16_t)resp_data[resp_len - 2] << 8) | resp_data[resp_len - 1]
                : 0;
            LOG_I(TAG, "XFR seq=%u INS=0x%02X in=%uB out=%dB sw=%04X",
                  hdr->bSeq, apdu_len >= 2 ? apdu_data[1] : 0,
                  apdu_len, resp_len, sw);
            break;
        }

        case CCID_PC_TO_RDR_GET_PARAMETERS: {
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
            resp[9] = 0x01;  // bProtocolNum: T=1
            memcpy(resp + CCID_HEADER_SIZE, params, sizeof(params));
            result_len = CCID_HEADER_SIZE + sizeof(params);
            break;
        }

        case CCID_PC_TO_RDR_RESET_PARAMETERS: {
            uint8_t params[] = {
                0x01, 0x00, 0x00, 0xFE, 0x00, 0xFE, 0x00
            };
            ccid_build_header(resp, CCID_RDR_TO_PC_PARAMETERS, sizeof(params),
                             current_slot, current_seq, status, error);
            resp[9] = 0x01;  // bProtocolNum: T=1
            memcpy(resp + CCID_HEADER_SIZE, params, sizeof(params));
            result_len = CCID_HEADER_SIZE + sizeof(params);
            break;
        }

        default:
            LOG_W(TAG, "Unknown CCID type 0x%02X seq=%u", hdr->bMessageType, hdr->bSeq);
            error = CCID_ERROR_CMD_NOT_SUPPORTED;
            status = CCID_CMD_STATUS_FAILED;
            ccid_build_header(resp, CCID_RDR_TO_PC_SLOT_STATUS, 0,
                             current_slot, current_seq, status, error);
            result_len = CCID_HEADER_SIZE;
            break;
    }

    return result_len;
}
