/**
 * BLE Advertising Data Parser
 *
 * Parses raw AD structures per BLE Core Spec Vol 3, Part C, Section 11.
 * Each AD structure: [length][type][data...]
 * - length = size of type + data (does NOT include the length byte itself)
 * - type   = AD type identifier
 * - data   = type-specific payload
 */

#include "cdc_hal/IBluetoothController.h"
#include <cstring>

namespace cdc::hal::BleAdvParser {

/**
 * \brief AD type constants from BLE Core Spec Supplement, Part A.
 */
static constexpr uint8_t AD_TYPE_SHORTENED_NAME            = 0x08;
static constexpr uint8_t AD_TYPE_COMPLETE_NAME             = 0x09;
static constexpr uint8_t AD_TYPE_INCOMPLETE_UUID128        = 0x06;
static constexpr uint8_t AD_TYPE_COMPLETE_UUID128          = 0x07;
static constexpr uint8_t AD_TYPE_MANUFACTURER_SPECIFIC     = 0xFF;

static constexpr uint8_t UUID128_SIZE                      = 16;
static constexpr uint8_t COMPANY_ID_SIZE                   = 2;

/**
 * \brief Iterates over AD structures and invokes a callback for each structure.
 * \brief Stops early when the callback reports a match.
 *
 * \param advData Raw advertising data buffer.
 * \param len Total length of `advData`.
 * \param callback Callback receiving `(adType, adPayload, adPayloadLen)`.
 * \return `true` if the callback returned `true` for any structure, otherwise `false`.
 */
template <typename Callback>
static bool walkAdStructures(const uint8_t* advData, uint8_t len, Callback callback) {
    if (advData == nullptr || len == 0) {
        return false;
    }

    uint8_t offset = 0;
    while (offset < len) {
        const uint8_t adLength = advData[offset];

        // Length of 0 signals the end of significant AD structures
        if (adLength == 0) {
            break;
        }

        // The full structure occupies (1 + adLength) bytes.
        // Verify it fits within the remaining data.
        if (offset + 1 + adLength > len) {
            break;
        }

        const uint8_t adType       = advData[offset + 1];
        const uint8_t* adPayload   = &advData[offset + 2];
        const uint8_t adPayloadLen = adLength - 1; // exclude the type byte

        if (callback(adType, adPayload, adPayloadLen)) {
            return true;
        }

        offset += 1 + adLength;
    }

    return false;
}

/**
 * \brief Extracts manufacturer-specific AD payload and company identifier.
 * \param advData Raw advertising data buffer.
 * \param len Buffer length.
 * \param companyId Output company identifier.
 * \param data Output pointer to manufacturer payload bytes (after company ID).
 * \param dataLen Output payload length.
 * \return `true` if manufacturer data was found and parsed.
 */
bool findManufacturerData(const uint8_t* advData, uint8_t len,
                           uint16_t* companyId, const uint8_t** data, uint8_t* dataLen) {
    if (companyId == nullptr || data == nullptr || dataLen == nullptr) {
        return false;
    }

    return walkAdStructures(advData, len,
        [&](uint8_t adType, const uint8_t* payload, uint8_t payloadLen) -> bool {
            if (adType != AD_TYPE_MANUFACTURER_SPECIFIC) {
                return false;
            }

            // Manufacturer data must contain at least the 2-byte company ID
            if (payloadLen < COMPANY_ID_SIZE) {
                return false;
            }

            // Company ID is little-endian
            *companyId = static_cast<uint16_t>(payload[0]) |
                         (static_cast<uint16_t>(payload[1]) << 8);
            *data      = &payload[COMPANY_ID_SIZE];
            *dataLen   = payloadLen - COMPANY_ID_SIZE;

            return true;
        });
}

/**
 * \brief Checks whether a specific 128-bit service UUID is present in AD structures.
 * \param advData Raw advertising data buffer.
 * \param len Buffer length.
 * \param uuid128 Target 128-bit UUID bytes.
 * \return `true` if the UUID appears in an incomplete or complete UUID128 AD field.
 */
bool findServiceUuid128(const uint8_t* advData, uint8_t len,
                          const uint8_t uuid128[16]) {
    if (uuid128 == nullptr) {
        return false;
    }

    return walkAdStructures(advData, len,
        [&](uint8_t adType, const uint8_t* payload, uint8_t payloadLen) -> bool {
            if (adType != AD_TYPE_INCOMPLETE_UUID128 && adType != AD_TYPE_COMPLETE_UUID128) {
                return false;
            }

            // Payload contains one or more 16-byte UUIDs packed consecutively
            uint8_t remaining = payloadLen;
            const uint8_t* ptr = payload;
            while (remaining >= UUID128_SIZE) {
                if (std::memcmp(ptr, uuid128, UUID128_SIZE) == 0) {
                    return true;
                }
                ptr       += UUID128_SIZE;
                remaining -= UUID128_SIZE;
            }

            return false;
        });
}

/**
 * \brief Extracts local device name from advertising data.
 * \param advData Raw advertising data buffer.
 * \param len Buffer length.
 * \param name Output buffer for the parsed name.
 * \param nameMaxLen Size of `name`.
 * \return `true` if a complete or shortened name field was found.
 */
bool findName(const uint8_t* advData, uint8_t len,
                char* name, uint8_t nameMaxLen) {
    if (name == nullptr || nameMaxLen == 0) {
        return false;
    }

    // Ensure the output is always null-terminated even on failure
    name[0] = '\0';

    return walkAdStructures(advData, len,
        [&](uint8_t adType, const uint8_t* payload, uint8_t payloadLen) -> bool {
            if (adType != AD_TYPE_COMPLETE_NAME && adType != AD_TYPE_SHORTENED_NAME) {
                return false;
            }

            // Determine how many characters we can copy (reserve 1 byte for null)
            const uint8_t copyLen = (payloadLen < static_cast<uint8_t>(nameMaxLen - 1))
                                    ? payloadLen
                                    : static_cast<uint8_t>(nameMaxLen - 1);

            std::memcpy(name, payload, copyLen);
            name[copyLen] = '\0';

            return true;
        });
}

} // namespace cdc::hal::BleAdvParser
