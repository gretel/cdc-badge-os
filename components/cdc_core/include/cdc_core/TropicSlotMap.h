#pragma once

#include <cstdint>

namespace cdc::core {

class TropicSlotMap {
public:
    enum class SlotType : uint8_t {
        ECC,
        RMEM
    };

    struct SlotRange {
        bool valid = false;
        SlotType type = SlotType::ECC;
        const char* moduleName = nullptr;
        uint8_t moduleId = 0;
        uint16_t start = 0;
        uint16_t end = 0;
    };

    static TropicSlotMap& instance();

    bool isValid() const { return valid_; }
    const char* errorMessage() const { return errorMessage_; }

    bool getRangeByName(const char* moduleName, SlotType type, SlotRange* out) const;
    bool getRangeByModuleId(uint8_t moduleId, SlotType type, SlotRange* out) const;
    bool isRmemAllowedForModuleId(uint16_t slot, uint8_t moduleId) const;

    using RangeCallback = void (*)(const SlotRange& range, void* user);
    void forEachRange(SlotType type, RangeCallback cb, void* user) const;

    uint16_t rmemMax() const;

    uint32_t computeMapSignature() const;

private:
    TropicSlotMap();

    void validateOnce();
    void setError(const char* message);

    bool valid_ = true;
    const char* errorMessage_ = nullptr;
};

} // namespace cdc::core
