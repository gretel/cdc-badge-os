#include "mod_homeassistant/HaStorage.h"
#include "mod_homeassistant/HomeAssistantModule.h"
#include "cdc_hal/ISecureElement.h"
#include "cdc_log.h"
#include "mbedtls/md.h"
#include <cstring>
#include <cstdio>

static const char* TAG = "HA_TOKEN";

namespace cdc::mod_homeassistant {
namespace HaTokenStorage {

/**
 * \brief Resolves the R-Memory slot used for the token.
 * \param slotOut Receives the slot number on success.
 * \param moduleIdOut Receives the assigned module id.
 * \return `true` if a slot is allocated via the module's SlotRange.
 */
static bool resolveSlot(uint16_t* slotOut, uint8_t* moduleIdOut) {
    const auto& range = HomeAssistantModule::instance().slotRange();
    if (!range.hasRmem) return false;
    if (slotOut)     *slotOut     = range.rmemStart;
    if (moduleIdOut) *moduleIdOut = range.moduleId;
    return true;
}

bool isPresent() {
    auto* se = hal::getSecureElementInstance();
    if (!se) return false;
    uint16_t slot = 0;
    if (!resolveSlot(&slot, nullptr)) return false;
    hal::ISecureElement::RMemHeader hdr = {};
    uint16_t payloadLen = 0;
    auto res = se->rmemReadWithHeader(slot, &hdr, nullptr, 0, &payloadLen);
    return res == hal::SeResult::OK && payloadLen > 0;
}

bool read(char* out, size_t maxLen) {
    if (!out || maxLen == 0) return false;
    out[0] = '\0';

    auto* se = hal::getSecureElementInstance();
    if (!se) return false;
    uint16_t slot = 0;
    if (!resolveSlot(&slot, nullptr)) return false;

    hal::ISecureElement::RMemHeader hdr = {};
    uint8_t buf[MAX_LEN + 1] = {};
    uint16_t payloadLen = 0;
    auto res = se->rmemReadWithHeader(slot, &hdr, buf, sizeof(buf) - 1, &payloadLen);
    if (res != hal::SeResult::OK) {
        LOG_W(TAG, "rmemReadWithHeader failed (res=%d)", static_cast<int>(res));
        return false;
    }
    if (payloadLen == 0 || payloadLen >= maxLen) {
        return false;
    }
    buf[payloadLen] = '\0';
    strncpy(out, reinterpret_cast<const char*>(buf), maxLen - 1);
    out[maxLen - 1] = '\0';
    return true;
}

bool write(const char* token) {
    if (!token) return false;
    size_t len = strlen(token);
    if (len < 32 || len > MAX_LEN) {
        LOG_E(TAG, "Token length out of range (%u)", static_cast<unsigned>(len));
        return false;
    }

    auto* se = hal::getSecureElementInstance();
    if (!se) return false;
    uint16_t slot = 0;
    uint8_t  moduleId = 0;
    if (!resolveSlot(&slot, &moduleId)) return false;

    auto res = se->rmemWriteWithHeader(
        slot, moduleId, "HA_TOKEN", /*flags=*/0,
        reinterpret_cast<const uint8_t*>(token), static_cast<uint16_t>(len));
    if (res != hal::SeResult::OK) {
        LOG_E(TAG, "rmemWriteWithHeader failed (res=%d)", static_cast<int>(res));
        return false;
    }
    return true;
}

bool clear() {
    auto* se = hal::getSecureElementInstance();
    if (!se) return false;
    uint16_t slot = 0;
    if (!resolveSlot(&slot, nullptr)) return false;
    return se->rmemErase(slot) == hal::SeResult::OK;
}

bool fingerprint(char* outHex16, size_t outSize) {
    if (!outHex16 || outSize < 17) return false;
    outHex16[0] = '\0';

    char token[MAX_LEN + 1] = {};
    if (!read(token, sizeof(token))) return false;

    uint8_t digest[32] = {};
    auto* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!info) return false;
    if (mbedtls_md(info,
                   reinterpret_cast<const uint8_t*>(token),
                   strlen(token),
                   digest) != 0) {
        return false;
    }

    // First 8 bytes as 16 hex chars.
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 8; i++) {
        outHex16[i * 2]     = hex[(digest[i] >> 4) & 0x0F];
        outHex16[i * 2 + 1] = hex[digest[i] & 0x0F];
    }
    outHex16[16] = '\0';
    return true;
}

} // namespace HaTokenStorage
} // namespace cdc::mod_homeassistant
