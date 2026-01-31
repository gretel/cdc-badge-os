#include "mod_totp/TotpStore.h"
#include "cdc_hal/ISecureElement.h"
#include "cdc_core/TropicStorage.h"
#include "cdc_log.h"
#include <mbedtls/md.h>
#include <cstring>
#include <ctime>
#include <memory>
#include <new>

static const char* TAG = "TOTP";

namespace cdc::mod_totp {

#pragma pack(push, 1)
struct TotpPayload {
    char issuer[TotpStore::ISSUER_LEN];
    uint8_t secret[TotpStore::SECRET_LEN];
    uint8_t secretLen;
    uint8_t digits;
    uint32_t period;
    uint8_t algorithm;
    uint8_t flags;
};
#pragma pack(pop)

static int base32CharValue(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a';
    if (c >= '2' && c <= '7') return c - '2' + 26;
    return -1;
}

static int base32Decode(const char* encoded, uint8_t* out, size_t outMax) {
    if (!encoded || !out) return -1;

    int buffer = 0;
    int bitsLeft = 0;
    size_t count = 0;

    for (const char* p = encoded; *p; ++p) {
        if (*p == '=' || *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
            continue;
        }
        int value = base32CharValue(*p);
        if (value < 0) {
            return -1;
        }
        buffer = (buffer << 5) | value;
        bitsLeft += 5;
        if (bitsLeft >= 8) {
            bitsLeft -= 8;
            if (count >= outMax) {
                return -1;
            }
            out[count++] = static_cast<uint8_t>((buffer >> bitsLeft) & 0xFF);
        }
    }

    return static_cast<int>(count);
}

static const uint32_t POWERS_10[] = {
    1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000
};

TotpStore& TotpStore::instance() {
    static TotpStore inst;
    return inst;
}

void TotpStore::setSlotRange(uint16_t start, uint16_t end, uint8_t moduleId) {
    if (start > end || start == 0 || end == 0) {
        hasSlotRange_ = false;
        rmemStart_ = 0;
        rmemEnd_ = 0;
        moduleId_ = 0;
        return;
    }
    hasSlotRange_ = true;
    rmemStart_ = start;
    rmemEnd_ = end;
    moduleId_ = moduleId;
}

uint16_t TotpStore::capacity() const {
    if (!hasSlotRange_) return 0;
    return static_cast<uint16_t>(rmemEnd_ - rmemStart_ + 1);
}

bool TotpStore::toPhysicalSlot(uint16_t logicalIndex, uint16_t* slotOut) const {
    if (!slotOut) return false;
    if (!hasSlotRange_) return false;
    uint32_t slot = static_cast<uint32_t>(rmemStart_) + logicalIndex;
    if (slot > rmemEnd_) return false;
    *slotOut = static_cast<uint16_t>(slot);
    return true;
}

bool TotpStore::toLogicalSlot(uint16_t slot, uint16_t* logicalIndexOut) const {
    if (!logicalIndexOut) return false;
    if (!hasSlotRange_) return false;
    if (slot < rmemStart_ || slot > rmemEnd_) return false;
    *logicalIndexOut = static_cast<uint16_t>(slot - rmemStart_);
    return true;
}

bool TotpStore::readAccount(uint16_t slot, TotpAccount* out) {
    if (!out) return false;
    if (!hasSlotRange_) return false;
    uint16_t physSlot = 0;
    if (!toPhysicalSlot(slot, &physSlot)) return false;

    auto* se = cdc::hal::getSecureElementInstance();
    if (!se) return false;

    cdc::hal::ISecureElement::RMemHeader header = {};
    uint8_t payloadBuf[sizeof(TotpPayload)] = {};
    uint16_t payloadLen = 0;

    auto res = se->rmemReadWithHeader(physSlot, &header, payloadBuf, sizeof(payloadBuf), &payloadLen);
    if (res != cdc::hal::SeResult::OK) {
        return false;
    }

    if (header.moduleId != moduleId_) {
        return false;
    }

    TotpPayload payload = {};
    memcpy(&payload, payloadBuf, sizeof(payload));

    memset(out, 0, sizeof(*out));
    strncpy(out->name, header.name, sizeof(out->name) - 1);
    strncpy(out->issuer, payload.issuer, sizeof(out->issuer) - 1);
    memcpy(out->secret, payload.secret, sizeof(out->secret));
    out->secretLen = payload.secretLen;
    out->digits = payload.digits ? payload.digits : DEFAULT_DIGITS;
    out->period = payload.period ? payload.period : DEFAULT_PERIOD;
    out->algorithm = payload.algorithm;
    out->flags = payload.flags;

    return true;
}

bool TotpStore::findFreeSlot(uint16_t* slotOut) {
    if (!slotOut) return false;
    if (!hasSlotRange_) return false;

    uint16_t cap = capacity();
    if (cap == 0) return false;
    auto used = std::unique_ptr<bool[]>(new (std::nothrow) bool[cap]);
    if (!used) return false;
    memset(used.get(), 0, cap * sizeof(bool));
    struct Ctx {
        bool* used;
        uint16_t base;
        uint16_t cap;
    } ctx = { used.get(), 0, cap };

    auto cb = [](uint16_t slot, const cdc::core::TropicStorage::CacheEntry&, void* user) {
        auto* c = static_cast<Ctx*>(user);
        if (slot < c->base) return;
        uint16_t idx = slot - c->base;
        if (idx < c->cap) {
            c->used[idx] = true;
        }
    };

    ctx.base = rmemStart_;
    cdc::core::TropicStorage::instance().forEachSlot(
        moduleId_, rmemStart_, rmemEnd_, cb, &ctx);

    for (uint16_t i = 0; i < cap; i++) {
        if (!used[i]) {
            uint16_t candidate = static_cast<uint16_t>(rmemStart_ + i);
            if (candidate <= rmemEnd_) {
                *slotOut = candidate;
                return true;
            }
            return false;
        }
    }

    return false;
}

bool TotpStore::addAccount(const char* name, const char* issuer, const char* secretBase32,
                           uint8_t digits, uint32_t period, uint8_t algorithm) {
    if (!name || !secretBase32) return false;
    if (!hasSlotRange_) return false;

    uint16_t slot = 0;
    if (!findFreeSlot(&slot)) {
        LOG_W(TAG, "No free TOTP slots");
        return false;
    }

    uint8_t secret[SECRET_LEN];
    int secretLen = base32Decode(secretBase32, secret, SECRET_LEN);
    if (secretLen <= 0) {
        LOG_E(TAG, "Invalid Base32 secret");
        return false;
    }

    TotpPayload payload = {};
    if (issuer) {
        strncpy(payload.issuer, issuer, sizeof(payload.issuer) - 1);
    }
    memcpy(payload.secret, secret, static_cast<size_t>(secretLen));
    payload.secretLen = static_cast<uint8_t>(secretLen);
    payload.digits = digits ? digits : DEFAULT_DIGITS;
    payload.period = period ? period : DEFAULT_PERIOD;
    payload.algorithm = algorithm;
    payload.flags = 0;

    auto* se = cdc::hal::getSecureElementInstance();
    if (!se) return false;

    auto res = se->rmemWriteWithHeader(
        slot,
        moduleId_,
        name,
        0,
        reinterpret_cast<const uint8_t*>(&payload),
        sizeof(payload)
    );

    if (res != cdc::hal::SeResult::OK) {
        LOG_E(TAG, "Failed to write slot %u", slot);
        return false;
    }

    cdc::core::TropicStorage::instance().writeSlot(moduleId_, slot, name, 0);

    return true;
}

bool TotpStore::updateAccount(uint16_t slot, const char* name, const char* issuer, const char* secretBase32,
                              uint8_t digits, uint32_t period, uint8_t algorithm) {
    if (!name || !secretBase32) return false;
    if (!hasSlotRange_) return false;
    uint16_t physSlot = 0;
    if (!toPhysicalSlot(slot, &physSlot)) return false;

    uint8_t secret[SECRET_LEN];
    int secretLen = base32Decode(secretBase32, secret, SECRET_LEN);
    if (secretLen <= 0) {
        LOG_E(TAG, "Invalid Base32 secret");
        return false;
    }

    TotpPayload payload = {};
    if (issuer) {
        strncpy(payload.issuer, issuer, sizeof(payload.issuer) - 1);
    }
    memcpy(payload.secret, secret, static_cast<size_t>(secretLen));
    payload.secretLen = static_cast<uint8_t>(secretLen);
    payload.digits = digits ? digits : DEFAULT_DIGITS;
    payload.period = period ? period : DEFAULT_PERIOD;
    payload.algorithm = algorithm;
    payload.flags = 0;

    auto* se = cdc::hal::getSecureElementInstance();
    if (!se) return false;

    auto res = se->rmemWriteWithHeader(
        physSlot,
        moduleId_,
        name,
        0,
        reinterpret_cast<const uint8_t*>(&payload),
        sizeof(payload)
    );

    if (res != cdc::hal::SeResult::OK) {
        LOG_E(TAG, "Failed to write slot %u", slot);
        return false;
    }

    cdc::core::TropicStorage::instance().writeSlot(moduleId_, physSlot, name, 0);

    return true;
}

bool TotpStore::deleteAccount(uint16_t slot) {
    if (!hasSlotRange_) return false;
    uint16_t physSlot = 0;
    if (!toPhysicalSlot(slot, &physSlot)) return false;
    auto* se = cdc::hal::getSecureElementInstance();
    if (!se) return false;

    auto res = se->rmemErase(physSlot);
    if (res != cdc::hal::SeResult::OK) {
        return false;
    }

    cdc::core::TropicStorage::instance().eraseSlot(moduleId_, physSlot);

    return true;
}

uint32_t TotpStore::generate(const uint8_t* secret, size_t secretLen, time_t timestamp,
                             uint32_t period, uint8_t digits, TotpAlgorithm algorithm) const {
    if (!secret || secretLen == 0 || secretLen > SECRET_LEN) {
        return 0;
    }

    if (digits < 6 || digits > 8) {
        digits = DEFAULT_DIGITS;
    }

    if (period == 0) {
        period = DEFAULT_PERIOD;
    }

    uint64_t counter = static_cast<uint64_t>(timestamp / period);

    uint8_t counterBytes[8];
    for (int i = 7; i >= 0; i--) {
        counterBytes[i] = static_cast<uint8_t>(counter & 0xFF);
        counter >>= 8;
    }

    uint8_t hmac[64] = {};
    size_t hmacLen = 0;
    if (!hmacCompute(algorithm, secret, secretLen, counterBytes, 8, hmac, &hmacLen)) {
        return 0;
    }

    int offset = hmac[hmacLen - 1] & 0x0F;
    uint32_t binary =
        ((hmac[offset] & 0x7F) << 24) |
        ((hmac[offset + 1] & 0xFF) << 16) |
        ((hmac[offset + 2] & 0xFF) << 8) |
        (hmac[offset + 3] & 0xFF);

    return binary % POWERS_10[digits];
}

bool TotpStore::hmacCompute(TotpAlgorithm algo, const uint8_t* key, size_t keyLen,
                            const uint8_t* data, size_t dataLen,
                            uint8_t* output, size_t* outputLen) const {
    mbedtls_md_type_t mdType;
    size_t expectedLen;

    switch (algo) {
        case TotpAlgorithm::SHA256:
            mdType = MBEDTLS_MD_SHA256;
            expectedLen = 32;
            break;
        case TotpAlgorithm::SHA512:
            mdType = MBEDTLS_MD_SHA512;
            expectedLen = 64;
            break;
        default:
            mdType = MBEDTLS_MD_SHA1;
            expectedLen = 20;
            break;
    }

    const mbedtls_md_info_t* mdInfo = mbedtls_md_info_from_type(mdType);
    if (!mdInfo) {
        return false;
    }

    int ret = mbedtls_md_hmac(mdInfo, key, keyLen, data, dataLen, output);
    if (ret != 0) {
        return false;
    }

    if (outputLen) {
        *outputLen = expectedLen;
    }
    return true;
}

int8_t TotpStore::generateCode(uint16_t slot, char* codeOut) {
    if (!codeOut) return -1;

    TotpAccount account = {};
    if (!readAccount(slot, &account)) {
        return -1;
    }

    if (!isTimeValid()) {
        strcpy(codeOut, "------");
        return -1;
    }

    uint32_t code = generate(account.secret, account.secretLen, time(nullptr),
                             account.period, account.digits,
                             static_cast<TotpAlgorithm>(account.algorithm));

    if (account.digits == 8) {
        snprintf(codeOut, 9, "%08lu", static_cast<unsigned long>(code));
    } else if (account.digits == 7) {
        snprintf(codeOut, 8, "%07lu", static_cast<unsigned long>(code));
    } else {
        snprintf(codeOut, 7, "%06lu", static_cast<unsigned long>(code));
    }

    return static_cast<int8_t>(timeRemaining(account.period));
}

uint8_t TotpStore::timeRemaining(uint32_t period) const {
    if (period == 0) period = DEFAULT_PERIOD;
    return static_cast<uint8_t>(period - (time(nullptr) % period));
}

bool TotpStore::isTimeValid() const {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_year >= 124; // 2024+
}

} // namespace cdc::mod_totp
