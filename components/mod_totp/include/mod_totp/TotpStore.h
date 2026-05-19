#pragma once

#include "cdc_core/IModule.h"
#include "cdc_core/SlotManager.h"
#include <cstdint>
#include <cstddef>
#include <ctime>

namespace cdc::mod_totp {

enum class TotpAlgorithm : uint8_t {
    SHA1 = 0,
    SHA256 = 1,
    SHA512 = 2
};

struct TotpAccount {
    char name[16 + 1];
    char issuer[32 + 1];
    uint8_t secret[32];
    uint8_t secretLen;
    uint8_t digits;
    uint32_t period;
    uint8_t algorithm;
    uint8_t flags;
};

class TotpStore {
public:
    static constexpr uint8_t NAME_LEN = 16;
    static constexpr uint8_t ISSUER_LEN = 32;
    static constexpr uint8_t SECRET_LEN = 32;
    static constexpr uint8_t DEFAULT_DIGITS = 6;
    static constexpr uint32_t DEFAULT_PERIOD = 30;

    bool readAccount(uint16_t slot, TotpAccount* out);
    bool addAccount(const char* name, const char* issuer, const char* secretBase32,
                    uint8_t digits, uint32_t period, uint8_t algorithm);
    bool updateAccount(uint16_t slot, const char* name, const char* issuer, const char* secretBase32,
                       uint8_t digits, uint32_t period, uint8_t algorithm);
    bool deleteAccount(uint16_t slot);

    /**
     * \brief Renders the current TOTP code or a placeholder into \p codeOut.
     * \param slot Logical slot index.
     * \param codeOut Output buffer (must hold at least 9 bytes for 8-digit codes).
     * \param codeOutLen Size of \p codeOut in bytes.
     * \return Remaining seconds in the current step, or `-1` on failure.
     */
    int8_t generateCode(uint16_t slot, char* codeOut, size_t codeOutLen);

    bool isTimeValid() const;
    uint8_t timeRemaining(uint32_t period) const;

    static TotpStore& instance();

    void setSlotRange(const cdc::core::IModule::SlotRange& range);
    uint16_t capacity() const { return slots_.capacity(); }
    bool toPhysicalSlot(uint16_t logicalIndex, uint16_t* slotOut) const {
        return slots_.toPhysicalSlot(logicalIndex, slotOut);
    }
    bool toLogicalSlot(uint16_t slot, uint16_t* logicalIndexOut) const {
        return slots_.toLogicalSlot(slot, logicalIndexOut);
    }
    bool hasSlotRange() const { return slots_.hasSlotRange(); }
    uint8_t moduleId() const { return slots_.moduleId(); }
    uint16_t rmemStart() const { return slots_.rmemStart(); }
    uint16_t rmemEnd() const { return slots_.rmemEnd(); }

private:
    TotpStore() = default;

    uint32_t generate(const uint8_t* secret, size_t secretLen, time_t timestamp,
                      uint32_t period, uint8_t digits, TotpAlgorithm algorithm) const;
    bool hmacCompute(TotpAlgorithm algo, const uint8_t* key, size_t keyLen,
                     const uint8_t* data, size_t dataLen,
                     uint8_t* output, size_t* outputLen) const;

    cdc::core::SlotManager slots_;
};

} // namespace cdc::mod_totp
