#pragma once

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

    int8_t generateCode(uint16_t slot, char* codeOut);

    bool isTimeValid() const;
    uint8_t timeRemaining(uint32_t period) const;

    static TotpStore& instance();

    void setSlotRange(uint16_t start, uint16_t end, uint8_t moduleId);
    uint16_t capacity() const;
    bool toPhysicalSlot(uint16_t logicalIndex, uint16_t* slotOut) const;
    bool toLogicalSlot(uint16_t slot, uint16_t* logicalIndexOut) const;
    bool hasSlotRange() const { return hasSlotRange_; }
    uint8_t moduleId() const { return moduleId_; }
    uint16_t rmemStart() const { return rmemStart_; }
    uint16_t rmemEnd() const { return rmemEnd_; }

private:
    TotpStore() = default;

    bool findFreeSlot(uint16_t* slotOut);
    uint32_t generate(const uint8_t* secret, size_t secretLen, time_t timestamp,
                      uint32_t period, uint8_t digits, TotpAlgorithm algorithm) const;
    bool hmacCompute(TotpAlgorithm algo, const uint8_t* key, size_t keyLen,
                     const uint8_t* data, size_t dataLen,
                     uint8_t* output, size_t* outputLen) const;

    bool hasSlotRange_ = false;
    uint16_t rmemStart_ = 0;
    uint16_t rmemEnd_ = 0;
    uint8_t moduleId_ = 0;
};

} // namespace cdc::mod_totp
